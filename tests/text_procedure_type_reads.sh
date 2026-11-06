#!/bin/sh
# `#reads` on a procedure type promises that every procedure stored in it
# only reads its string parameters and the string fields of its record
# parameters during the call, so a fresh Text passed through a value of
# that type, a local slot or a record field, is freed when its block
# closes. A procedure that keeps such a string cannot be stored in the type.
# An unmarked type still keeps the Text. Source and saved IR agree; C runs
# allocation-counted and under AddressSanitizer.
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
cat > "$work/slots.zi" <<'ZI'
#import "std/vec"
Props :: struct { text: string; size: s32; }
Painter :: #type (x: s32, props: Props, label: string) -> s32 #reads;
Plain :: #type (label: string) -> s32;
Hooks :: struct { paint: Painter; }

Make :: (a: string) -> Text { return TextCopy(a) }
Paint :: (x: s32, props: Props, label: string) -> s32 {
    return x + cast(s32)props.text.count + props.size + cast(s32)label[0:1].count
}
Count :: (label: string) -> s32 { return cast(s32)label.count }

#program_export
Read :: (rounds: s64) -> s64 {
    hooks := Hooks.{.paint = Paint}
    slot: Painter = Paint
    total: s64 = 0
    round: s64 = 0
    while round < rounds {
        total += cast(s64)hooks.paint(1, Props.{.text = Make("abc"), .size = 2}, Make("d"))
        total += cast(s64)slot(0, Props.{.text = "", .size = 0}, Make("ef"))
        round += 1
    }
    return total
}

#program_export
Kept :: (rounds: s64) -> s64 {
    plain: Plain = Count
    total: s64 = 0
    round: s64 = 0
    while round < rounds {
        total += cast(s64)plain(Make("g"))
        round += 1
    }
    return total
}
ZI
"$ziran" check --root "$work" --module-path "$repo" "$work/slots.zi"
"$ziran" ir --root "$work" --module-path "$repo" -o "$work/ir" "$work/slots.zi"
for kind in source saved; do
    root=$work; module=$work/slots.zi
    if test "$kind" = saved; then root=$work/ir; module=$work/ir/slots.zir; fi
    output=$work/$kind-c
    "$ziran" build --target=c --root "$root" --module-path "$repo" -o "$output" "$module"
    cat > "$work/count.c" <<'C'
#include "slots.h"
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
    /* (1 + 3 + 2 + 1) + (0 + 0 + 0 + 1) for each round */
    if (Read(1000) != 8000) return 1;
    long read = outstanding - before;
    before = outstanding;
    if (Kept(1000) != 1000) return 1;
    long kept = outstanding - before;
    printf("1000 rounds: %ld allocations left read, %ld kept\n", read, kept);
    return read == 0 && kept == 1000 ? 0 : 2;
}
C
    "${CC:-cc}" -std=c11 -I"$repo/include" -I"$output" "$output"/*.c "$work/count.c" \
        -Wl,--wrap=realloc -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=free \
        -o "$work/count-$kind"
    "$work/count-$kind"
    printf '#include "slots.h"\nint main(void) { return Read(50) == 400 && Kept(50) == 50 ? 0 : 1; }\n' > "$work/plain.c"
    "${CC:-cc}" -std=c11 -g -fsanitize=address,undefined -fno-sanitize-recover=undefined \
        -fno-omit-frame-pointer -I"$repo/include" -I"$output" "$output"/*.c \
        "$work/plain.c" -o "$work/asan-$kind"
    ASAN_OPTIONS=detect_leaks=0 "$work/asan-$kind"
done
cmp "$work/source-c/slots.c" "$work/saved-c/slots.c"

# A procedure that keeps a string cannot be stored in a #reads type, as a
# local slot, a record field, an argument or a global's initializer.
mkdir "$work/bad"
cat > "$work/bad/main.zi" <<'ZI'
Props :: struct { text: string; }
Painter :: #type (props: Props, label: string) -> s32 #reads;
Hooks :: struct { paint: Painter; }
kept: string;
KeepLabel :: (props: Props, label: string) -> s32 { kept = label; return 0 }
KeepField :: (props: Props, label: string) -> s32 { kept = props.text; return 0 }
KeepGlobal :: (props: Props, label: string) -> s32 { kept = label; return 0 }
global_hooks: Hooks = .{paint = KeepGlobal};
Use :: (paint: Painter) -> s32 { return paint(Props.{.text = ""}, "") }
#program_export
Main :: () -> s32 {
    slot: Painter = KeepLabel
    hooks := Hooks.{.paint = KeepField}
    return Use(KeepLabel) + slot(Props.{.text = ""}, "") + hooks.paint(Props.{.text = ""}, "")
}
ZI
if "$ziran" check --root "$work/bad" "$work/bad/main.zi" 2> "$work/bad.err"; then
    echo 'a keeping procedure was stored in a #reads procedure type' >&2
    exit 1
fi
grep -q 'KeepLabel keeps its parameter label, but Painter is #reads' "$work/bad.err"
grep -q 'KeepField keeps its parameter props.text, but Painter is #reads' "$work/bad.err"
grep -q 'KeepGlobal keeps its parameter label, but Painter is #reads' "$work/bad.err"
echo 'Text passed through a #reads procedure type: freed, 0 left; unmarked types keep it; keeping procedures are rejected; source and saved IR agree; ASan clean'
