# Retained native instances repeat BundleInstanceRun on the main thread,
# ordinary and custom pthread stacks, and an alternate ucontext stack. A second
# process lowers RLIMIT_STACK between retained calls to prove that a cached
# main-stack floor is not reused after the effective stack limit changes.
# Recursive exhaustion also runs on ordinary and custom pthread stacks.
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
library=${ZIRAN_LIB:-$repo/build/libziran.a}
scratch=${TMPDIR:-$repo/build}
case $scratch in
    /*) ;;
    *) scratch=$repo/$scratch ;;
esac
mkdir -p "$scratch"
work=$(mktemp -d "$scratch/zx-native-invocation.XXXXXX")
trap 'status=$?; if [ "$status" -ne 0 ]; then
        echo "retained native invocation regression artifacts:" >&2
        for file in "$work"/*.out "$work"/*.err; do
            [ -f "$file" ] || continue
            echo "--- ${file##*/} ---" >&2
            cat "$file" >&2
        done
    fi
    rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS

cat > "$work/zx_compare.zi" <<'ZI'
host_api :: #system_library "host_api";
Input :: (key: s64) -> s64 #foreign host_api;
#program_export
Answer :: () -> s64 {
    return Input(0) * 3 + 11
}
ZI
cat > "$work/depth.zi" <<'ZI'
host_api :: #system_library "host_api";
Input :: (key: s64) -> s64 #foreign host_api;
Depth :: (n: s64) -> s64 {
    if n == 0 { return 0 }
    return Depth(n - 1) + 1
}
#program_export
Answer :: () -> s64 {
    return Depth(Input(0))
}
ZI
"$ziran" bundle --root "$work" --entry zx_compare:Answer -o "$work/answer.zx" "$work/zx_compare.zi"
"$ziran" bundle --root "$work" --entry depth:Answer -o "$work/depth.zx" "$work/depth.zi"

cat > "$work/host.c" <<'C'
#define _GNU_SOURCE
#include <limits.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <ucontext.h>
#include "ziran_host.h"

enum {
    CUSTOM_STACK = 4 * 1024 * 1024,
    ALTERNATE_STACK = 2 * 1024 * 1024,
    MINIMUM_MAIN_STACK = 1024 * 1024
};

static int64_t input_value;

static int input(void *context, const char *module, const char *function,
                 const VmHostValue *args, int count, VmHostValue *value) {
    (void)context;
    if((strcmp(module, "zx_compare") != 0 && strcmp(module, "depth") != 0) ||
       strcmp(function, "Input") != 0 || count != 1 ||
       args[0].kind != VM_HOST_INTEGER || args[0].integer != 0)
        return 0;
    value->kind = VM_HOST_INTEGER;
    value->type = "s64";
    value->integer = input_value;
    return 1;
}

static int run_answer(BundleInstance *instance, int calls) {
    long long result = 0;
    int has_result = 0;
    for(int i = 0; i < calls; i++) {
        input_value++;
        if(!BundleInstanceRun(instance, &result, &has_result) || !has_result ||
           result != input_value * 3 + 11)
            return 0;
    }
    return 1;
}

static int exhaust_thread_stack(BundleInstance *instance) {
    long long result = 0;
    int has_result = 0;
    input_value = 0;
    if(!BundleInstanceRun(instance, &result, &has_result) || !has_result ||
       result != 0)
        return 0;
    input_value = 100000000;
    return !BundleInstanceRun(instance, &result, &has_result);
}

struct ThreadInput {
    BundleInstance *first;
    BundleInstance *second;
    int calls;
    int exhaust;
    int ok;
};

static void *answer_thread(void *opaque) {
    struct ThreadInput *input = opaque;
    input->ok = input->exhaust ?
        exhaust_thread_stack(input->first) &&
        exhaust_thread_stack(input->second) :
        run_answer(input->first, input->calls) &&
        run_answer(input->second, input->calls);
    return NULL;
}

static int run_on_new_pthread(BundleInstance *first, BundleInstance *second,
                              int calls, int exhaust) {
    pthread_t thread;
    struct ThreadInput input = {first, second, calls, exhaust, 0};
    if(pthread_create(&thread, NULL, answer_thread, &input) != 0 ||
       pthread_join(thread, NULL) != 0)
        return 0;
    return input.ok;
}

static int run_on_custom_pthread(BundleInstance *first,
                                 BundleInstance *second, int calls,
                                 int exhaust) {
    pthread_attr_t attributes;
    void *stack = NULL;
    int ok = 0;
    if(posix_memalign(&stack, 64, CUSTOM_STACK) == 0 &&
       pthread_attr_init(&attributes) == 0) {
        if(pthread_attr_setstack(&attributes, stack, CUSTOM_STACK) == 0) {
            pthread_t thread;
            struct ThreadInput input = {first, second, calls, exhaust, 0};
            int created = pthread_create(&thread, &attributes,
                                         answer_thread, &input) == 0;
            int joined = created && pthread_join(thread, NULL) == 0;
            ok = created && joined && input.ok;
        }
        pthread_attr_destroy(&attributes);
    }
    free(stack);
    return ok;
}

static ucontext_t alternate_context;
static ucontext_t return_context;
static BundleInstance *alternate_first;
static BundleInstance *alternate_second;
static int alternate_calls;
static int alternate_ok;

static void alternate_entry(void) {
    alternate_ok = run_answer(alternate_first, alternate_calls) &&
                   run_answer(alternate_second, alternate_calls);
}

