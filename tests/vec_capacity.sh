#!/bin/sh
set -eu
. "$(dirname -- "$0")/lib/expect_rejection.sh"
ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
case "$ziran" in /*) ;; *) ziran="$repo/$ziran" ;; esac
work="$repo/build/test-vec-capacity"
mkdir -p "$work/tmp"
export TMPDIR="$work/tmp"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY WAYLAND_SOCKET XDG_SESSION_ID XDG_SESSION_TYPE XDG_SESSION_CLASS XDG_SESSION_DESKTOP DESKTOP_SESSION SESSION_MANAGER DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS
unset XDG_CURRENT_DESKTOP XDG_RUNTIME_DIR DESKTOP_STARTUP_ID ICEAUTHORITY SSH_AUTH_SOCK ZIRAN_PROFILE GOBIN GOWORK
export ZIRAN_DESKTOP_RECOVERY=0 HARMONY_DESKTOP_RECOVERY=0 YUE_DESKTOP_RECOVERY=0
export GOCACHE="$work/go-cache" GOPATH="$work/go" GOTMPDIR="$work/tmp" GOMAXPROCS=2 GOFLAGS=-p=2 GOTOOLCHAIN=local
export ZIRAN_NATIVE_CACHE="$work/native-cache" XDG_CACHE_HOME="$work/cache"
ulimit -c 0
library="$(dirname "$ziran")/../libziran.a"
"${CC:-cc}" -std=c11 -D_GNU_SOURCE ${VM_CFLAGS:-} -I"$repo/include" -I"$repo/cmd/zir" "$repo/tests/vec_capacity_vm.c" "$library" -Wl,--wrap=calloc -lm -pthread -ldl -o "$work/vm"
printf '#include <stdlib.h>\n#include <stdint.h>\nint main(void) { return 0; }\n' > "$work/probe32.c"
have_c32=false
if "${CC:-cc}" -m32 "$work/probe32.c" -o "$work/probe32" > "$work/c32.log" 2>&1; then
    have_c32=true
else
    reason=$(sed -n 's/.*fatal error: /missing /p' "$work/c32.log" | head -n 1)
    echo "C 32-bit unavailable: ${reason:-multilib compiler/linker unavailable}"
fi
cp "$repo/tests/vec_capacity.zi" "$work/capacity.zi"
"$ziran" ir --root "$work" --module-path "$repo/std" -o "$work/ir" "$work/capacity.zi"
for input in source saved; do
    module="$work/capacity.zi"; root="$work"
    if test "$input" = saved; then module="$work/ir/capacity.zir"; root="$work/ir"; fi
    for entry in Answer Budget; do
        "$ziran" bundle --root "$root" --module-path "$repo/std" --entry "capacity:$entry" -o "$work/$input-$entry.zx" "$module"
        "$work/vm" "$work/$input-$entry.zx"
        if test "$entry" = Answer; then
            result=$("$ziran" run "$work/$input-$entry.zx")
            test "$result" = 42 || { echo "$input zx $entry: $result" >&2; exit 1; }
        fi
    done
    for target in c go; do
        output="$work/$input-$target"
        if test "$target" = go; then
            "$ziran" build --target=go --pkg main --root "$root" --module-path "$repo/std" -o "$output" "$module"
        else
            "$ziran" build --target=c --root "$root" --module-path "$repo/std" -o "$output" "$module"
        fi
        if test "$target" = c; then
            cat > "$output/main.c" <<'C'
#include "capacity.h"
#include <stdlib.h>
#include <stdio.h>
void *__real_realloc(void *, size_t);
void __real_free(void *);
static int live, fail_growth, failures, allocations, enforce_bound, bounded_allocations;
void *__wrap_realloc(void *p, size_t size) {
    if(enforce_bound) {
        const size_t limits[] = {5 * sizeof(uint32_t), 7 * sizeof(uint32_t)};
        if(bounded_allocations >= 2 || size > limits[bounded_allocations++]) abort();
    }
    if(fail_growth && size >= 4096) { failures++; return NULL; }
    void *q = __real_realloc(p, size);
    if(q) { allocations++; if(!p) live++; }
    return q;
}
void __wrap_free(void *p) { if(p) live--; __real_free(p); }
int main(void) {
    int answer = Answer();
    if(answer != 42 || live != 0) { fprintf(stderr,"Answer=%d live=%d\n", answer, live); return 1; }
    allocations = 0;
    if(Reuse() != 42 || allocations != 1 || live != 0) return 3;
    enforce_bound = 1;
    if(BoundedAllocation() != 42 || bounded_allocations != 2 || live != 0) return 4;
    enforce_bound = 0;
    fail_growth = 1;
    if(Failure() != 42 || live != 0 || failures != 6) return 2;
    return 0;
}
C
            "${CC:-cc}" -std=c99 -pedantic-errors -DZIRAN_BOUNDS_CHECK -I"$repo/include" -I"$output" "$output"/*.c -Wl,--wrap=realloc -Wl,--wrap=free -o "$output/app"
            "$output/app"
            "${CC:-cc}" -std=c99 -g -fsanitize=address,undefined -fno-omit-frame-pointer -DZIRAN_BOUNDS_CHECK -I"$repo/include" -I"$output" "$output"/*.c -Wl,--wrap=realloc -Wl,--wrap=free -o "$output/app-sanitized"
            ASAN_OPTIONS=detect_leaks=1 "$output/app-sanitized"
            if "$have_c32"; then
                "${CC:-cc}" -m32 -std=c99 -I"$repo/include" -I"$output" "$output"/*.c -Wl,--wrap=realloc -Wl,--wrap=free -o "$output/app32"
                "$output/app32"
                echo "$input C 32-bit: passed"
            fi
        else
            cat > "$output/main.go" <<'GO'
package main
func main() { if n := Capacity_Answer(); n != 42 { panic(n) } }
GO
            GO111MODULE=off go run "$output"/*.go
            GO111MODULE=off GOARCH=386 go run "$output"/*.go
        fi
    done
done
cmp "$work/source-Answer.zx" "$work/saved-Answer.zx"
# Actual VM calloc failures, including fallible owned-default initialization.
cp "$repo/tests/vec_capacity_failure.zi" "$work/failure.zi"
"$ziran" ir --root "$work" --module-path "$repo/std" -o "$work/failure-ir" "$work/failure.zi"
for input in source saved; do
    module="$work/failure.zi"; root="$work"
    if test "$input" = saved; then module="$work/failure-ir/failure.zir"; root="$work/failure-ir"; fi
    for entry in Check BoundedCheck; do
        "$ziran" bundle --root "$root" --module-path "$repo/std" --entry "failure:$entry" -o "$work/$input-failure-$entry.zx" "$module"
        "$work/vm" "$work/$input-failure-$entry.zx"
    done
done
# Reserved storage is not initialized length, even though allocation exists.
cat > "$work/bounds.zi" <<'ZI'
#import "vec"
#program_export
Answer :: () -> s32 {
    bytes: Vec(u8)
    if !VecReserve(bytes, 4) { return -1 }
    return cast(s32)bytes[0]
}
ZI
"$ziran" ir --root "$work" --module-path "$repo/std" -o "$work/bounds-ir" "$work/bounds.zi"
for input in source saved; do
    module="$work/bounds.zi"; root="$work"
    if test "$input" = saved; then module="$work/bounds-ir/bounds.zir"; root="$work/bounds-ir"; fi
    "$ziran" bundle --root "$root" --module-path "$repo/std" --entry bounds:Answer -o "$work/$input-bounds.zx" "$module"
    "$work/vm" "$work/$input-bounds.zx" expect-failure 2> "$work/bounds-vm.err"
    rg -q 'bounds|portable execution failed' "$work/bounds-vm.err"
    output="$work/bounds-$input-c"
    "$ziran" build --target=c --root "$root" --module-path "$repo/std" --entry bounds:Answer -o "$output" "$module"
    printf '#include "bounds.h"\nint main(void) { return Answer(); }\n' > "$output/main.c"
    "${CC:-cc}" -std=c99 -I"$repo/include" -I"$output" "$output"/*.c -o "$output/app"
    expect_rejection 'reserved capacity became indexable in C' "$work/bounds-c.err" "$output/app"
    rg -q 'Vec index out of bounds' "$work/bounds-c.err"
    output="$work/bounds-$input-go"
    "$ziran" build --target=go --pkg main --root "$root" --module-path "$repo/std" --entry bounds:Answer -o "$output" "$module"
    printf 'package main\nfunc main() { Bounds_Answer() }\n' > "$output/main.go"
    expect_rejection 'reserved capacity became indexable in Go' "$work/bounds-go.err" env GO111MODULE=off go run "$output"/*.go
    rg -q 'index out of range' "$work/bounds-go.err"
    expect_rejection 'unsupported wasm Vec accepted' "$work/wasm.err" "$(dirname "$ziran")/zi2wasm" --root "$root" --module-path "$repo/std" --entry bounds:Answer -o "$work/$input.wasm" "$module"
    rg -qi 'unsupported|not supported|cannot|wasm' "$work/wasm.err"
done
# The bounded primitive also rejects an unsupported backend explicitly.
printf '#import "vec"\n#program_export\nAnswer :: () -> s32 { v: Vec(u32); if !VecResizeBounded(v, 3, 5) { return 0; }; return 42; }\n' > "$work/bounded-only.zi"
"$ziran" ir --root "$work" --module-path "$repo/std" -o "$work/bounded-only-ir" "$work/bounded-only.zi"
for module in "$work/bounded-only.zi" "$work/bounded-only-ir/bounded-only.zir"; do
    expect_rejection 'unsupported wasm bounded Vec accepted' "$work/bounded-wasm.err" "$(dirname "$ziran")/zi2wasm" --root "$work" --module-path "$repo/std" --entry bounded-only:Answer -o "$work/bounded.wasm" "$module"
    rg -qi 'unsupported|not supported|cannot|wasm' "$work/bounded-wasm.err"
done
# Read and mutation restrictions are the same for both capacity operations.
for operation in VecReserve VecResize VecResizeBounded; do
    extra=""; arity_pattern="requires one s64 size"
    if test "$operation" = VecResizeBounded; then extra=", 6"; arity_pattern="requires two s64 sizes"; fi
    for mode in borrow moved size arity named owned_copy; do
        case "$mode" in
        borrow) body="v: Vec(u8); VecResize(v, 1); view := VecSlice(v, 0, 1); $operation(v, 3$extra); return cast(s32)view.count"; pattern='borrow' ;;
        moved) body="v: Vec(u8); moved := v; $operation(v, 3$extra); return cast(s32)moved.count"; pattern='after moving' ;;
        size) body="v: Vec(u8); $operation(v, cast(u64)3$extra); return 0"; pattern='s64 size' ;;
        arity) body="v: Vec(u8); $operation(v); return 0"; pattern="$arity_pattern" ;;
        named) body="v: Vec(u8); $operation(v, size = 3$extra); return 0"; pattern='named parameters' ;;
        owned_copy) body="v: Vec(Vec(u8)); VecResize(v, 1); copied := VecGet(v, 0); return 0"; pattern='cannot copy owned' ;;
        esac
        printf '#import "vec"\n#import "option"\nAnswer :: () -> s32 { %s }\n' "$body" > "$work/bad.zi"
        expect_rejection "$operation $mode accepted" "$work/bad.err" "$ziran" ir --root "$work" --module-path "$repo/std" -o "$work/bad-ir" "$work/bad.zi"
        rg -q "$pattern" "$work/bad.err"
    done
done
printf '#import "vec"\nAnswer :: () -> s32 { v: Vec(u8); VecResizeBounded(v, 1, cast(u64)8); return 0; }\n' > "$work/bad.zi"
expect_rejection 'unsigned ceiling accepted' "$work/bad.err" "$ziran" ir --root "$work" --module-path "$repo/std" -o "$work/bad-ir" "$work/bad.zi"
rg -q 's64 size' "$work/bad.err"
# Parent-routed isolated size_of([]u8) regression, without importing vec.
cp "$repo/tests/vec_capacity_layout.zi" "$work/layout.zi"
"$ziran" ir --root "$work" -o "$work/layout-ir" "$work/layout.zi"
for input in source saved; do
    module="$work/layout.zi"; root="$work"
    if test "$input" = saved; then module="$work/layout-ir/layout.zir"; root="$work/layout-ir"; fi
    output="$work/layout-$input"
    "$ziran" build --target=c --root "$root" --entry layout:Answer -o "$output" "$module"
    cat > "$output/main.c" <<'C'
#include "layout.h"
#include "zir_slice.h"
int main(void) { return Answer() == sizeof(Slice) ? 0 : 1; }
C
    "${CC:-cc}" -std=c99 -pedantic-errors -I"$repo/include" -I"$output" "$output"/*.c -o "$output/app"
    "$output/app"
done
echo 'vec capacity: source/saved IR, C/Go/VM, 386 Go, native/VM allocator failure, VM budget failure, ownership and borrow local tests passed'
