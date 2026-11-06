#!/bin/sh
# A fresh Text result that is only read within its statement, such as an
# argument to a procedure that only reads it, a comparison or a `.count`, is
# freed when its block closes instead of kept as a string. A callee that
# stores, pushes, returns, aliases or remembers the argument in a global keeps
# the old string. The result is never computed earlier than written: after
# another call in the same expression, in a loop or else-if condition, or in
# an `&&`, `||` or `ifx` arm it stays in place. Source and saved IR agree on C
# (allocation-counted and under AddressSanitizer), Go and portable `.zx`.
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/scoped.zi" <<'ZI'
#import "vec"
Holder :: struct { s: string; names: Vec(string); }
calls: s64 = 0;
last: string = "";

Make :: (a: string, b: string) -> Text {
    calls += 1
    out: Vec(u8)
    if !BuilderAppend(out, a) || !BuilderAppend(out, b) { return .{} }
    return BuilderFinish(out)
}
Late :: () -> Text {
    out: Vec(u8)
    if calls >= 10 { BuilderAppend(out, "late") } else { BuilderAppend(out, "x") }
    return BuilderFinish(out)
}
Bump :: () -> s64 { calls += 10; return 0 }

// Only read during the call.
Take :: (s: string) -> s64 { return s.count }
Outer :: (s: string) -> s64 { return Take(s) + Take(s[1:]) + cast(s64) s[0] }
// Keep the argument past the call.
Same :: (s: string) -> string { return s }
Store :: (h: *Holder, s: string) { h.s = s }
Push :: (h: *Holder, s: string) { VecPush(h.names, s) }
Remember :: (s: string) { last = s }
Alias :: (h: *Holder, s: string) { t := s; h.s = t }

#program_export
Scoped :: (rounds: s64) -> s64 {
    total: s64 = 0
    round: s64 = 0
    while round < rounds {
        total += Take(Make("ab", "c"))
        total += Outer(Make("xy", "z"))
        if Make("a", "b") == "ab" { total += 1 }
        total += Make("q", "").count
        b: Vec(u8)
        BuilderAppend(b, Make("b", "uilt"))
        total += Take(BuilderFinish(b))
        early: Vec(u8)
        BuilderAppend(early, "e")
        if BuilderFinish(early) != "e" { return -1 }
        round += 1
    }
    return total
}

Fill :: (h: *Holder) {
    Store(h, Make("fi", "eld"))
    Push(h, Make("ve", "c"))
    Remember(Make("gl", "obal"))
}
Kept :: () -> string { return Same(Make("re", "turned")) }
Aliased :: (h: *Holder) { Alias(h, Make("al", "ias")) }

#program_export
Retained :: () -> s64 {
    h: Holder
    Fill(*h)
    kept := Kept()
    if h.s != "field" || h.names[0] != "vec" || last != "global" || kept != "returned" { return 1 }
    a: Holder
    Aliased(*a)
    if a.s != "alias" { return 2 }
    return 0
}

#program_export
Order :: () -> s64 {
    calls = 0
    // Late must run after Bump.
    if Bump() + Take(Late()) != 4 { return 1 }
    // Arms that do not run never make their Text.
    calls = 0
    no := false
    if no && Take(Make("a", "")) > 0 { return 2 }
    if no || Take(Make("a", "")) == 1 { calls += 0 }
    n := ifx no then Take(Make("a", "")) else 0
    if calls != 1 || n != 0 { return 3 }
    if !no { calls += 0 } else if Take(Make("a", "")) > 0 { return 4 }
    if calls != 1 { return 5 }
    // A loop condition makes its Text every time it runs.
    i: s64 = 0
    while Take(Make("ab", "")) > i { i += 1 }
    if calls != 4 { return 6 }
    return 0
}

#program_export
Check :: () -> s32 {
    if Scoped(1) != 3 + 125 + 1 + 1 + 5 { return 1 }
    if Retained() != 0 { return 2 }
    if Order() != 0 { return 3 }
    return 42
}
ZI

"$ziran" check --root "$work" --module-path "$repo/std" "$work/scoped.zi"
"$ziran" ir --root "$work" --module-path "$repo/std" \
    -o "$work/ir" "$work/scoped.zi"
for kind in source saved; do
    if test "$kind" = source; then
        root=$work
        module=$work/scoped.zi
    else
        root=$work/ir
        module=$work/ir/scoped.zir
    fi
    "$ziran" bundle --entry scoped:Check --root "$root" \
        --module-path "$repo/std" -o "$work/$kind.zx" "$module"
    test "$("$ziran" run "$work/$kind.zx")" = 42

    c_output=$work/$kind-c
    "$ziran" build --target=c --entry scoped:Check --root "$root" \
        --module-path "$repo/std" -o "$c_output" "$module"
    cat > "$work/count.c" <<'C'
#include "scoped.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
void *__real_realloc(void *, size_t);
void *__real_malloc(size_t);
void *__real_calloc(size_t, size_t);
void __real_free(void *);
static long outstanding;
void *__wrap_malloc(size_t size) { void *r = __real_malloc(size); if (r) outstanding++; return r; }
void *__wrap_calloc(size_t n, size_t size) { void *r = __real_calloc(n, size); if (r) outstanding++; return r; }
void *__wrap_realloc(void *p, size_t size) { void *r = __real_realloc(p, size); if (!p && r) outstanding++; return r; }
void __wrap_free(void *p) { if (p) outstanding--; __real_free(p); }
int main(void) {
    long before = outstanding;
    if (Scoped(1000) != 135000) return 1;
    long scoped = outstanding - before;
    printf("1000 rounds: %ld allocations left\n", scoped);
    if (Check() != 42) return 1;
    return scoped == 0 ? 0 : 2;
}
C
    "${CC:-cc}" -std=c11 -I"$repo/include" -I"$c_output" \
        "$c_output"/*.c "$work/count.c" -Wl,--wrap=realloc -Wl,--wrap=malloc \
        -Wl,--wrap=calloc -Wl,--wrap=free -o "$work/count-$kind"
    "$work/count-$kind"
    # A library build, without --entry, lowers the checked program directly.
    library=$work/$kind-library
    "$ziran" build --target=c --root "$root" --module-path "$repo/std" \
        -o "$library" "$module"
    cat > "$work/plain.c" <<'C'
#include "scoped.h"
int main(void) { return Scoped(50) == 6750 && Check() == 42 ? 0 : 1; }
C
    "${CC:-cc}" -std=c11 -g -fsanitize=address,undefined \
        -fno-sanitize-recover=undefined -fno-omit-frame-pointer \
        -I"$repo/include" -I"$library" "$library"/*.c "$work/plain.c" \
        -o "$work/asan-$kind"
    ASAN_OPTIONS=detect_leaks=0 "$work/asan-$kind"

    go_output=$work/$kind-go
    "$ziran" build --target=go --pkg main --entry scoped:Check \
        --root "$root" --module-path "$repo/std" \
        -o "$go_output" "$module"
    cat > "$go_output/main.go" <<'GO'
package main
func main() { if Scoped_Check() != 42 { panic("scoped text arguments") } }
GO
    GO111MODULE=off go run "$go_output"/*.go
done

cmp "$work/source.zx" "$work/saved.zx"
cmp "$work/source-c/scoped.c" "$work/saved-c/scoped.c"
cmp "$work/source-go/scoped.go" "$work/saved-go/scoped.go"
