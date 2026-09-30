#!/bin/sh
# Jai operator procedures: `operator + :: (a: V, b: V) -> V` gives records
# +, and several declarations of one operator are overloads. `a += b` uses
# operator +, `a != b` falls back to operator ==, and #symmetric also takes
# the arguments swapped. Operators reach callers through open and named
# imports. Records without an operator procedure get a clear error. Source
# and saved IR agree on every target.
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/vectors.zi" <<'ZI'
V :: struct { x: s32; y: s32; }
operator + :: (a: V, b: V) -> V { return V.{x = a.x + b.x, y = a.y + b.y}; }
operator - :: (a: V, b: V) -> V { return V.{x = a.x - b.x, y = a.y - b.y}; }
operator * :: (a: V, k: s32) -> V #symmetric { return V.{x = a.x * k, y = a.y * k}; }
operator * :: (a: V, b: V) -> s32 { return a.x * b.x + a.y * b.y; }
operator == :: (a: V, b: V) -> bool { return a.x == b.x && a.y == b.y; }
operator < :: (a: V, b: V) -> bool { return a.x * a.x + a.y * a.y < b.x * b.x + b.y * b.y; }
ZI

cat > "$work/app.zi" <<'ZI'
#import "vectors"
Named :: #import "vectors"

Open :: () -> s32 {
    a := V.{x = 1, y = 2}
    b := V.{x = 3, y = 4}
    c := a + b
    if c.x != 4 || c.y != 6 { return 1 }
    if a * b != 11 { return 2 }
    d := 2 * a - a * 3
    if d.x != -1 || d.y != -2 { return 3 }
    if a == b || !(a != b) || !(a < b) { return 4 }
    e := a
    e += b
    e -= a
    if e != b { return 5 }
    return 0
}

Qualified :: () -> s32 {
    a := Named.V.{x = 2, y = 0}
    b := a * 5 + a
    if b.x != 12 { return 6 }
    return 0
}

#program_export
Answer :: () -> s32 {
    failed := Open()
    if failed == 0 { failed = Qualified() }
    if failed != 0 { return failed }
    return 42
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
    grep -Fq "$message" "$work/$name.err" || { cat "$work/$name.err" >&2; exit 1; }
    test "$(wc -l < "$work/$name.err")" = 1 || { cat "$work/$name.err" >&2; exit 1; }
}
reject missing 'record operation needs an operator procedure' <<'ZI'
P :: struct { x: s32; }
main :: () { a: P; b: P; c := a + b; print("%\n", c); }
ZI
reject compound 'record compound assignment needs an operator procedure: +=' <<'ZI'
P :: struct { x: s32; }
main :: () { a: P; b: P; a += b; }
ZI
reject index 'operator procedures are written operator + :: (a: T, b: T) -> T' <<'ZI'
P :: struct { x: s32; }
operator [] :: (a: P, i: s32) -> s32 { return a.x; }
ZI
reject symmetric '#symmetric requires an operator procedure with two parameters' <<'ZI'
Twice :: (a: s32) -> s32 #symmetric { return a * 2; }
ZI

"$ziran" check --root "$work" "$work/app.zi"
# API listings show operator procedures as declared.
"$ziran" api --root "$work" "$work/vectors.zi" > "$work/api.txt"
grep -Fq '  operator + :: (a: V, b: V) -> V' "$work/api.txt"
test "$(grep -c '^  operator \* :: ' "$work/api.txt")" = 3
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
