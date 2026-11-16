/* Native ZX through the embedding API: backend policy, shared code for one
 * file's content, fuel and stack traps that end only their instance,
 * independent globals, and host strings that outlive later host calls. */
#include "ziran_host.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#if defined(__linux__)
#include <pthread.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
#include <ucontext.h>
#endif
#include <string.h>

static char directory[4096];

static Bundle *
open_entry(const char *entry)
{
    char path[4200];
    snprintf(path, sizeof(path), "%s/%s.zx", directory, entry);
    Bundle *bundle = BundleOpen(path);
    assert(bundle != NULL);
    return bundle;
}

static BundleInstance *
instantiate(const Bundle *bundle, BundleBackendPolicy policy, int max_steps,
            const HostBinding *bindings, size_t binding_count)
{
    BundleInstanceOptions options = {policy, max_steps};
    return BundleInstantiateWithOptions(bundle, bindings, binding_count, &options);
}

static void
run(BundleInstance *instance, BundleExecutionBackend backend, long long expected)
{
    long long result = 0;
    int present = 0;
    assert(instance != NULL);
    assert(BundleInstanceBackend(instance) == backend);
    assert(BundleInstanceRun(instance, &result, &present));
    assert(present && result == expected);
    BundleInstanceClose(instance);
}

static int labels;

static int
label(void *context, const char *module, const char *function,
      const HostValue *args, int count, HostValue *result)
{
    (void)context; (void)module;
    assert(strcmp(function, "Label") == 0 && count == 1);
    static const char *const names[] = {"zero", "one", "two"};
    assert(args[0].integer >= 0 && args[0].integer <= 2);
    const char *name = names[args[0].integer];
    labels++;
    result->kind = HOST_STRING;
    result->type = "string";
    result->data = (const unsigned char *)name;
    result->length = strlen(name);
    return 1;
}

static void
policies(void)
{
    Bundle *bundle = open_entry("Answer");
    assert(strcmp(BundleNativeProblem(bundle), "") == 0);
    assert(BundleInstanceBackend(NULL) == BUNDLE_EXECUTION_NONE);
    assert(BundleInstantiateWithOptions(bundle, NULL, 0, NULL) == NULL);
    assert(instantiate(bundle, BUNDLE_BACKEND_AUTO, -1, NULL, 0) == NULL);
    assert(instantiate(bundle, (BundleBackendPolicy)99, 0, NULL, 0) == NULL);
    run(instantiate(bundle, BUNDLE_BACKEND_NATIVE, 0, NULL, 0), BUNDLE_EXECUTION_NATIVE_X86_64, 42);
    run(instantiate(bundle, BUNDLE_BACKEND_ENGINE, 0, NULL, 0), BUNDLE_EXECUTION_ENGINE, 42);
    /* Auto is the platform's backend: native code on linux-x86_64. */
    run(instantiate(bundle, BUNDLE_BACKEND_AUTO, 0, NULL, 0), BUNDLE_EXECUTION_NATIVE_X86_64, 42);

    /* A second bundle of the same file shares its code; either may close
     * first, and a reopen after both closed still runs. */
    Bundle *again = open_entry("Answer");
    assert(strcmp(BundleNativeProblem(again), "") == 0);
    BundleInstance *left = instantiate(bundle, BUNDLE_BACKEND_NATIVE, 0, NULL, 0);
    BundleInstance *right = instantiate(again, BUNDLE_BACKEND_NATIVE, 0, NULL, 0);
    assert(left != NULL && right != NULL && left != right);
    BundleClose(bundle);
    run(left, BUNDLE_EXECUTION_NATIVE_X86_64, 42);
    run(right, BUNDLE_EXECUTION_NATIVE_X86_64, 42);
    BundleClose(again);
    bundle = open_entry("Answer");
    run(instantiate(bundle, BUNDLE_BACKEND_NATIVE, 0, NULL, 0), BUNDLE_EXECUTION_NATIVE_X86_64, 42);
    BundleClose(bundle);
}

