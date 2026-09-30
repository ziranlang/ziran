#!/bin/sh
# print shows a record as {x = 1, y = 2}: nested records in braces, strings
# quoted, enums by name, fixed arrays by element (up to 16), and Vecs and
# slices by count. Each argument is evaluated once, a value holding a Vec is
# read in place rather than moved, and records from other modules print
# too. Source and saved IR agree on every target.
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/shapes.zi" <<'ZI'
Point :: struct { x: s32; y: s32; }
ZI

cat > "$work/app.zi" <<'ZI'
#import "std/vec"
Shapes :: #import "shapes"

Color :: enum u8 { RED; GREEN; }
Inner :: struct { a: s32; b: bool; }
Record :: struct {
    x: s32
    y: float32
    name: string
    color: Color
    inner: Inner
    grid: [3]u8
    corner: Shapes.Point
}
Holder :: struct { items: Vec(s32); label: string; }
Wide :: struct {
    a0: u8; a1: u8; a2: u8; a3: u8; a4: u8; a5: u8; a6: u8; a7: u8; a8: u8; a9: u8;
    b0: u8; b1: u8; b2: u8; b3: u8; b4: u8; b5: u8; b6: u8; b7: u8; b8: u8; b9: u8;
    c0: u8; c1: u8; c2: u8; c3: u8; c4: u8; c5: u8; c6: u8; c7: u8; c8: u8; c9: u8;
}

calls: s32 = 0;
Make :: () -> Inner {
    calls += 1
    return Inner.{a = calls, b = false}
}

#program_export
Show :: () {
    r: Record
    r.x = 1
    r.y = 2.5
    r.name = "pt"
    r.color = .GREEN
    r.inner.a = 3
    r.inner.b = true
    r.grid[1] = 9
    r.corner.y = 4
    print("r=% (100%%)\n", r)
    print("% then %\n", Make(), calls)
    holder: Holder
    VecPush(holder.items, 5)
    VecPush(holder.items, 6)
    holder.label = "h"
    print("%\n", holder)
    print("% %\n", holder.items.count, r.grid)
    VecFree(holder.items)
    wide: Wide
    wide.c9 = 30
    print("%\n", wide)
}
ZI

cat > "$work/expected" <<'OUT'
r={x = 1, y = 2.5, name = "pt", color = GREEN, inner = {a = 3, b = true}, grid = [0, 9, 0], corner = {x = 0, y = 4}} (100%)
{a = 1, b = false} then 1
{items = [count = 2], label = "h"}
2 [0, 9, 0]
{a0 = 0, a1 = 0, a2 = 0, a3 = 0, a4 = 0, a5 = 0, a6 = 0, a7 = 0, a8 = 0, a9 = 0, b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0, b7 = 0, b8 = 0, b9 = 0, c0 = 0, c1 = 0, c2 = 0, c3 = 0, c4 = 0, c5 = 0, c6 = 0, c7 = 0, c8 = 0, c9 = 30}
OUT

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
    "$ziran" bundle --root "$root" --entry app:Show -o "$work/$input.zib" "$module"
    "$ziran" run "$work/$input.zib" > "$work/$input.out"
    cmp "$work/expected" "$work/$input.out"
    output="$work/c-$input"
    "$ziran" build --target=c --exe --entry app:Show --root "$root" -o "$output" "$module"
    "$output/app" > "$output.out"
    cmp "$work/expected" "$output.out"
    output="$work/cpp-$input"
    "$ziran" build --target=cpp --root "$root" -o "$output" "$module"
    cat > "$output/entry.cpp" <<'CPP'
#include "app.hpp"
int main() { Show(); return 0; }
CPP
    "${CXX:-c++}" -std=c++17 -I"$repo/include" -I"$output" "$output"/*.cpp -o "$output/app"
    "$output/app" > "$output.out"
    cmp "$work/expected" "$output.out"
    output="$work/go-$input"
    "$ziran" build --target=go --pkg main --root "$root" -o "$output" "$module"
    cat > "$output/entry.go" <<'GO'
package main
func main() { App_Show() }
GO
    (cd "$output" && GO111MODULE=off go run .) > "$output.out"
    cmp "$work/expected" "$output.out"
done
cmp "$work/source.zib" "$work/saved.zib"
