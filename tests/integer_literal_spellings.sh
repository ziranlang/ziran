#!/bin/sh
# Integer literals keep one meaning on every target: _ separators in
# decimal, hex and binary, and u64 values above the s64 maximum, in locals,
# globals, module constants, constant arrays, array sizes and range bounds.
# The portable VM verifies and runs them; native, C and Go agree, from
# source and saved IR.
set -eu

ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
run_vm="$(dirname "$ziran")/zi2zx-vm"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
K :: 1_000;
H :: 0xFF_FF;
Big: u64 = 18446744073709551615;
Mask: u64 = 0xFFFF_FFFF_FFFF_FFFF;
Sep: s64 = 1_000_000;
Bin: u8 = 0b1010_0101;
Low: s64 = -9_223_372_036_854_775_808;
T :: u64.[0xFFFF_FFFF_FFFF_FFFF, 1_0];
B :: u8.[0b1010_0101, 0x0_1];
#program_export
Answer :: () -> s64 {
    big: u64 = 18446744073709551615
    mask: u64 = 0xFFFF_FFFF_FFFF_FFFF
    half: u64 = 9_223_372_036_854_775_808
    sep: s64 = 1_000_000
    hex: u32 = 0xDEAD_BEEF
    bin: u8 = 0b1010_0101
    if big != mask || half >> 63 != 1 || big + 1 != 0 { return 1 }
    if sep != 1000000 || hex != 3735928559 || bin != 165 { return 2 }
    if K != 1000 || H != 65535 || K * 2 != 2_000 { return 3 }
    if Big != Mask || Big + 1 != 0 || Sep != 1000000 || Bin != 165 { return 4 }
    if Low + 1 != -9223372036854775807 { return 5 }
    if T[0] != Big || T[1] != 10 || B[0] != 165 || B[1] != 1 { return 6 }
    sized: [K]u8
    if sized.count != 1000 { return 7 }
    n: s64 = 0
    for i: 0..0x0_F { n += 1 }
    for i: 0..K - 1 { n += 1 }
    if n != 1_016 { return 8 }
    return 42
}
ZI

"$ziran" ir --root "$work" -o "$work/ir" "$work/app.zi"
for input in source saved; do
    if test "$input" = source; then
        module=$work/app.zi
        root=$work
    else
        module=$work/ir/app.zir
        root=$work/ir
    fi
    "$ziran" bundle --root "$root" --entry app:Answer \
        -o "$work/$input.zx" "$module"
    test "$("$ziran" run "$work/$input.zx")" = 42
    test "$("$run_vm" run "$work/$input.zx")" = 42

    output=$work/c-$input
    "$ziran" build --target=c --root "$root" -o "$output" "$module"
    cat > "$output/main.c" <<'C'
#include "app.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
    "${CC:-cc}" -std=c99 -pedantic-errors -Wall -Wno-unused-variable -O2 \
        -I"$repo/include" -I"$output" "$output"/*.c -o "$output/app"
    "$output/app"
    # The C source spells every literal without separators or 0b.
    if grep -En '(^|[^A-Za-z0-9_])([0-9][0-9A-Fa-fxX]*_[0-9A-Fa-f]|0[bB][01])' \
        "$output/app.c" "$output/app.h" >&2; then
        echo 'a literal kept a Ziran-only spelling in C' >&2
        exit 1
    fi
    grep -q '^#define K 1000$' "$output/app.h"
    grep -q '^#define H 65535$' "$output/app.h"

    output=$work/go-$input
    "$ziran" build --target=go --pkg main --root "$root" -o "$output" "$module"
    cat > "$output/main.go" <<'GO'
package main
func main() { if App_Answer() != 42 { panic("integer literal spellings") } }
GO
    (cd "$output" && GO111MODULE=off go run .)
done
cmp "$work/source.zx" "$work/saved.zx"
