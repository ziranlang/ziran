#!/bin/sh
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/viewlib.zi" <<'ZI'
#program_export
Tail :: (values: []s32) -> []s32 {
    return values[1:]
}

#program_export
Sum :: (values: []s32) -> s32 {
    return values[0] + values[1]
}
ZI

cat > "$work/slices.zi" <<'ZI'
#import "viewlib"

Point :: struct {
    x: s32
}

Cell :: struct {
    head: Point
    tail: Point
}

WriteCell :: (cells: []Cell, index: s32, value: s32) -> s32 {
    cells[index] = Cell.{Point.{value}, Point.{value + 1}}
    return 1
}

ReplaceBorrowedRecord :: () -> s32 {
    cells: [2]Cell
    view: []Cell = cells[:]
    WriteCell(view, 0, 20)
    WriteCell(view, 1, 30)
    first: Cell = view[0]
    first.tail = Point.{21}
    view[0] = first
    return view[0].head.x + view[0].tail.x + view[1].head.x - 29
}

state: [2]s32;

GlobalView :: () -> []s32 {
    return state[:]
}

ReplaceGlobalWhileBorrowed :: () -> s32 {
    state[0] = 1
    state[1] = 2
    view: []s32 = GlobalView()
    replacement: [2]s32 = .[40, 2]
    state = replacement
    return view[0] + view[1]
}

ReplaceWhileBorrowed :: () -> s32 {
    values: [2]s32 = .[1, 2]
    view: []s32 = values[:]
    replacement: [2]s32 = .[40, 2]
    values = replacement
    return view[0] + view[1]
}

#program_export
Answer :: () -> s32 {
    if ReplaceBorrowedRecord() != 42 { return -5 }
    if ReplaceWhileBorrowed() != 42 { return -1 }
    global_result: s32 = ReplaceGlobalWhileBorrowed()
    if global_result != 42 { return global_result }
    empty: []s32
    if empty.count != 0 { return -3 }
    values: [4]s32 = .[1, 2, 3, 4]
    part: []s32 = values[1:3]
    part[0] = 40
    tail: []s32 = Tail(part)
    if part.count != 2 || tail.count != 1 ||
        tail[0] != 3 || values[1] != 40 { return -4 }
    points: [2]Point
    view: []Point = points[:]
    view[1].x = 7
    if points[1].x != 7 { return -5 }
    return Sum(part) - 1
}

#program_export
ReadOutside :: () -> s32 {
    values: [2]s32 = .[1, 2]
    view: []s32 = values[:]
    return view[2]
}

#program_export
WriteOutside :: () -> s32 {
    values: [2]s32 = .[1, 2]
    view: []s32 = values[:]
    view[2] = 3
    return values[0]
}

#program_export
RangeOutside :: () -> s32 {
    values: [2]s32 = .[1, 2]
    view: []s32 = values[0:3]
    return cast(s32)view.count
}

#program_export
RangeReverse :: () -> s32 {
    values: [2]s32 = .[1, 2]
    low: s32 = 2
    high: s32 = 1
    view: []s32 = values[low:high]
    return cast(s32)view.count
}
ZI

"$ziran" ir --root "$work" -o "$work/ir" "$work/slices.zi"
for input in source saved; do
    if test "$input" = source; then
        source=$work/slices.zi
        root=$work
    else
        source=$work/ir/slices.zir
        root=$work/ir
    fi
    "$ziran" bundle --root "$root" --entry slices:Answer \
        -o "$work/$input.zx" "$source"
    test "$("$ziran" run "$work/$input.zx")" = 42
    for target in c cpp go; do
        output="$work/$target-$input"
        "$ziran" build --target="$target" --root "$root" \
            -o "$output" "$source"
        if test "$target" = go; then
            cat > "$output/main_test.go" <<'GO'
package ziran
import "testing"
func TestSlice(t *testing.T) {
    if Slices_Answer() != 42 { t.Fatal("slice result") }
}
GO
            GO111MODULE=off go test "$output"/*.go
        elif test "$target" = c; then
            cat > "$output/main.c" <<'C'
#include "slices.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
            "${CC:-cc}" -std=c11 -I"$repo/include" -I"$output" \
                "$output"/*.c -o "$output/app"
            "$output/app"
        else
            cat > "$output/main.cpp" <<'CPP'
#include "slices.hpp"
int main() { return Answer() == 42 ? 0 : 1; }
CPP
            "${CXX:-c++}" -std=c++17 -I"$repo/include" -I"$output" \
                "$output"/*.cpp -o "$output/app"
            "$output/app"
        fi
    done
done

cat > "$work/invalid_slice_record.zi" <<'ZI'
Bad :: struct {
    values: [][]s32
}
ZI
if "$ziran" check --root "$work" "$work/invalid_slice_record.zi" \
    2> "$work/invalid_slice_record.err"; then
    echo 'nested slice descriptor entered a record' >&2
    exit 1
fi

cat > "$work/invalid_slice_global.zi" <<'ZI'
view: [][]s32;
ZI
if "$ziran" check --root "$work" "$work/invalid_slice_global.zi" \
    2> "$work/invalid_slice_global.err"; then
    echo 'nested slice descriptor entered a global' >&2
    exit 1
fi

cat > "$work/invalid_slice_escape.zi" <<'ZI'
#program_export
Escape :: () -> []s32 {
    values: [2]s32 = .[1, 2]
    return values[:]
}
ZI
if "$ziran" check --root "$work" "$work/invalid_slice_escape.zi" \
    2> "$work/invalid_slice_escape.err"; then
    echo 'local slice escaped its backing array' >&2
    exit 1
fi

cmp "$work/source.zx" "$work/saved.zx"

for entry in ReadOutside WriteOutside RangeOutside RangeReverse; do
    "$ziran" bundle --root "$work" --entry "slices:$entry" \
        -o "$work/$entry.zx" "$work/slices.zi"
    if "$ziran" run "$work/$entry.zx" 2> "$work/$entry.err"; then
        echo "portable slice $entry escaped its bounds" >&2
        exit 1
    fi
    rg -q 'portable execution failed' "$work/$entry.err"
done
