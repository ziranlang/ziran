#!/bin/sh
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

# A cast that widens a narrower arithmetic group keeps the wide type when it
# is the operand of a shift or product; C computes both at the width of their
# operands, so the group must not stay 32 bits wide.
cat > "$work/widen.zi" <<'ZI'
#program_export
Answer :: () -> s32 {
    x: u32 = 0x1234
    high: u64 = cast(u64)(x & 0xffff) << 32
    if high != 0x123400000000 { return 1 }
    grouped: u64 = (cast(u64)(x + 1)) << 32
    if grouped != 0x123500000000 { return 2 }
    y: u32 = 0x10000
    product: u64 = cast(u64)(y | 0) * cast(u64)(y + 0)
    if product != 0x100000000 { return 3 }
    return 42
}
ZI

"$ziran" ir --root "$work" --entry widen:Answer -o "$work/ir" "$work/widen.zi"
for input in source saved; do
    if test "$input" = source; then
        source=$work/widen.zi
        root=$work
    else
        source=$work/ir/widen.zir
        root=$work/ir
    fi
    "$ziran" bundle --root "$root" --entry widen:Answer \
        -o "$work/$input.zib" "$source"
    test "$("$ziran" run "$work/$input.zib")" = 42
    for target in c cpp go; do
        output="$work/$target-$input"
        "$ziran" build --target="$target" --root "$root" \
            --entry widen:Answer -o "$output" "$source"
        if test "$target" = go; then
            cat > "$output/main_test.go" <<'GO'
package ziran
import "testing"
func TestWiden(t *testing.T) {
    if Widen_Answer() != 42 { t.Fatal(Widen_Answer()) }
}
GO
            GO111MODULE=off go test "$output"/*.go
        elif test "$target" = c; then
            cat > "$output/main.c" <<'C'
#include "widen.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
            "${CC:-cc}" -std=c11 -I"$repo/include" -I"$output" \
                "$output"/*.c -o "$output/app"
            "$output/app"
        else
            cat > "$output/main.cpp" <<'CPP'
#include "widen.hpp"
int main() { return Answer() == 42 ? 0 : 1; }
CPP
            "${CXX:-c++}" -std=c++17 -I"$repo/include" -I"$output" \
                "$output"/*.cpp -o "$output/app"
            "$output/app"
        fi
    done
done
echo 'widening casts of narrow groups: VM, C, C++, and Go passed'
