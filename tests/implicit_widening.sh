#!/bin/sh
# A narrower integer of the same signedness, an unsigned integer in a wider
# signed type, and float32 in float64 convert implicitly at initializers,
# assignments, returns, arguments, and binary operators. The checker inserts
# an explicit cast, so source and saved IR agree on every target. Conversions
# that could lose a value still need an explicit cast.
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/widen.zi" <<'ZI'
Meters :: struct { value: s32; }
ToMeters :: #as (v: s32) -> Meters { return .{value = v}; }
Add3 :: (a: s64, b: s64, c: s64) -> s64 { return a + b + c; }
Code :: (c: u8) -> s32 { return c; }
Wide :: (x: u32) -> u64 { return x; }
Half :: (x: float64) -> float64 { return x / 2; }

#program_export
Answer :: () -> s32 {
    small: s32 = 40;
    big: s64 = small;
    byte: u8 = 200;
    code: s32 = byte;
    half_word: u16 = byte;
    narrow_real: float32 = 1.5;
    wide_real: float64 = narrow_real;
    total: s64 = 0;
    for i: 0..small { total += i; }
    big += small;
    count := 0;
    if count < small { count = small; }
    m: Meters = 7;
    sum := Add3(small, byte, m.value);
    if big != 80 || code != 200 || half_word != 200 { return 1; }
    if wide_real != 1.5 || Half(narrow_real) != 0.75 { return 2; }
    if total != 820 || count != 40 || sum != 247 { return 3; }
    if Code(byte) != 200 || Wide(4000000000) != 4000000000 { return 4; }
    return 42;
}
ZI

reject() {
    name=$1
    cat > "$work/$name.zi"
    if "$ziran" check --root "$work" "$work/$name.zi" 2> "$work/$name.err"; then
        echo "$name: lossy implicit conversion was accepted" >&2
        exit 1
    fi
}
reject narrowing <<'ZI'
F :: (x: s64) -> s32 { return x; }
ZI
reject sign_change <<'ZI'
F :: (x: s32) -> u64 { return x; }
ZI
reject same_width_unsigned <<'ZI'
F :: (x: u32) -> s32 { return x; }
ZI
reject float_to_single <<'ZI'
F :: (x: float64) -> float32 { return x; }
ZI
reject int_to_float <<'ZI'
F :: (x: s32) -> float64 { return x; }
ZI

"$ziran" check --root "$work" "$work/widen.zi"
"$ziran" ir --root "$work" -o "$work/ir" "$work/widen.zi"
for input in source saved; do
    if test "$input" = source; then
        module=$work/widen.zi
        root=$work
    else
        module=$work/ir/widen.zir
        root=$work/ir
    fi
    "$ziran" bundle --root "$root" --entry widen:Answer \
        -o "$work/$input.zib" "$module"
    test "$("$ziran" run "$work/$input.zib")" = 42
    output="$work/c-$input"
    "$ziran" build --target=c --root "$root" -o "$output" "$module"
    cat > "$output/main.c" <<'C'
#include "widen.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
    "${CC:-cc}" -I"$repo/include" -I"$output" "$output"/*.c -o "$output/app"
    "$output/app"
    output="$work/cpp-$input"
    "$ziran" build --target=cpp --root "$root" -o "$output" "$module"
    cat > "$output/main.cpp" <<'CPP'
#include "widen.hpp"
int main() { return Answer() == 42 ? 0 : 1; }
CPP
    "${CXX:-c++}" -I"$repo/include" -I"$output" "$output"/*.cpp -o "$output/app"
    "$output/app"
    output="$work/go-$input"
    "$ziran" build --target=go --pkg main --root "$root" -o "$output" "$module"
    cat > "$output/main.go" <<'GO'
package main
func main() { if Widen_Answer() != 42 { panic("wrong result") } }
GO
    GO111MODULE=off go run "$output"/*.go
done
cmp "$work/source.zib" "$work/saved.zib"
