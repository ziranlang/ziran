#!/bin/sh
# Generated C leaves out the bounds check of a two-dimensional index such as
# row * stride + column when a guard before it, an enclosing if or the loop
# conditions prove it lies inside its slice or array: an if that returns
# when input.count < (height + 3) * stride + width + 4, loops bounding row
# and column, and && or || operands that rule values out before later ones
# run. A local such as at := row * stride + column declared before the
# guard that bounds what it reads is bounded there too. Guards that do not cover the read, or test a sum or product that may
# wrap, whether directly, through a local, an ifx choice or a loop limit,
# keep its check, and those reads still abort on C, Go, native and the
# portable VM from source and saved IR.
set -eu

ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
run_vm="$(dirname "$ziran")/zi2zx"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
// A CDEF-style block: the guard rules out other widths before the count
// test runs, so (height + 3) * stride cannot wrap.
Block :: (input: []u16, stride: s32, width: s32, height: s32, output: []u16) -> s64 {
    if (width != 4 && width != 8) || (height != 4 && height != 8) || stride < width + 4 ||
        stride > 64 || input.count < cast(s64)(height + 3) * stride + width + 4 ||
        output.count < cast(s64)height * width {
        return -1
    }
    total: s64 = 0
    for row: 0..height + 3 {
        for column: 0..width + 3 { total += cast(s64)input[row * stride + column] }
    }
    for row: 0..height - 1 {
        line := row * width
        for column: 0..width - 1 { output[line + column] = input[(row + 2) * stride + column + 2] }
    }
    return total
}
// A restoration-style window: rows y + 3 of a padded block.
Window :: (input: []u16, stride: s32, width: s32, height: s32) -> s64 {
    if width < 1 || width > 8 || height < 1 || height > 8 || stride < width + 6 || stride > 64 ||
        input.count < cast(s64)(height + 5) * stride + width + 6 {
        return -1
    }
    total: s64 = 0
    for y: 0..height - 1 {
        for x: 0..width - 1 { total += cast(s64)input[(y + 3) * stride + x + 3] }
    }
    return total
}
// A palette-style neighbour: the guard bounds the sample, an enclosing if
// the column. Without column <= 64 the s32 index could wrap.
Neighbor :: (indices: []u8, stride: s32, row: s32, column: s32) -> s64 {
    if row < 0 || column < 0 || stride < 0 || stride > 64 || row > 64 || column > 64 ||
        cast(s64)row * stride + column >= indices.count {
        return -1
    }
    total := cast(s64)indices[row * stride + column]
    if column > 0 { total += cast(s64)indices[row * stride + column - 1] }
    return total
}
// A coefficient-context-style neighbourhood: at is computed before the
// guard that bounds row, column and stride, and at + 4 * stride bounds the
// smaller offsets, as stride >= 4.
Levels :: (levels: []u8, stride: s32, row: s32, column: s32) -> s64 {
    at := row * stride + column
    if row < 0 || column < 0 || stride < 4 || stride > 36 || row > 35 || column > 35 ||
        cast(s64)at + 4 * cast(s64)stride >= levels.count {
        return -1
    }
    return cast(s64)levels[at + 1] + cast(s64)levels[at + stride] + cast(s64)levels[at + 2 * stride] +
        cast(s64)levels[at + 4] + cast(s64)levels[at + 4 * stride]
}
// An enclosing if and a fixed array a guard bounds.
Inside :: (view: []s64, x: s64, n: s64) -> s64 {
    t: [8]s64
    total: s64 = 0
    if x >= 0 && x < view.count { total += view[x] }
    if n < 0 || n > 8 { return total }
    for i: 0..n - 1 { t[i] = i; total += t[i] }
    return total
}
// The guard bounds rows 0..height-1, but the loop reads row height.
Short :: (input: []u16, stride: s32, height: s32) -> s64 {
    if stride < 1 || stride > 64 || height < 1 || height > 8 ||
        input.count < cast(s64)height * stride {
        return -1
    }
    total: s64 = 0
    for row: 0..height { total += cast(s64)input[row * stride] }
    return total
}
// The index moves after the guard.
Moved :: (view: []s64, x: s64) -> s64 {
    if x < 0 || x >= view.count { return -1 }
    y := x
    y = y + 1
    return view[y]
}
// A true || proves neither side.
Either :: (view: []s64, a: s64, x: s64) -> s64 {
    if x < 0 { return -1 }
    if a < 0 || x < view.count { return view[x] }
    return 0
}
// An else-if guard returns only when the first branch did not run.
Branch :: (view: []s64, a: s64, x: s64) -> s64 {
    r: s64 = 0
    if x < 0 { return -1 }
    if a > 0 { r = 1 } else if x >= view.count { return -1 }
    return r + view[x]
}
// The guard tests the stride the read does not use.
Other :: (view: []s64, stride: s64, wide: s64, row: s64) -> s64 {
    if stride < 0 || stride > 64 || wide < 0 || wide > 64 || row < 0 || row > 64 ||
        row * stride >= view.count {
        return -1
    }
    return view[row * wide]
}
// first + 1 + second may wrap past the s64 range, so the guard bounds
// neither term.
Wrapped :: (view: []s64, first: s64, second: s64) -> s64 {
    if first < 0 || second < 0 { return -1 }
    count := first + 1 + second
    if count > view.count { return -1 }
    return view[first]
}
// Nor does a product that may wrap bound the column.
Plane :: (view: []s64, row: s64, stride: s64, column: s64) -> s64 {
    if row < 0 || stride < 0 || column < 0 || row * stride + column >= view.count { return -1 }
    return view[column]
}
// A local holding a sum that may wrap.
Through :: (view: []s64, a: s64, b: s64) -> s64 {
    if a < 0 || b < 0 { return -1 }
    total := a * 2 + b
    if total >= view.count { return -1 }
    return view[b]
}
// An ifx choice of terms of a sum that may wrap.
Choice :: (view: []s64, a: s64, b: s64, flip: bool) -> s64 {
    if a < 0 || b < 0 || a + b >= view.count { return -1 }
    at := ifx flip then a else b
    return view[at]
}
// Without row <= 35, row * stride may wrap to a negative at.
Unbounded :: (levels: []u8, stride: s32, row: s32, column: s32) -> s64 {
    at := row * stride + column
    if row < 0 || column < 0 || stride < 4 || stride > 36 || column > 35 ||
        cast(s64)at + 4 * cast(s64)stride >= levels.count {
        return -1
    }
    return cast(s64)levels[at + 1]
}
// The guard bounds row after at was computed from an earlier row.
Reset :: (levels: []u8, stride: s32, row: s32, column: s32) -> s64 {
    at := row * stride + column
    row = 0
    if row < 0 || column < 0 || stride < 4 || stride > 36 || row > 35 || column > 35 ||
        cast(s64)at + 4 * cast(s64)stride >= levels.count {
        return -1
    }
    return cast(s64)levels[at + 1]
}
// One past the offset the guard bounds.
Edge :: (levels: []u8, stride: s32, row: s32, column: s32) -> s64 {
    at := row * stride + column
    if row < 0 || column < 0 || stride < 4 || stride > 36 || row > 35 || column > 35 ||
        at + 4 * stride >= levels.count {
        return -1
    }
    return cast(s64)levels[at + 4 * stride + 1]
}
// A loop limit bounded only by a sum that may wrap.
Limit :: (view: []s64, n: s64, m: s64) -> s64 {
    if n < 0 || m < 0 || n + m > view.count { return -1 }
    total: s64 = 0
    for i: 0..n - 1 { total += view[i] }
    return total
}
#program_export
Answer :: () -> s64 {
    input: [256]u16
    for i: 0..255 { input[i] = cast(u16)(i & 7) }
    output: [64]u16
    view: [4]s64
    view[0] = 5; view[1] = 6; view[2] = 7; view[3] = 8
    indices: [64]u8
    for i: 0..63 { indices[i] = cast(u8)i }
    block := Block(input[0:220], 16, 8, 8, output[0:64])
    window := Window(input[0:256], 16, 8, 8)
    neighbor := Neighbor(indices[0:64], 8, 3, 5)
    inside := Inside(view[0:4], 2, 8)
    levels := Levels(indices[0:64], 8, 3, 5)
    expect: s64 = 0
    for row: 0..11 { for column: 0..11 { expect += cast(s64)((row * 16 + column) & 7) } }
    if block != expect { return 1 }
    if output[9] != input[2 * 16 + 1 + 16 + 2] { return 2 }
    expect = 0
    for y: 0..7 { for x: 0..7 { expect += cast(s64)(((y + 3) * 16 + x + 3) & 7) } }
    if window != expect { return 3 }
    if neighbor != 29 + 28 { return 4 }
    if inside != 7 + 28 { return 5 }
    if Block(input[0:187], 16, 8, 8, output[0:64]) != -1 { return 6 }
    if Neighbor(indices[0:64], 8, 7, 8) != -1 { return 7 }
    if levels != 30 + 37 + 45 + 33 + 61 { return 8 }
    if Levels(indices[0:61], 8, 3, 5) != -1 { return 9 }
    if Edge(indices[0:64], 4, 11, 2) != cast(s64)indices[63] { return 10 }
    return 42
}
Fill :: () -> [8]s64 {
    out: [8]s64
    for i: 0..7 { out[i] = i }
    return out
}
#program_export
Fault :: (which: s64) -> s64 {
    input: [64]u16
    t := Fill()
    if which == 1 { return Short(input[0:32], 8, 4) }
    if which == 2 { return Moved(t[0:4], 3) }
    if which == 3 { return Either(t[0:4], -1, 4) }
    if which == 4 { return Branch(t[0:4], 1, 4) }
    if which == 5 { return Other(t[0:8], 1, 8, 3) }
    huge: s64 = 6917529027641081856
    if which == 6 { return Wrapped(t[0:4], huge, huge) }
    if which == 7 { return Plane(t[0:4], 4611686018427387905, 3, 100) }
    if which == 8 { return Through(t[0:4], huge, 5) }
    if which == 9 { return Choice(t[0:4], huge, huge, true) }
    if which == 10 { return Limit(t[0:4], huge, huge) }
    levels: [64]u8
    if which == 11 { return Unbounded(levels[0:64], 32, 67108864, 0) }
    if which == 12 { return Reset(levels[0:64], 32, 67108864, 0) }
    if which == 13 { return Edge(levels[0:64], 4, 11, 3) }
    return 0
}
ZI
faults="Short Moved Either Branch Other Wrapped Plane Through Choice Limit Unbounded Reset Edge"
which=1
for name in $faults; do
    printf '#program_export\nRun%s :: () -> s64 { return Fault(%d) }\n' "$name" "$which" >> "$work/app.zi"
    which=$((which + 1))
