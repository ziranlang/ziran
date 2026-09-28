#!/bin/sh
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

# Named integer constants keep their full value: a packed RGBA color or a
# 64-bit mask folds without the s32 limit that array bounds keep.
cat > "$work/wide.zi" <<'ZI'
Opaque :: 0xEDF4FCFF;
Mask :: 0xFFFFFFFF;
Doubled :: Mask * 2;

#program_export
Answer :: () -> s32 {
    color: u32 = Opaque
    if color >> 24 != 237 { return 1 }
    wide: s64 = Doubled
    if wide != 8589934590 { return 2 }
    low: u8 = cast(u8)(color & 255)
    if low != 255 { return 3 }
    return 42
}
ZI

"$ziran" ir --root "$work" --entry wide:Answer -o "$work/ir" "$work/wide.zi"
for input in source saved; do
    if test "$input" = source; then
        source=$work/wide.zi
        root=$work
    else
        source=$work/ir/wide.zir
        root=$work/ir
    fi
    "$ziran" bundle --root "$root" --entry wide:Answer \
        -o "$work/$input.zib" "$source"
    test "$("$ziran" run "$work/$input.zib")" = 42
    for target in c cpp go; do
        output="$work/$target-$input"
        "$ziran" build --target="$target" --root "$root" \
            --entry wide:Answer -o "$output" "$source"
        if test "$target" = go; then
            cat > "$output/main_test.go" <<'GO'
package ziran
import "testing"
func TestWide(t *testing.T) {
    if Wide_Answer() != 42 { t.Fatal(Wide_Answer()) }
}
GO
            GO111MODULE=off go test "$output"/*.go
        elif test "$target" = c; then
            cat > "$output/main.c" <<'C'
#include "wide.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
            "${CC:-cc}" -std=c11 -I"$repo/include" -I"$output" \
                "$output"/*.c -o "$output/app"
            "$output/app"
        else
            cat > "$output/main.cpp" <<'CPP'
#include "wide.hpp"
int main() { return Answer() == 42 ? 0 : 1; }
CPP
            "${CXX:-c++}" -std=c++17 -I"$repo/include" -I"$output" \
                "$output"/*.cpp -o "$output/app"
            "$output/app"
        fi
    done
done

echo 'named constants beyond s32: VM, C, C++, and Go passed'
