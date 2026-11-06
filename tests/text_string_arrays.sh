#!/bin/sh
# A procedure that copies its string parameters into a local `[N]string`
# and only reads the elements, through an index, a `for` loop or `.count`,
# only reads those parameters, so a fresh Text passed to it is freed. One
# that lets an element escape, here into a global, keeps the Text. Source
# and saved IR agree; C runs allocation-counted and under AddressSanitizer.
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
cat > "$work/arrays.zi" <<'ZI'
#import "std/vec"
Make :: (a: string) -> Text { return TextCopy(a) }

// Reads every piece through the array.
Cat :: (a: string, b: string = "", c: string = "") -> s64 {
    parts: [3]string = .[a, b, c]
    total: s64 = 0
    for part: parts { total += part.count }
    return total + parts[0].count + parts.count
}

kept: string;
Escape :: (a: string) -> s64 {
    parts: [1]string = .[a]
    kept = parts[0]
    return parts.count
}

#program_export
Read :: (rounds: s64) -> s64 {
    total: s64 = 0
    round: s64 = 0
    while round < rounds {
        total += Cat(Make("ab"), Make("c"), "d")
        round += 1
    }
    return total
}

#program_export
Kept :: (rounds: s64) -> s64 {
    total: s64 = 0
    round: s64 = 0
    while round < rounds {
        total += Escape(Make("e"))
        round += 1
    }
    return total
}
ZI
"$ziran" check --root "$work" --module-path "$repo" "$work/arrays.zi"
"$ziran" ir --root "$work" --module-path "$repo" -o "$work/ir" "$work/arrays.zi"
for kind in source saved; do
    root=$work; module=$work/arrays.zi
    if test "$kind" = saved; then root=$work/ir; module=$work/ir/arrays.zir; fi
    output=$work/$kind-c
    "$ziran" build --target=c --root "$root" --module-path "$repo" -o "$output" "$module"
    cat > "$work/count.c" <<'C'
#include "arrays.h"
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
    if (Read(1000) != 9000) return 1;
    long read = outstanding - before;
    before = outstanding;
    if (Kept(1000) != 1000) return 1;
    long kept = outstanding - before;
    printf("1000 rounds: %ld allocations left read, %ld escaped\n", read, kept);
    return read == 0 && kept == 1000 ? 0 : 2;
}
C
    "${CC:-cc}" -std=c11 -I"$repo/include" -I"$output" "$output"/*.c "$work/count.c" \
        -Wl,--wrap=realloc -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=free \
        -o "$work/count-$kind"
    "$work/count-$kind"
    printf '#include "arrays.h"\nint main(void) { return Read(50) == 450 && Kept(50) == 50 ? 0 : 1; }\n' > "$work/plain.c"
    "${CC:-cc}" -std=c11 -g -fsanitize=address,undefined -fno-sanitize-recover=undefined \
        -fno-omit-frame-pointer -I"$repo/include" -I"$output" "$output"/*.c \
        "$work/plain.c" -o "$work/asan-$kind"
    ASAN_OPTIONS=detect_leaks=0 "$work/asan-$kind"
done
cmp "$work/source-c/arrays.c" "$work/saved-c/arrays.c"
echo 'string parameters read through a local [N]string: freed, 0 left; an escaping array keeps them; source and saved IR agree; ASan clean'
