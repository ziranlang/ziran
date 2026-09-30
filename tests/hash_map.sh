#!/bin/sh
# std/hash_map keeps HashMap(K, V) from string or integer keys on every target
# and the portable runner: HashMapSet inserts or replaces, HashMapGet copies a
# value out, HashMapRemove leaves a slot later inserts reuse, and the table
# grows as it fills. Source and saved IR agree on every target.
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
#import "std/hash_map"

Point :: struct { x: s32; y: s32; }

Names :: () -> s32 {
    ages: HashMap(string, s32)
    HashMapSet(*ages, "ada", 36)
    HashMapSet(*ages, "grace", 45)
    HashMapSet(*ages, "ada", 37)
    age: s32 = 0
    if !HashMapGet(*ages, "ada", *age) || age != 37 { return 1 }
    if ages.count != 2 || !HashMapHas(*ages, "grace") || HashMapHas(*ages, "linus") { return 2 }
    if !HashMapRemove(*ages, "grace") || HashMapRemove(*ages, "grace") { return 3 }
    HashMapSet(*ages, "linus", 29)
    if ages.count != 2 || HashMapHas(*ages, "grace") || !HashMapGet(*ages, "linus", *age) || age != 29 { return 4 }
    HashMapFree(*ages)
    return 0
}

Squares :: () -> s32 {
    squares: HashMap(s64, s64)
    i: s64 = 0
    while i < 1000 { HashMapSet(*squares, i, i * i); i += 1; }
    i = 0
    while i < 1000 { if i % 3 == 0 { HashMapRemove(*squares, i); } i += 1; }
    total: s64 = 0
    value: s64 = 0
    i = 0
    while i < 1000 { if HashMapGet(*squares, i, *value) { total += value; } i += 1; }
    // Sum of squares below 1000 without the multiples of three.
    if total != 221555889 || squares.count != 666 { return 5 }
    HashMapFree(*squares)
    if squares.count != 0 || HashMapHas(*squares, 1) { return 6 }
    return 0
}

Points :: () -> s32 {
    points: HashMap(u32, Point)
    HashMapSet(*points, 7, Point.{x = 1, y = 2})
    HashMapSet(*points, 9, Point.{x = 3, y = 4})
    found: Point
    if !HashMapGet(*points, 9, *found) || found.x != 3 || found.y != 4 { return 7 }
    HashMapFree(*points)
    return 0
}

#program_export
Answer :: () -> s32 {
    failed := Names()
    if failed == 0 { failed = Squares() }
    if failed == 0 { failed = Points() }
    if failed != 0 {
        print("failed %\n", failed)
        return failed
    }
    return 42
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