static int run_on_alternate_stack(BundleInstance *first,
                                  BundleInstance *second, int calls) {
    void *stack = malloc(ALTERNATE_STACK);
    int ok = 0;
    if(stack != NULL && getcontext(&alternate_context) == 0) {
        alternate_first = first;
        alternate_second = second;
        alternate_calls = calls;
        alternate_ok = 0;
        alternate_context.uc_stack.ss_sp = stack;
        alternate_context.uc_stack.ss_size = ALTERNATE_STACK;
        alternate_context.uc_link = &return_context;
        makecontext(&alternate_context, alternate_entry, 0);
        int switched = swapcontext(&return_context, &alternate_context) == 0;
        ok = switched && alternate_ok;
    }
    free(stack);
    return ok;
}

static int exhaust_after_stack_limit_decrease(BundleInstance *instance) {
    struct rlimit original;
    struct rlimit reduced;
    long long result = 0;
    int has_result = 0;
    input_value = 0;
    if(getrlimit(RLIMIT_STACK, &original) != 0 ||
       !BundleInstanceRun(instance, &result, &has_result) || !has_result ||
       result != 0)
        return 0;
    reduced = original;
    if(original.rlim_cur == RLIM_INFINITY)
        reduced.rlim_cur = 2 * MINIMUM_MAIN_STACK;
    else if(original.rlim_cur / 2 >= MINIMUM_MAIN_STACK)
        reduced.rlim_cur = original.rlim_cur / 2;
    else if(original.rlim_cur > MINIMUM_MAIN_STACK)
        reduced.rlim_cur = MINIMUM_MAIN_STACK;
    else
        return 0;
    if(original.rlim_max != RLIM_INFINITY &&
       reduced.rlim_cur > original.rlim_max)
        return 0;
    if(setrlimit(RLIMIT_STACK, &reduced) != 0)
        return 0;
    input_value = 100000000;
    return !BundleInstanceRun(instance, &result, &has_result);
}

int main(int argc, char **argv) {
    if(argc != 4) return 2;
    int calls = atoi(argv[2]);
    int depth = strcmp(argv[1], "depth") == 0;
    int depth_threads = strcmp(argv[1], "depth-threads") == 0;
    Bundle *bundle = BundleOpen(argv[3]);
    HostBinding binding = {(depth || depth_threads) ? "depth" : "zx_compare",
                           "Input", input, NULL};
    BundleInstance *instance = bundle != NULL ?
        BundleInstantiate(bundle, &binding, 1) : NULL;
    int ok = instance != NULL;
    BundleInstance *second = NULL;
    BundleInstance *custom_first = NULL;
    BundleInstance *custom_second = NULL;
    if(ok && depth_threads) {
        second = BundleInstantiate(bundle, &binding, 1);
        int normal_ok = second != NULL &&
                        run_on_new_pthread(instance, second, 0, 1);
        custom_first = normal_ok ?
                       BundleInstantiate(bundle, &binding, 1) : NULL;
        custom_second = normal_ok ?
                        BundleInstantiate(bundle, &binding, 1) : NULL;
        int custom_ok = custom_first != NULL && custom_second != NULL &&
                        run_on_custom_pthread(custom_first, custom_second,
                                              0, 1);
        fprintf(stderr, "depth-threads normal=%d custom=%d\n",
                normal_ok, custom_ok);
        ok = custom_ok;
    } else if(ok && depth)
        ok = exhaust_after_stack_limit_decrease(instance);
    else if(ok) {
        second = BundleInstantiate(bundle, &binding, 1);
        ok = second != NULL &&
             run_answer(instance, calls) && run_answer(second, calls) &&
             run_on_new_pthread(instance, second, calls, 0) &&
             run_on_custom_pthread(instance, second, calls, 0) &&
             run_on_alternate_stack(instance, second, calls) &&
             run_answer(instance, calls) && run_answer(second, calls);
    }
    BundleInstanceClose(custom_second);
    BundleInstanceClose(custom_first);
    BundleInstanceClose(second);
    BundleInstanceClose(instance);
    BundleClose(bundle);
    puts(ok ? "ok" : "failed");
    return ok ? 0 : 1;
}
C

${CC:-cc} ${VM_CFLAGS:-} -O2 -std=c11 -I"$repo/include" -o "$work/host" "$work/host.c" \
    "$library" -lm -pthread -ldl

ZIRAN_NATIVE_CACHE=0 ZIRAN_NATIVE_TIMING=1 "$work/host" answer 1 "$work/answer.zx" \
    > "$work/engine.out" 2> "$work/engine.err"
if ! grep -q '^ziran: native' "$work/engine.err"; then
    echo "portable VM build: no retained native instance"
    exit 0
fi

"$work/host" answer 512 "$work/answer.zx" > "$work/answer.out" 2> "$work/answer.err"
test "$(cat "$work/answer.out")" = ok

ZIRAN_NATIVE_CACHE=0 "$work/host" depth 1 "$work/depth.zx" \
    > "$work/depth.out" 2> "$work/depth.err"
test "$(cat "$work/depth.out")" = ok
grep -q 'filled the stack' "$work/depth.err"

ZIRAN_NATIVE_CACHE=0 "$work/host" depth-threads 1 "$work/depth.zx" \
    > "$work/depth-threads.out" 2> "$work/depth-threads.err"
test "$(cat "$work/depth-threads.out")" = ok
grep -q 'filled the stack' "$work/depth-threads.err"
echo "retained native calls reuse per-thread stack floors and keep exhaustion guards"
