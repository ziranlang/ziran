#!/bin/sh
# Procedures with several results: `-> s32, s32` or named `-> (a: s32, b: s32)`,
# `return x, y`, `a, b := F()`, `a, b = F()`, `_` to skip one, and the first
# result where one value is expected. Source and saved IR agree on every
# target.
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/lib.zi" <<'ZI'
DivMod :: (a: s32, b: s32) -> s32, s32 {
    return a / b, a % b;
}
ZI
cat > "$work/app.zi" <<'ZI'
#import "lib";

MinMax :: (a: s32, b: s32) -> (low: s32, high: s32) {
    if a < b { return a, b; }
    return b, a;
}
Local :: (a: s32, b: s32) -> s32, s32 { return a / b, a % b; }
Pass :: () -> s32, s32 { return Local(9, 4); }
Forward :: () -> s32, s32 {
    q, r := DivMod(9, 4);
    return q, r;
}
Twice :: (x: s64) -> s64 { return x * 2; }
Split :: (x: s64) -> s64, bool, string {
    return x / 2, x % 2 == 0, "split";
}

#program_export
Answer :: () -> s32 {
    q, r := DivMod(17, 5);
    if q != 3 || r != 2 { return 1; }
    low, _ := MinMax(9, 4);
    if low != 4 { return 2; }
    x: s32 = 0;
    y: s32 = 0;
    x, y = MinMax(8, 3);
    if x != 3 || y != 8 { return 3; }
    first := DivMod(7, 2);
    wide: s64 = DivMod(20, 3);
    if first != 3 || wide != 6 || DivMod(8, 3) + 1 != 3 { return 4; }
    if Twice(DivMod(10, 3)) != 6 { return 5; }
    a, b := Pass();
    if a != 2 || b != 1 { return 6; }
    c, d := Forward();
    if c != 2 || d != 1 { return 6; }
    half, even, label := Split(9);
    if half != 4 || even || label != "split" { return 7; }
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
    grep -Fq "$message" "$work/$name.err"
}
reject count 'return gives 3 values but the procedure has 2 results' <<'ZI'
F :: () -> s32, s32 { return 1, 2, 3; }
ZI
reject generic 'a polymorphic procedure cannot have several results yet' <<'ZI'
F :: (x: $T) -> T, T { return x, x; }
ZI
reject imported 'results from another module must be bound before they are returned' <<'ZI'
#import "lib";
F :: () -> s32, s32 { return DivMod(1, 1); }
ZI

"$ziran" check --root "$work" "$work/app.zi"
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