done

"$ziran" ir --root "$work" -o "$work/ir" "$work/app.zi"
for input in source saved; do
    if test "$input" = source; then
        module=$work/app.zi
        root=$work
    else
        module=$work/ir/app.zir
        root=$work/ir
    fi
    "$ziran" bundle --root "$root" --entry app:Answer \
        -o "$work/$input.zx" "$module"
    test "$("$ziran" run "$work/$input.zx")" = 42
    test "$("$run_vm" run --vm "$work/$input.zx")" = 42
    for name in $faults; do
        "$ziran" bundle --root "$root" --entry "app:Run$name" \
            -o "$work/fault.zx" "$module"
        if timeout 60 "$run_vm" run --vm "$work/fault.zx" > /dev/null 2>&1 ||
           timeout 60 "$ziran" run "$work/fault.zx" > /dev/null 2>&1; then
            echo "$name read outside its slice on the VM or native" >&2
            exit 1
        fi
    done

    output=$work/c-$input
    "$ziran" build --target=c --root "$root" -o "$output" "$module"
    cat > "$output/main.c" <<'C'
#include <stdlib.h>
#include "app.h"
int main(int argc, char **argv) {
    if(argc > 1) return (int)Fault(atoi(argv[1]));
    return Answer() == 42 ? 0 : 1;
}
C
    "${CC:-cc}" -std=c99 -pedantic-errors -Wall -O2 \
        -I"$repo/include" -I"$output" "$output"/*.c -o "$output/app"
    "$output/app"
    which=1
    for name in $faults; do
        if "$output/app" "$which" 2> /dev/null; then
            echo "$name read outside its slice in C" >&2
            exit 1
        fi
        which=$((which + 1))
    done
    for name in Block Window Neighbor Inside Levels; do
        sed -n "/^app_$name(/,/^}/p" "$output/app.c" > "$work/proven.c"
        test -s "$work/proven.c"
        if grep -q 'ZIRAN_INDEX\|SliceIndex' "$work/proven.c"; then
            echo "a proven index in $name kept its bounds check" >&2
            grep 'ZIRAN_INDEX\|SliceIndex' "$work/proven.c" >&2
            exit 1
        fi
    done
    for name in $faults; do
        sed -n "/^app_$name(/,/^}/p" "$output/app.c" > "$work/kept.c"
        if ! grep -q 'ZIRAN_INDEX\|SliceIndex' "$work/kept.c"; then
            echo "$name lost a bounds check it needs" >&2
            exit 1
        fi
    done

    output=$work/go-$input
    "$ziran" build --target=go --pkg main --root "$root" -o "$output" "$module"
    cat > "$output/main.go" <<'GO'
package main
import ("os"; "strconv")
func main() {
    if len(os.Args) > 1 {
        which, _ := strconv.Atoi(os.Args[1])
        App_Fault(int64(which))
        return
    }
    if App_Answer() != 42 { panic("guarded index ranges") }
}
GO
    (cd "$output" && GO111MODULE=off go build -o app .)
    "$output/app"
    which=1
    for name in $faults; do
        if "$output/app" "$which" 2> /dev/null; then
            echo "$name read outside its slice in Go" >&2
            exit 1
        fi
        which=$((which + 1))
    done
done
cmp "$work/source.zx" "$work/saved.zx"