static void
limits(void)
{
    long long result = 0;
    int present = 0;
    /* Fuel ends the run, and the instance stays ended. */
    Bundle *spin = open_entry("Spin");
    BundleInstance *instance = instantiate(spin, BUNDLE_BACKEND_NATIVE, 1000, NULL, 0);
    assert(instance != NULL);
    assert(!BundleInstanceRun(instance, &result, &present));
    assert(!BundleInstanceRun(instance, &result, &present));
    assert(BundleInstanceBackend(instance) == BUNDLE_EXECUTION_NATIVE_X86_64);
    BundleInstanceClose(instance);
    instance = instantiate(spin, BUNDLE_BACKEND_NATIVE, 0, NULL, 0);
    assert(instance != NULL);
    BundleInstanceLimitSteps(instance, 500);
    assert(!BundleInstanceRun(instance, &result, &present));
    BundleInstanceClose(instance);
    BundleClose(spin);

    /* Runaway recursion takes the stack trap instead of faulting. */
    Bundle *overflow = open_entry("Overflow");
    instance = instantiate(overflow, BUNDLE_BACKEND_NATIVE, 0, NULL, 0);
    assert(instance != NULL);
    assert(!BundleInstanceRun(instance, &result, &present));
    BundleInstanceClose(instance);
    BundleClose(overflow);

    /* More iterations than any fixed fuel budget run to the end. */
    Bundle *long_loop = open_entry("Long");
    run(instantiate(long_loop, BUNDLE_BACKEND_NATIVE, 0, NULL, 0), BUNDLE_EXECUTION_NATIVE_X86_64, 20);
    BundleClose(long_loop);
}

/* Native pointers are checked like the engine's: a read through a pointer
 * into a returned frame traps, and Vec storage grows, reads and frees. */
static void
checked(void)
{
    long long result = 0;
    int present = 0;
    /* A dangling local, use after free, double free and a read past the end
     * each end their instance with a trap. */
    const char *const traps[] = {"Dangling", "AfterFree", "FreeTwice", "PastEnd"};
    for(size_t at = 0; at < 4; at++) {
        Bundle *bundle = open_entry(traps[at]);
        assert(strcmp(BundleNativeProblem(bundle), "") == 0);
        BundleInstance *instance = instantiate(bundle, BUNDLE_BACKEND_NATIVE, 0, NULL, 0);
        assert(instance != NULL);
        assert(!BundleInstanceRun(instance, &result, &present));
        BundleInstanceClose(instance);
        BundleClose(bundle);
    }
    Bundle *grow = open_entry("Grow");
    run(instantiate(grow, BUNDLE_BACKEND_NATIVE, 0, NULL, 0), BUNDLE_EXECUTION_NATIVE_X86_64, 42);
    run(instantiate(grow, BUNDLE_BACKEND_ENGINE, 0, NULL, 0), BUNDLE_EXECUTION_ENGINE, 42);
    BundleClose(grow);
}

static void
state(void)
{
    long long result = 0;
    int present = 0;
    Bundle *bump = open_entry("Bump");
    BundleInstance *left = instantiate(bump, BUNDLE_BACKEND_NATIVE, 0, NULL, 0);
    BundleInstance *right = instantiate(bump, BUNDLE_BACKEND_NATIVE, 0, NULL, 0);
    assert(left != NULL && right != NULL);
    assert(BundleInstanceRun(left, &result, &present) && result == 1);
    assert(BundleInstanceRun(left, &result, &present) && result == 2);
    assert(BundleInstanceRun(right, &result, &present) && result == 1);
    BundleInstanceClose(left);
    BundleInstanceClose(right);
    BundleClose(bump);

    Bundle *retain = open_entry("Retain");
    HostBinding binding = {"native_bundle", "Label", label, NULL};
    run(instantiate(retain, BUNDLE_BACKEND_NATIVE, 0, &binding, 1), BUNDLE_EXECUTION_NATIVE_X86_64, 42);
    run(instantiate(retain, BUNDLE_BACKEND_ENGINE, 0, &binding, 1), BUNDLE_EXECUTION_ENGINE, 42);
    assert(labels == 4);
    BundleClose(retain);

    Bundle *fields = open_entry("Fields");
    run(instantiate(fields, BUNDLE_BACKEND_NATIVE, 0, NULL, 0), BUNDLE_EXECUTION_NATIVE_X86_64, 42);
    run(instantiate(fields, BUNDLE_BACKEND_ENGINE, 0, NULL, 0), BUNDLE_EXECUTION_ENGINE, 42);
    BundleClose(fields);
}

#if defined(__x86_64__) && defined(__linux__)
/* Optional measurement uses the constant-return entry: no work or host calls. */
static void
warm_entries(void)
{
    Bundle *bundle = open_entry("Answer");
    BundleInstance *instance = instantiate(bundle, BUNDLE_BACKEND_NATIVE, 0, NULL, 0);
    long long result = 0;
    int present = 0;
    assert(instance != NULL);
    for(int i = 0; i < 1000; i++)
        assert(BundleInstanceRun(instance, &result, &present) && present && result == 42);
    if(getenv("NATIVE_ENTRY_TIMING") != NULL) {
        for(int sample = 0; sample < 7; sample++) {
            struct timespec start, end;
            assert(clock_gettime(CLOCK_MONOTONIC, &start) == 0);
            for(int i = 0; i < 100000; i++)
                assert(BundleInstanceRun(instance, &result, &present) && present && result == 42);
            assert(clock_gettime(CLOCK_MONOTONIC, &end) == 0);
            double ns = (end.tv_sec - start.tv_sec) * 1e9 + end.tv_nsec - start.tv_nsec;
            printf("empty warm native entry sample %d: %.2f ns/call (100000 calls)\n", sample, ns / 100000);
        }
    }
    BundleInstanceClose(instance);
    BundleClose(bundle);
}

