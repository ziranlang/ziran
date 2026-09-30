#!/bin/sh
# A polymorphic procedure may bind several type parameters, each with its own
# $Name. Every call infers each one and gets its own specialization; source
# and saved IR agree on every target.
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/lib.zi" <<'ZI'
Pick :: (a: $A, b: $B) -> A { return a; }
Second :: (a: $A, b: $B) -> B { return b; }
ZI
cat > "$work/app.zi" <<'ZI'
#import "lib";
Pair :: struct { left: s32; right: s64; }
Fill :: (values: []$T, value: $V, count: s32) -> T {
    total: T = 0;
    for values { total += it; }
    return total + value * count;
}

#program_export
Answer :: () -> s32 {
    x: s32 = 3;
    if Pick(x, "text") != 3 || Second(x, "text") != "text" { return 1; }
    pair := Pick(Pair.{left = 4, right = 5}, x);
    if pair.right != 5 { return 2; }
    numbers: [3]s64 = .[1, 2, 3];
    small: s32 = 2;
    if Fill(numbers[:], small, 5) != 16 { return 3; }
    return 42;
}
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
