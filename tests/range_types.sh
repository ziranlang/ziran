#!/bin/sh
# `x: type_of(expression)` declares x with that expression's checked type,
# and a for range counts in its bounds' type, as in Jai: 0..count with
# count: s32 counts in s32, a u8 range stops at 0 without wrapping, and
# mixed widths count in the wider type. Constant ranges count in s64.
# Source and saved IR agree on every target.
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
Declared :: () -> s32 {
    a: s32 = 3
    b: type_of(a) = 4
    c: type_of(a + 1) = 5
    f: float32 = 1.5
    g: type_of(f * 2) = 2.5
    narrow: s32 = b + c
    half: float32 = g
    if narrow != 9 || half != 2.5 { return 1 }
    return 0
}

Ranges :: () -> s32 {
    count: s32 = 3
    total: s32 = 0
    for i: 0..count { total += i }
    for 1..count { total += it * 10 }
    for i: 0..count { if i == 1 { continue } total += cast(s32) it_index }
    if total != 71 { return 2 }
    n: u8 = 4
    small: u8 = 0
    for < k: 0..n { small += k }
    if small != 10 { return 3 }
    big: s64 = 2
    wide: s64 = 0
    for j: count..big + 3 { wide += j }
    if wide != 12 { return 4 }
    steps: s64 = 0
    for 0..9 { steps += it }
    if steps != 45 { return 5 }
    return 0
}

#program_export
Answer :: () -> s32 {
    failed := Declared()
    if failed == 0 { failed = Ranges() }
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
reject missing 'unresolved name: missing' <<'ZI'
main :: () { x: type_of(missing) = 1; print("%\n", x); }
ZI
reject slice_range 'slices are written value[start:end]: s[1..3]' <<'ZI'
main :: () { s := "hello"; t := s[1..3]; print("%\n", t); }
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