/* Each thread warms its own bounds before trapping another instance. */
static void *
stack_thread(void *unused)
{
    (void)unused;
    warm_entries();
    Bundle *bundle = open_entry("Overflow");
    BundleInstance *instance = instantiate(bundle, BUNDLE_BACKEND_NATIVE, 0, NULL, 0);
    long long result = 0;
    int present = 0;
    assert(instance != NULL);
    assert(!BundleInstanceRun(instance, &result, &present));
    BundleInstanceClose(instance);
    BundleClose(bundle);
    return NULL;
}

static void
alternate_stack(void)
{
    stack_thread(NULL);
}

static void
stack_entries(void)
{
    /* Warm main-thread bounds, then reduce RLIMIT_STACK in an isolated child. */
    warm_entries();
    unsetenv("NATIVE_ENTRY_TIMING");
    assert(fflush(NULL) == 0);
    pid_t child = fork();
    assert(child >= 0);
    if(child == 0) {
        struct rlimit limit;
        assert(getrlimit(RLIMIT_STACK, &limit) == 0);
        if(limit.rlim_cur > 1024 * 1024) {
            limit.rlim_cur = 1024 * 1024;
            assert(setrlimit(RLIMIT_STACK, &limit) == 0);
        }
        stack_thread(NULL);
        _exit(0);
    }
    int status;
    assert(waitpid(child, &status, 0) == child);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);

    pthread_t threads[2];
    pthread_attr_t attributes;
    assert(pthread_attr_init(&attributes) == 0);
    assert(pthread_attr_setstacksize(&attributes, 1024 * 1024) == 0);
    for(int i = 0; i < 2; i++)
        assert(pthread_create(&threads[i], &attributes, stack_thread, NULL) == 0);
    for(int i = 0; i < 2; i++) assert(pthread_join(threads[i], NULL) == 0);
    assert(pthread_attr_destroy(&attributes) == 0);

    /* Switching stacks must not reuse a cached pthread range. The established
     * fallback permits 1 MiB, so supply 2 MiB for this alternate stack. */
    ucontext_t caller, alternate;
    assert(getcontext(&alternate) == 0);
    alternate.uc_stack.ss_size = 2 * 1024 * 1024;
    alternate.uc_stack.ss_sp = malloc(alternate.uc_stack.ss_size);
    assert(alternate.uc_stack.ss_sp != NULL);
    alternate.uc_stack.ss_flags = 0;
    alternate.uc_link = &caller;
    makecontext(&alternate, alternate_stack, 0);
    assert(swapcontext(&caller, &alternate) == 0);
    free(alternate.uc_stack.ss_sp);
    stack_thread(NULL);
}

static int
reentrant_label(void *context, const char *module, const char *function,
                const HostValue *args, int count, HostValue *result)
{
    long long value = 0;
    int present = 0;
    assert(BundleInstanceRun(context, &value, &present) && present && value == 42);
    return label(NULL, module, function, args, count, result);
}

static void
reentrant_entries(void)
{
    Bundle *answer = open_entry("Answer");
    BundleInstance *inner = instantiate(answer, BUNDLE_BACKEND_NATIVE, 0, NULL, 0);
    Bundle *retain = open_entry("Retain");
    HostBinding binding = {"native_bundle", "Label", reentrant_label, inner};
    run(instantiate(retain, BUNDLE_BACKEND_NATIVE, 0, &binding, 1), BUNDLE_EXECUTION_NATIVE_X86_64, 42);
    BundleInstanceClose(inner);
    BundleClose(answer);
    BundleClose(retain);
}
#endif

int
main(int argc, char **argv)
{
    assert(argc == 2 && strlen(argv[1]) < sizeof(directory));
    strcpy(directory, argv[1]);
#if defined(__x86_64__) && defined(__linux__)
    policies();
    limits();
    state();
    checked();
    stack_entries();
    reentrant_entries();
    puts("native ZX: strict, engine and automatic backends, shared code, fuel, stack, globals, host strings and checked pointers");
#else
    Bundle *bundle = open_entry("Answer");
    assert(strncmp(BundleNativeProblem(bundle), "assembly.gap.target-unimplemented", 33) == 0);
    assert(instantiate(bundle, BUNDLE_BACKEND_NATIVE, 0, NULL, 0) == NULL);
    BundleClose(bundle);
    puts("native ZX: this host has no native backend and says so");
#endif
    return 0;
}
