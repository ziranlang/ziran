#!/bin/sh
# Procedures may share a name when their parameters differ. A call chooses
# the overload whose parameters take its arguments best: exact types first,
# then an untyped literal's usual type, then widening. The choice is made in
# checking, so source and saved IR agree on every target.
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/shapes.zi" <<'ZI'
Area :: (w: s32, h: s32) -> s32 { return w * h; }
Area :: (side: s32) -> s32 { return side * side; }
ZI
cat > "$work/app.zi" <<'ZI'
#import "shapes";
Shapes :: #import "shapes";

Describe :: (x: s32) -> s32 { return 1; }
Describe :: (x: string) -> s32 { return 2; }
Describe :: (x: float64, scale: s32 = 10) -> s32 { return scale; }
Describe :: (x: s64) -> s32 { return 4; }

#program_export
Answer :: () -> s32 {
    small: s32 = 3;
    wide: s64 = 3;
    byte: u8 = 3;
    if Describe(small) != 1 || Describe("a") != 2 { return 1; }
    if Describe(2.5) != 10 || Describe(2.5, scale = 7) != 7 { return 2; }
    if Describe(wide) != 4 || Describe(5) != 4 { return 3; }
    if Describe(byte) != 1 { return 4; }
    if Area(3, 4) != 12 || Area(5) != 25 { return 5; }
    if Shapes.Area(2, 3) != 6 || Shapes.Area(4) != 16 { return 6; }
    return 42;
}
ZI

reject() {
    name=$1
    message=$2
    cat > "$work/$name.zi"
    if "$ziran" check --root "$work" "$work/$name.zi" 2> "$work/$name.err"; then
        echo "$name: accepted" >&2
        exit 1
    fi
    if ! grep -Fq "$message" "$work/$name.err"; then
        cat "$work/$name.err" >&2
        exit 1
    fi
}
reject nomatch 'no overload accepts these arguments: F(string)' <<'ZI'
F :: (x: s32) -> s32 { return x; }
F :: (x: bool) -> s32 { return 0; }
main :: () { print("%\n", F("text")); }
ZI
reject tie 'overloads match these arguments equally well: F(u8)' <<'ZI'
F :: (x: s32) -> s32 { return 1; }
F :: (x: u32) -> s32 { return 2; }
main :: () { value: u8 = 1; print("%\n", F(value)); }
ZI
reject duplicate 'F is already declared with these parameters' <<'ZI'
F :: (x: s32) -> s32 { return 1; }
F :: (x: s32) -> s32 { return 2; }
ZI

"$ziran" check --root "$work" "$work/app.zi"
# API listings name each overload as callers write it.
"$ziran" api --root "$work" "$work/shapes.zi" > "$work/api.txt"
test "$(grep -c '^  Area :: ' "$work/api.txt")" = 2
if grep -q overload "$work/api.txt"; then cat "$work/api.txt" >&2; exit 1; fi
"$ziran" ir --root "$work" -o "$work/ir" "$work/app.zi"
for input in source saved; do
    if test "$input" = source; then
        module=$work/app.zi
        root=$work
    else
        module=$work/ir/app.zir
        root=$work/ir
    fi
    "$ziran" bundle --root "$root" --entry app:Answer -o "$work/$input.zib" "$module"
    test "$("$ziran" run "$work/$input.zib")" = 42
    output="$work/c-$input"
    "$ziran" build --target=c --exe --entry app:Answer --root "$root" -o "$output" "$module"
    status=0
    "$output/app" || status=$?
    test "$status" = 42
    output="$work/cpp-$input"
    "$ziran" build --target=cpp --root "$root" -o "$output" "$module"
    cat > "$output/main.cpp" <<'CPP'
#include "app.hpp"
int main() { return Answer() == 42 ? 0 : 1; }
CPP
    "${CXX:-c++}" -I"$repo/include" -I"$output" "$output"/*.cpp -o "$output/app"
    "$output/app"
    output="$work/go-$input"
    "$ziran" build --target=go --pkg main --root "$root" -o "$output" "$module"
    cat > "$output/main.go" <<'GO'
package main
func main() { if App_Answer() != 42 { panic("wrong result") } }
GO
    GO111MODULE=off go run "$output"/*.go
done
cmp "$work/source.zib" "$work/saved.zib"
