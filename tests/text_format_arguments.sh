#!/bin/sh
# A fresh Text handed straight to std/format's Append or BuilderPrint, whose
# first argument takes the builder's address, is scoped and freed like one
# handed to BuilderAppend: the builder's address is fixed, so the Text can be
# made first. Source and saved IR agree; C runs allocation-counted and under
# AddressSanitizer.
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
cat > "$work/fmt.zi" <<'ZI'
#import "std/vec"
#import "std/format"
Make :: (a: string) -> Text { return TextCopy(a) }

#program_export
Rounds :: (rounds: s64) -> s64 {
    total: s64 = 0
    round: s64 = 0
    while round < rounds {
        out: Vec(u8)
        Append(*out, Make("ab"))
        BuilderPrint(*out, "[%|%]", Make("cde"), round % 10)
        BuilderAppend(out, Make("f"))
        total += out.count
        VecFree(out)
        round += 1
    }
    return total
}

#program_export
Check :: () -> s32 {
    out: Vec(u8)
    BuilderPrint(*out, "% and %", Make("x"), Make("yz"))
    text := BuilderFinish(out)
    if text != "x and yz" { return 1 }
    return 42
}
ZI
"$ziran" check --root "$work" --module-path "$repo" "$work/fmt.zi"
"$ziran" ir --root "$work" --module-path "$repo" -o "$work/ir" "$work/fmt.zi"
for kind in source saved; do
    root=$work; module=$work/fmt.zi
    if test "$kind" = saved; then root=$work/ir; module=$work/ir/fmt.zir; fi
    output=$work/$kind-c
    "$ziran" build --target=c --root "$root" --module-path "$repo" -o "$output" "$module"
    if grep -q 'TextDetach' "$output/fmt.c"; then
        echo "$kind: a Text handed to Append or BuilderPrint was detached" >&2
        exit 1
    fi
    cat > "$work/count.c" <<'C'
#include "fmt.h"
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
    if (Rounds(1000) != 10000) return 1;
    long left = outstanding - before;
    printf("1000 rounds: %ld allocations left\n", left);
    if (Check() != 42) return 1;
    return left == 0 ? 0 : 2;
}
C
    "${CC:-cc}" -std=c11 -I"$repo/include" -I"$output" "$output"/*.c "$work/count.c" \
        -Wl,--wrap=realloc -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=free \
        -o "$work/count-$kind"
    "$work/count-$kind"
    printf '#include "fmt.h"\nint main(void) { return Rounds(50) == 500 && Check() == 42 ? 0 : 1; }\n' > "$work/plain.c"
    "${CC:-cc}" -std=c11 -g -fsanitize=address,undefined -fno-sanitize-recover=undefined \
        -fno-omit-frame-pointer -I"$repo/include" -I"$output" "$output"/*.c \
        "$work/plain.c" -o "$work/asan-$kind"
    ASAN_OPTIONS=detect_leaks=0 "$work/asan-$kind"
done
cmp "$work/source-c/fmt.c" "$work/saved-c/fmt.c"
echo 'Text handed to Append and BuilderPrint: freed, 0 left; source and saved IR agree; ASan clean'
