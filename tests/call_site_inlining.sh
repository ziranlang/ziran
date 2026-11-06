#!/bin/sh
# A call of a small procedure of the same module is emitted as that
# procedure's body in generated C, so compilers that never inline, such as
# TinyCC under `ziran run`, skip the call. Arguments still run once, left to
# right; a parameter may share a name with the caller's argument; an early
# return leaves the body; a private procedure every caller inlined is no C
# warning; and the inlined call still costs its step under `ziran run`.
set -eu

ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY

cat > "$work/app.zi" <<'ZI'
Pair :: struct {
    low: s32
    high: s32
}
counter: s64 = 0;
Tick :: () -> s64 {
    counter += 1
    return counter
}
Twice :: (x: s64) -> s64 { return x * 2 }
Clamp :: (value: s64, low: s64, high: s64) -> s64 {
    if value < low { return low }
    if value > high { return high }
    return value
}
Order :: (first: s64, second: s64) -> s64 { return first * 10 + second }
Swap :: (pair: Pair) -> Pair { return Pair.{.low = pair.high, .high = pair.low} }
Unused :: (ignored: s64, used: s64) -> s64 { return used + 1 }
#program_export
Answer :: () -> s64 {
    x: s64 = 5
    value: s64 = 40
    if Twice(x) != 10 || Clamp(value, 0, 9) != 9 || Clamp(-3, 0, 9) != 0 { return 1 }
    if Clamp(4, 0, 9) != 4 { return 2 }
    // Arguments run once each, left to right.
    if Order(Tick(), Tick()) != 12 || counter != 2 { return 3 }
    // The caller's `low` and `value` reach parameters with those names.
    low: s64 = 7
    if Clamp(low, value, 50) != 40 || Clamp(value, low, 8) != 8 { return 4 }
    p := Swap(Pair.{.low = 1, .high = 2})
    if p.low != 2 || p.high != 1 || Unused(Tick(), 41) != 42 || counter != 3 { return 5 }
    return 42
}
ZI

"$ziran" bundle --root "$work" --entry app:Answer -o "$work/app.zx" "$work/app.zi"
test "$("$ziran" run "$work/app.zx")" = 42

output=$work/c
"$ziran" build --target=c --root "$work" -o "$output" "$work/app.zi"
cat > "$output/main.c" <<'C'
#include "app.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
for level in -O0 -O2; do
    "${CC:-cc}" -std=c99 -pedantic-errors -Wall -Werror $level \
        -I"$repo/include" -I"$output" "$output"/*.c -o "$output/app"
    "$output/app"
done
sed -n '/^Answer(/,/^}/p' "$output/app.c" > "$work/answer.c"
for name in Twice Clamp Order Swap Unused; do
    if grep -q "app_$name(" "$work/answer.c"; then
        echo "a call of $name was not inlined" >&2
        exit 1
    fi
done
grep -q 'app_Tick(' "$work/answer.c"
