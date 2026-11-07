#!/bin/sh
# A Text call in an else-if condition, in the second operand of `&&` or
# `||`, or in an ifx condition or arm of a scalar type is freed when its block
# closes: the else-if becomes an else block holding a plain if, and the arm
# runs as statements that leave its value in a local. Short-circuiting is
# kept, so a skipped arm never runs. Source and saved IR agree; C runs
# allocation-counted and under AddressSanitizer, and native and VM bundles
# give the same results.
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
zi2zx=$(dirname -- "$ziran")/zi2zx
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
cat > "$work/arms.zi" <<'ZI'
#import "std/vec"
Make :: (a: string) -> Text { return TextCopy(a) }
Width :: (s: string) -> s64 { return s.count }
calls: s64;
Counted :: (a: string) -> Text { calls += 1; return TextCopy(a) }

#program_export
Arms :: (rounds: s64) -> s64 {
    total: s64 = 0
    round: s64 = 0
    while round < rounds {
        n := round % 4
        if n > 0 && Width(Make("abc")) == 3 { total += 1 }
        ok := n == 0 || Width(Make("de")) == 2
        if ok { total += 10 }
        if n == 3 { total += 100 }
        else if Width(Make("fgh")) == n + 2 { total += 1000 }
        else if n == 0 || Width(Make("ij")) == n { total += 10000 }
        else { total += 100000 }
        if n > 0 && (n == 1 || Width(Make("klm")) == n + 1) { total += 1000000 }
        total += ifx n == 2 then Width(Make("nopq")) * 10000000 else 0
        total += ifx Width(Make("r")) == 1 then 0 else 1
        if n < 0 && Width(Counted("s")) > 0 { total += 1 }
        round += 1
    }
    return total + calls
}

#program_export
Run :: () -> s64 { return Arms(8) }
ZI
"$ziran" check --root "$work" --module-path "$repo" "$work/arms.zi"
"$ziran" ir --root "$work" --module-path "$repo" -o "$work/ir" "$work/arms.zi"
for kind in source saved; do
    root=$work; module=$work/arms.zi
    if test "$kind" = saved; then root=$work/ir; module=$work/ir/arms.zir; fi
    output=$work/$kind-c
    "$ziran" build --target=c --root "$root" --module-path "$repo" -o "$output" "$module"
    cat > "$work/count.c" <<'C'
#include "arms.h"
#include <stddef.h>
#include <stdio.h>
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
    /* 10010 + 1001011 + 41010011 + 111 for every four rounds */
    if (Arms(1000) != 10505285750LL) return 1;
    printf("1000 rounds: %ld allocations left\n", outstanding - before);
    return outstanding == before ? 0 : 2;
}
C
    "${CC:-cc}" -std=c11 -I"$repo/include" -I"$output" "$output"/*.c "$work/count.c" \
        -Wl,--wrap=realloc -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=free \
        -o "$work/count-$kind"
    "$work/count-$kind"
    printf '#include "arms.h"\nint main(void) { return Arms(40) == 420211430 && Run() == 84042286 ? 0 : 1; }\n' > "$work/plain.c"
    "${CC:-cc}" -std=c11 -g -fsanitize=address,undefined -fno-sanitize-recover=undefined \
        -fno-omit-frame-pointer -I"$repo/include" -I"$output" "$output"/*.c \
        "$work/plain.c" -o "$work/asan-$kind"
    ASAN_OPTIONS=detect_leaks=0 "$work/asan-$kind"
    "$ziran" bundle --entry arms:Run --root "$root" --module-path "$repo" -o "$work/$kind.zx" "$module"
    test "$("$zi2zx" run "$work/$kind.zx")" = 84042286
    test "$("$zi2zx" run --vm "$work/$kind.zx")" = 84042286
done
cmp "$work/source-c/arms.c" "$work/saved-c/arms.c"
echo 'Text in else-if conditions, && and || arms and scalar ifx: freed, 0 left; skipped arms never run; source and saved IR, native and VM agree; ASan clean'
