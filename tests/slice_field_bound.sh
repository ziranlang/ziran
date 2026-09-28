#!/bin/sh
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

# An array slice bounded by a nested s32 field keeps that bound's type in Go,
# where an int32 value cannot initialize an int64 temporary.
cat > "$work/bound.zi" <<'ZI'
Name :: struct {
    bytes: [64]u8
    length: s32
}

Token :: struct {
    name: Name
}

Count :: (bytes: []u8) -> s32 {
    return cast(s32)bytes.count
}

#program_export
Answer :: () -> s32 {
    token: Token
    token.name.length = 2
    return 40 + Count(token.name.bytes[0:token.name.length])
}
ZI

"$ziran" ir --root "$work" --entry bound:Answer -o "$work/ir" "$work/bound.zi"
for input in source saved; do
    if test "$input" = source; then
        source=$work/bound.zi
        root=$work
    else
        source=$work/ir/bound.zir
        root=$work/ir
    fi
    "$ziran" bundle --root "$root" --entry bound:Answer \
        -o "$work/$input.zib" "$source"
    test "$("$ziran" run "$work/$input.zib")" = 42
    for target in c cpp go; do
        output="$work/$target-$input"
        "$ziran" build --target="$target" --root "$root" \
            --entry bound:Answer -o "$output" "$source"
        if test "$target" = go; then
            cat > "$output/main_test.go" <<'GO'
package ziran
import "testing"
func TestBound(t *testing.T) {
    if Bound_Answer() != 42 { t.Fatal(Bound_Answer()) }
}
GO
            GO111MODULE=off go test "$output"/*.go
        elif test "$target" = c; then
            cat > "$output/main.c" <<'C'
#include "bound.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
            "${CC:-cc}" -std=c11 -I"$repo/include" -I"$output" \
                "$output"/*.c -o "$output/app"
            "$output/app"
        else
            cat > "$output/main.cpp" <<'CPP'
#include "bound.hpp"
int main() { return Answer() == 42 ? 0 : 1; }
CPP
            "${CXX:-c++}" -std=c++17 -I"$repo/include" -I"$output" \
                "$output"/*.cpp -o "$output/app"
            "$output/app"
        fi
    done
done
echo 'slice bounded by an s32 field: VM, C, C++, and Go passed'
