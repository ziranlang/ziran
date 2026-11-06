#!/bin/sh
# A string local that only ever holds views of a parameter or literals, set
# with `=` and only read, keeps nothing: a fresh Text passed as that
# parameter is freed after the call. Whole values, slices and field slices
# of a record parameter are covered; a local that lets its view escape into
# a global still keeps the Text. Source and saved IR agree; C runs
# allocation-counted and under AddressSanitizer.
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
cat > "$work/views.zi" <<'ZI'
#import "std/vec"
Card :: (round: s64) -> Text {
    out: Vec(u8)
    BuilderAppend(out, "ca")
    BuilderAppend(out, ifx round % 2 == 0 then "rd" else "RD")
    return BuilderFinish(out)
}
Width :: (s: string) -> s32 { return cast(s32)s.count }

ReassignedWhole :: (s: string, n: s32) -> s32 {
    v: string = s
    if n > 1 { v = "x" }
    return Width(v)
}
ReassignedSlice :: (s: string, n: s32) -> s32 {
    v: string = s[0:2]
    if n > 0 { v = s[1:3] }
    return Width(v) + n
}
Props :: struct { text: string; size: s32; }
FieldSlices :: (props: Props) -> s32 {
    line: string = props.text[0:1]
    i: s32 = 0
    while i < props.size {
        line = props.text[0:i + 1]
        i += 1
    }
    return Width(line)
}

kept: string;
Escape :: (s: string) -> s32 {
    v: string = "y"
    v = s
    kept = v
    return 1
}

#program_export
Read :: (rounds: s64) -> s64 {
    total: s64 = 0
    round: s64 = 0
    while round < rounds {
        total += ReassignedWhole(Card(round), 1)
        total += ReassignedSlice(Card(round), 1)
        total += FieldSlices(Props.{.text = Card(round), .size = 3})
        round += 1
    }
    return total
}

#program_export
Kept :: (rounds: s64) -> s64 {
    total: s64 = 0
    round: s64 = 0
    while round < rounds {
        total += Escape(Card(round))
        round += 1
    }
    return total
}
ZI
"$ziran" check --root "$work" --module-path "$repo" "$work/views.zi"
"$ziran" ir --root "$work" --module-path "$repo" -o "$work/ir" "$work/views.zi"
for kind in source saved; do
    root=$work; module=$work/views.zi
    if test "$kind" = saved; then root=$work/ir; module=$work/ir/views.zir; fi
    output=$work/$kind-c
    "$ziran" build --target=c --root "$root" --module-path "$repo" -o "$output" "$module"
    cat > "$work/count.c" <<'C'
#include "views.h"
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
    /* 4 + 3 + 3 for each round */
    if (Read(1000) != 10000) return 1;
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
    printf '#include "views.h"\nint main(void) { return Read(50) == 500 && Kept(50) == 50 ? 0 : 1; }\n' > "$work/plain.c"
    "${CC:-cc}" -std=c11 -g -fsanitize=address,undefined -fno-sanitize-recover=undefined \
        -fno-omit-frame-pointer -I"$repo/include" -I"$output" "$output"/*.c \
        "$work/plain.c" -o "$work/asan-$kind"
    ASAN_OPTIONS=detect_leaks=0 "$work/asan-$kind"
done
cmp "$work/source-c/views.c" "$work/saved-c/views.c"
echo 'string locals reassigned only views of parameters: freed, 0 left; an escaping view keeps them; source and saved IR agree; ASan clean'
