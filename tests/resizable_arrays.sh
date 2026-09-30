#!/bin/sh
# Jai's resizable arrays: [..]T is std/vec's Vec(T), imported automatically;
# array_add(*a, x), array_reset(*a), array_free(a), and
# array_reset_keeping_memory(*a) are Vec operations; and a local [..]T
# passed where []T is expected lends a view instead of moving. Source and
# saved IR agree on every target.
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
Point :: struct { x: s32; y: s32; }
Row :: struct { cells: [2]s32; }

Sum :: (values: [] s32) -> s32 {
    total: s32 = 0;
    for values { total += it; }
    return total;
}

Count :: (values: [..] s32) -> s64 { return values.count; }

#program_export
Answer :: () -> s32 {
    a: [..] s32;
    array_add(*a, 5);
    array_add(*a, 7);
    a[0] += 10;
    if a.count != 2 || a[0] != 15 || Sum(a) != 22 { return 1; }
    array_add(*a, 1);
    if Sum(a) != 23 { return 2; }
    points: [..]Point;
    array_add(*points, .{x = 1, y = 2});
    if points[0].y != 2 { return 3; }
    rows: [..]Row;
    array_add(*rows, .{cells = s32.[9, 8]});
    if rows.count != 1 || rows[0].cells[1] != 8 { return 4; }
    array_reset_keeping_memory(*a);
    if a.count != 0 { return 5; }
    array_add(*a, 4);
    if Count(a) != 1 { return 6; }
    array_reset(*points);
    array_free(rows);
    return 42;
}
ZI

cat > "$work/moved.zi" <<'ZI'
Count :: (values: [..] s32) -> s64 { return values.count; }
main :: () {
    a: [..] s32;
    array_add(*a, 1);
    print("% %\n", a.count, Count(a));
}
ZI
if "$ziran" check --root "$work" "$work/moved.zi" 2> "$work/moved.err"; then
    echo 'a Vec read and moved in one statement was accepted' >&2
    exit 1
fi
grep -q 'cannot also be read in it' "$work/moved.err"

printf 'main :: () { grid: [..][2]s32; }\n' > "$work/arrays.zi"
if "$ziran" check --root "$work" "$work/arrays.zi" 2> "$work/arrays.err"; then
    echo 'a Vec of fixed arrays was accepted' >&2
    exit 1
fi
grep -Fq 'wrap the array in a record: Vec([2]s32)' "$work/arrays.err"

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
