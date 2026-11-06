#!/bin/sh
# An if inside a loop adds to what the loop proves about its variable: a
# guard that continues or returns, such as if flag > 3 { continue } or
# if code != 0 { return code }, and an enclosing if, such as
# if i < 32 { ... }, keep i <= height - 1 from the loop, so generated C
# still leaves out the check of view[i * width + j]. The step a for loop
# runs before a continue does not reach the read, and a nearer, weaker bound
# such as i <= 31 does not hide the loop's own. Facts also chain: inside
# for j: 0..width - 1 width is at least 1, so stride >= width makes
# samples[i * stride + j] safe after samples.count < (height - 1) * stride +
# width returns. A local chosen by ifx, such as the row a flip picks, is
# proven for both choices, and a guard written as cast(s64)(height - 1)
# counts where the loop shows height - 1 did not wrap in s32. A guard of
# many || comparisons proves as much as the same comparisons split across
# ifs, however many unrelated ones come first. A bound that holds a
# value only a call gives, such as radius <= 2 for a box filter's radius,
# also scales: (2 - radius) * stride is never negative while stride is not.
# A write that
# does reach the read, a branch past the last row and a guard whose s32 sum
# may wrap keep their checks, and those reads still abort on C, Go, native
# and the portable VM from source and saved IR.
set -eu

ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
run_vm="$(dirname "$ziran")/zi2zx-vm"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
Plain :: (view: []s32, width: s32, height: s32) -> s64 {
    if width < 4 || width > 64 || height < 4 || height > 64 || view.count < cast(s64)height * width { return -1 }
    total: s64
    for i: 0..cast(s64)height - 1 { for j: 0..cast(s64)width - 1 { total += view[i * width + j] } }
    return total
}
// A guard on another value ends in continue.
Skip :: (view: []s32, width: s32, height: s32, flag: s64) -> s64 {
    if width < 4 || width > 64 || height < 4 || height > 64 || view.count < cast(s64)height * width { return -1 }
    total: s64
    for i: 0..cast(s64)height - 1 {
        if flag > 3 { continue }
        for j: 0..cast(s64)width - 1 { total += view[i * width + j] }
    }
    return total
}
// A guard on the loop variable gives a nearer bound that cannot prove it.
Rows :: (view: []s32, width: s32, height: s32) -> s64 {
    if width < 4 || width > 64 || height < 4 || height > 64 || view.count < cast(s64)height * width { return -1 }
    total: s64
    for i: 0..cast(s64)height - 1 {
        if i >= 32 { continue }
        for j: 0..cast(s64)width - 1 { total += view[i * width + j] }
    }
    return total
}
Inside :: (view: []s32, width: s32, height: s32) -> s64 {
    if width < 4 || width > 64 || height < 4 || height > 64 || view.count < cast(s64)height * width { return -1 }
    total: s64
    for i: 0..cast(s64)height - 1 {
        if i < 32 { for j: 0..cast(s64)width - 1 { total += view[i * width + j] } }
    }
    return total
}
Status :: (i: s64, bad: s64) -> s64 { if i == bad { return 7 } return 0 }
// A guard that returns an error code.
Decode :: (view: []s32, width: s32, height: s32, bad: s64) -> s64 {
    if width < 4 || width > 64 || height < 4 || height > 64 || view.count < cast(s64)height * width { return -1 }
    total: s64
    for i: 0..cast(s64)height - 1 {
        code := Status(i, bad)
        if code != 0 { return -code }
        for j: 0..cast(s64)width - 1 { total += view[i * width + j] }
    }
    return total
}
// A CDEF-style tap table read after a guard on another value.
Taps :: (strength: s64) -> s64 {
    thresholds: [12]s64
    for tap: 0..11 { thresholds[tap] = tap * 2 }
    total: s64
    for tap: 0..11 {
        offset := tap * strength
        if offset > 68 { continue }
        total += thresholds[tap]
    }
    return total
}
// AV1-style reconstruction: rows stride apart, residual packed.
Reconstruct :: (samples: []u16, stride: s32, residual: []s32, width: s32, height: s32) -> s64 {
    if stride < width || residual.count != cast(s64)width * height ||
        samples.count < (cast(s64)height - 1) * stride + width {
        return -1
    }
    total: s64
    for i: 0..cast(s64)height - 1 {
        for j: 0..cast(s64)width - 1 {
            total += cast(s64)samples[i * stride + j] + cast(s64)residual[i * width + j]
        }
    }
    return total
}
// Only the first sample of each row; width >= 1 still bounds stride.
Column :: (samples: []u16, stride: s32, width: s32, height: s32) -> s64 {
    if stride < width || samples.count < cast(s64)height * stride { return -1 }
    total: s64
    for i: 0..cast(s64)height - 1 { for j: 0..cast(s64)width - 1 { total += cast(s64)samples[i * stride] } }
    return total
}
// AV1-style flips with the guard's height - 1 in s32.
Flip :: (samples: []u16, stride: s32, residual: []s32, width: s32, height: s32,
         flip_rows: bool, flip_columns: bool) -> s64 {
    if stride < width || residual.count != cast(s64)width * height ||
        samples.count < cast(s64)(height - 1) * stride + width {
        return -1
    }
    total: s64
    for i: 0..cast(s64)height - 1 {
        yy := ifx flip_rows then cast(s64)height - 1 - i else i
        for j: 0..cast(s64)width - 1 {
            xx := ifx flip_columns then cast(s64)width - 1 - j else j
            at := yy * stride + xx
            total += cast(s64)samples[at] * (i + 1) + cast(s64)residual[i * width + j]
        }
    }
    return total
}
// Fourteen comparisons, the first four about values no index reads.
Box :: (input: []u16, stride: s32, width: s32, height: s32, output: []u16, output_stride: s32,
        gain: s32, bias: s32) -> s64 {
    if gain < 0 || gain > 255 || bias < -128 || bias > 127 ||
        width < 1 || width > 64 || height < 1 || height > 8 || stride < width + 6 || stride > 72 ||
        input.count < cast(s64)(height + 5) * stride + width + 6 || output_stride < width ||
        output_stride > 4096 || output.count < cast(s64)height * output_stride {
        return -1
    }
    total: s64
    for y: 0..height - 1 {
        for x: 0..width - 1 {
            output[y * output_stride + x] = input[(y + 3) * stride + x + 3]
            total += cast(s64)output[y * output_stride + x] * gain + bias
        }
    }
    return total
}
Radius :: (set: s32) -> s32 { return set & 3 }
// A box filter's window, 2 * radius + 1 rows of stride samples per row.
Window :: (input: []u16, stride: s32, width: s32, height: s32, set: s32) -> s64 {
    radius := Radius(set)
    if radius < 1 || radius > 2 { return -1 }
    if width < 1 || width > 64 || height < 1 || height > 8 || stride < width + 6 || stride > 72 { return -1 }
    if input.count < cast(s64)(height + 5) * stride + width + 6 { return -1 }
    total: s64
    columns := width + 2 + 2 * radius
    for iy: 0..height + 1 {
        top := (cast(s32)iy + 2 - radius) * stride + 2 - radius
        for k: 0..2 * radius {
            line := top + cast(s32)k * stride
            for j: 0..columns - 1 { total += cast(s64)input[line + cast(s32)j] }
        }
    }
    return total
}
// Radius 3 starts one sample before the slice; radius 2 one row past it.
Wider :: (input: []u16, stride: s32, width: s32, height: s32, set: s32) -> s64 {
    radius := Radius(set)
    if radius < 1 || radius > 3 { return -1 }
    if width < 1 || width > 64 || height < 1 || height > 8 || stride < width + 6 || stride > 72 { return -1 }
    if input.count < cast(s64)(height + 5) * stride + width + 6 { return -1 }
    total: s64
    columns := width + 2 + 2 * radius
    for iy: 0..height + 1 {
        top := (cast(s32)iy + 3 - radius) * stride + 2 - radius
        for k: 0..2 * radius {
            line := top + cast(s32)k * stride
            for j: 0..columns - 1 { total += cast(s64)input[line + cast(s32)j] }
        }
    }
    return total
}
// The flipped row starts one past the last.
Mirror :: (samples: []u16, stride: s32, width: s32, height: s32, flip_rows: bool) -> s64 {
    if stride < width || samples.count < cast(s64)(height - 1) * stride + width { return -1 }
    total: s64
    for i: 0..cast(s64)height - 1 {
        yy := ifx flip_rows then cast(s64)height - i else i
        for j: 0..cast(s64)width - 1 { total += cast(s64)samples[yy * stride + j] }
    }
    return total
}
// height + 1 wraps to the lowest s32 when height is the highest.
Wrap :: (samples: []u16, height: s32) -> s64 {
    if height < 0 || samples.count < cast(s64)(height + 1) { return -1 }
    return cast(s64)samples[cast(s64)height]
}
// Without the inner loop, stride 0 and an empty slice reach samples[0].
Flat :: (samples: []u16, stride: s32, height: s32) -> s64 {
    if stride < 0 || samples.count < cast(s64)height * stride { return -1 }
    total: s64
    for i: 0..cast(s64)height - 1 { total += cast(s64)samples[i * stride] }
    return total
}
// The write falls through to the read.
Bumped :: (view: []s64, n: s64) -> s64 {
    if n < 1 || view.count < n { return -1 }
    i: s64 = 0
    total: s64
    while i < n {
        if i == 2 { i += 1 }
        total += view[i]
        i += 1
    }
    return total
}
// The read lies between the write and the continue.
Late :: (view: []s64, n: s64, flag: s64) -> s64 {
    if n < 1 || view.count < n { return -1 }
    i: s64 = 0
    total: s64
    while i < n {
        if flag > 0 {
            i += 1
            total += view[i]
            continue
        }
        i += 1
    }
    return total
}
// Only a nested if continues; the write itself falls through.
Maybe :: (view: []s64, n: s64) -> s64 {
    if n < 1 || view.count < n { return -1 }
    i: s64 = 0
    total: s64
    while i < n {
        if i == 2 {
            i += 1
            if n > 100 { continue }
        }
        total += view[i]
        i += 1
    }
    return total
}
// Neither the guard's i <= 31 nor the loop's i <= height covers the read.
Weak :: (view: []s32, width: s32, height: s32) -> s64 {
    if width < 4 || width > 64 || height < 4 || height > 64 || view.count < cast(s64)height * width { return -1 }
    total: s64
    for i: 0..cast(s64)height {
        if i >= 32 { continue }
        for j: 0..cast(s64)width - 1 { total += view[i * width + j] }
    }
    return total
}
#program_export
Answer :: () -> s64 {
    v: [64]s32
    for i: 0..63 { v[i] = cast(s32)i }
    if Plain(v[:], 8, 8) != 2016 || Skip(v[:], 8, 8, 0) != 2016 || Skip(v[:], 8, 8, 4) != 0 { return 1 }
    if Rows(v[:], 8, 8) != 2016 || Inside(v[:], 8, 8) != 2016 { return 2 }
    if Decode(v[:], 8, 8, -1) != 2016 || Decode(v[:], 8, 8, 3) != -7 { return 3 }
    if Plain(v[0:63], 8, 8) != -1 || Rows(v[:], 8, 9) != -1 { return 4 }
    if Taps(10) != 42 || Taps(1) != 132 { return 5 }
    s: [40]u16
    for i: 0..39 { s[i] = cast(u16)i }
    r: [12]s32
    for i: 0..11 { r[i] = 1 }
    // Rows 0, 10 and 20, four samples each, plus twelve ones.
    if Reconstruct(s[0:24], 10, r[:], 4, 3) != 6 + 46 + 86 + 12 { return 7 }
    if Reconstruct(s[0:23], 10, r[:], 4, 3) != -1 || Column(s[:], 10, 4, 4) != 4 * 60 { return 8 }
    if Flat(s[:], 10, 4) != 60 { return 9 }
    // Rows weighted 1, 2 and 3 from the top, or from the bottom when flipped.
    if Flip(s[0:24], 10, r[:], 4, 3, false, false) != 368 || Flip(s[0:24], 10, r[:], 4, 3, true, true) != 208 ||
        Flip(s[0:23], 10, r[:], 4, 3, true, false) != -1 { return 10 }
    if Mirror(s[0:24], 10, 4, 3, false) != 138 || Wrap(s[:], 5) != 5 { return 11 }
    // Samples 27, 28, 35 and 36, doubled, plus one each.
    b: [64]u16
    for i: 0..63 { b[i] = cast(u16)i }
    o: [4]u16
    if Box(b[:], 8, 2, 2, o[:], 2, 2, 1) != 256 || o[3] != 36 ||
        Box(b[0:63], 8, 2, 2, o[:], 2, 2, 1) != -1 { return 12 }
    if Window(b[:], 8, 2, 2, 2) != 5040 || Window(b[:], 8, 2, 2, 3) != -1 ||
        Wider(b[:], 8, 2, 2, 1) != 2844 { return 13 }
    t: [4]s64
    t[0] = 1; t[1] = 2; t[2] = 3; t[3] = 4
    if Bumped(t[:], 2) != 3 || Late(t[:], 3, 1) != 9 || Late(t[:], 4, 0) != 0 || Maybe(t[:], 2) != 3 { return 6 }
    return 42
}
#program_export
Fault :: (which: s64) -> s64 {
    v: [64]s32
    t: [3]s64
    if which == 1 { return Bumped(t[:], 3) }
    if which == 2 { return Late(t[:], 3, 1) }
    if which == 3 { return Maybe(t[:], 3) }
    if which == 4 { return Weak(v[:], 8, 8) }
    e: [1]u16
    if which == 5 { return Flat(e[0:0], 0, 1) }
    s: [24]u16
    if which == 6 { return Mirror(s[:], 10, 4, 3, true) }
    if which == 7 { return Wrap(e[:], 2147483647) }
    w: [64]u16
    if which == 8 { return Wider(w[:], 8, 2, 2, 3) }
    return 0
}
ZI
faults="Bumped Late Maybe Weak Flat Mirror Wrap Wider"
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
    test "$("$run_vm" run "$work/$input.zx")" = 42
    for name in $faults; do
        "$ziran" bundle --root "$root" --entry "app:Run$name" \
            -o "$work/fault.zx" "$module"
        if timeout 60 "$run_vm" run "$work/fault.zx" > /dev/null 2>&1 ||
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
    for name in Plain Skip Rows Inside Decode Taps Reconstruct Column Flip Box Window; do
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
    if App_Answer() != 42 { panic("loop guard ranges") }
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
