#!/bin/sh
# Generated C leaves out the check of a SIMD load or store when the range
# facts prove every lane it touches lies inside its array or slice: rows of
# a fixed array in a counted loop, a guard such as at > view.count - 4 that
# returns first, LoadLow's two s32 lanes, and a CDEF-style tap loop whose
# rows a break bounds and whose table offsets a continue bounds. Runs one
# lane too long, a guard sum that may wrap, and slices shorter than the
# vector, empty or one lane short, keep the check and still trap on C, Go,
# native and the portable VM, from source and saved IR.
set -eu

ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
run_vm="$(dirname "$ziran")/zi2zx"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
Simd :: #import "std/simd"

Block :: struct { samples: [64]s32; }

// Proven: sixteen rows of four lanes fill the 64-element array.
Rows :: (block: *Block) -> s64 {
    total: s64 = 0
    for i: 0..15 { total += cast(s64)Simd.ReduceAdd(Simd.Load(s32x4, block.samples[:], i * 4)) }
    return total
}
// Proven: the guard keeps at and the three lanes after it in the view;
// view.count - 4 cannot wrap, and is negative for a shorter view.
Guarded :: (view: []s32, at: s64) -> s64 {
    if at < 0 || at > view.count - 4 { return -1 }
    return cast(s64)Simd.ReduceAdd(Simd.Load(s32x4, view, at))
}
// Proven: LoadLow touches two lanes.
Low :: (view: []s32, at: s64) -> s64 {
    if at < 0 || at > view.count - 2 { return -1 }
    return cast(s64)Simd.ReduceAdd(Simd.LoadLow(s32x4, view, at))
}
// Proven: a store under the same guard.
Put :: (out: []s32, at: s64, x: s32) -> s64 {
    if at < 0 || at > out.count - 4 { return -1 }
    Simd.Store(out, at, Simd.Splat(s32x4, x))
    return 0
}
// Proven: rows a break bounds, offsets from a table a continue bounds; the
// last load reads samples 60..63.
Taps :: (block: *Block, offsets: []s64, wide: bool) -> s64 {
    rows := ifx wide then 2 else 1
    total: s64 = 0
    for pass: 0..7 {
        first := pass * rows
        if first > 3 { break }
        line := first * 16
        for tap: 0..offsets.count - 1 {
            offset := offsets[tap]
            if offset < 0 || offset > 12 { continue }
            total += cast(s64)Simd.ReduceAdd(Simd.Load(s32x4, block.samples[:], line + offset))
        }
    }
    return total
}

// Kept: the guard lets at = count - 3 through, one lane too many.
Off :: (view: []s32, at: s64) -> s64 {
    if at < 0 || at > view.count - 3 { return -1 }
    return cast(s64)Simd.ReduceAdd(Simd.Load(s32x4, view, at))
}
// Kept: at + 4 may wrap past the s64 range and pass the guard.
Wrap :: (view: []s32, at: s64) -> s64 {
    if at < 0 || at + 4 > view.count { return -1 }
    return cast(s64)Simd.ReduceAdd(Simd.Load(s32x4, view, at))
}
// Kept: nothing bounds the view's count.
First :: (view: []s32) -> s64 {
    return cast(s64)Simd.ReduceAdd(Simd.Load(s32x4, view, 0))
}
// Kept: row 16 starts at the array's end.
Past :: (block: *Block) -> s64 {
    total: s64 = 0
    for i: 0..16 { total += cast(s64)Simd.ReduceAdd(Simd.Load(s32x4, block.samples[:], i * 4)) }
    return total
}
// Kept: LoadLow at the last element touches one past it.
LowOff :: (view: []s32, at: s64) -> s64 {
    if at < 0 || at > view.count - 1 { return -1 }
    return cast(s64)Simd.ReduceAdd(Simd.LoadLow(s32x4, view, at))
}
// Kept: a store one lane too long.
PutOff :: (out: []s32, at: s64) -> s64 {
    if at < 0 || at > out.count - 3 { return -1 }
    Simd.Store(out, at, Simd.Splat(s32x4, 1))
    return 0
}

Filled :: () -> Block {
    block: Block
    for i: 0..63 { block.samples[i] = cast(s32)i }
    return block
}
#program_export
Answer :: () -> s64 {
    block := Filled()
    data: [16]s32
    for i: 0..15 { data[i] = cast(s32)i + 1 }
    view := data[:]
    if Rows(*block) != 2016 { return 1 }
    if Guarded(view, 12) != 13 + 14 + 15 + 16 || Guarded(view, 13) != -1 || Guarded(view, -1) != -1 { return 2 }
    // Exactly one vector, one lane short, shorter and empty.
    if Guarded(data[0:4], 0) != 10 || Guarded(data[0:3], 0) != -1 || Guarded(data[0:2], 0) != -1 ||
        Guarded(data[0:0], 0) != -1 {
        return 3
    }
    if Low(view, 14) != 15 + 16 || Low(view, 15) != -1 || Low(data[0:1], 0) != -1 { return 4 }
    out: [8]s32
    if Put(out[:], 4, 7) != 0 || Put(out[:], 5, 1) != -1 || Put(out[0:3], 0, 1) != -1 { return 5 }
    total: s64 = 0
    for i: 0..7 { total += cast(s64)out[i] }
    if total != 28 { return 6 }
    offsets: [5]s64
    offsets[0] = 0; offsets[1] = 12; offsets[2] = 13; offsets[3] = -1; offsets[4] = 4
    if Taps(*block, offsets[:], true) != 548 || Taps(*block, offsets[:], false) != 1480 { return 7 }
    if Off(view, 12) != 58 || Wrap(view, 12) != 58 || First(view) != 10 || LowOff(view, 14) != 31 { return 8 }
    return 42
}
#program_export
Fault :: (which: s64) -> s64 {
    block := Filled()
    data: [16]s32
    view := data[:]
    out: [8]s32
    if which == 1 { return Off(view, 13) }
    if which == 2 { return Wrap(view, 9223372036854775806) }
    if which == 3 { return First(data[0:3]) }
    if which == 4 { return First(data[0:2]) }
    if which == 5 { return First(data[0:0]) }
    if which == 6 { return Past(*block) }
    if which == 7 { return LowOff(view, 15) }
    if which == 8 { return PutOff(out[:], 5) }
    return 0
}
ZI
faults="Off Wrap Short Shorter Empty Past LowOff PutOff"
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
    "$ziran" bundle --root "$root" --entry app:Answer -o "$work/$input.zx" "$module"
    test "$("$ziran" run "$work/$input.zx")" = 42
    test "$("$run_vm" run --vm "$work/$input.zx")" = 42
    for name in $faults; do
        "$ziran" bundle --root "$root" --entry "app:Run$name" -o "$work/fault.zx" "$module"
        if timeout 60 "$run_vm" run --vm "$work/fault.zx" > /dev/null 2>&1 ||
           timeout 60 "$ziran" run "$work/fault.zx" > /dev/null 2>&1; then
            echo "$name touched lanes outside its memory on the VM or native" >&2
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
    "${CC:-cc}" -std=c11 -Wall -O2 -I"$repo/include" -I"$output" "$output"/*.c -o "$output/app"
    "$output/app"
    which=1
    for name in $faults; do
        if "$output/app" "$which" 2> /dev/null; then
            echo "$name touched lanes outside its memory in C" >&2
            exit 1
        fi
        which=$((which + 1))
    done
    for name in Rows Guarded Low Put Taps; do
        sed -n "/^app_$name(/,/^}/p" "$output/app.c" > "$work/proven.c"
        test -s "$work/proven.c"
        if grep -q 'zi_simd_at' "$work/proven.c"; then
            echo "a proven SIMD access in $name kept its check" >&2
            grep 'zi_simd_at' "$work/proven.c" >&2
            exit 1
        fi
    done
    for name in Off Wrap First Past LowOff PutOff; do
        sed -n "/^app_$name(/,/^}/p" "$output/app.c" > "$work/kept.c"
        if ! grep -q 'zi_simd_at' "$work/kept.c"; then
            echo "$name lost a SIMD bounds check it needs" >&2
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
    if App_Answer() != 42 { panic("SIMD run ranges") }
}
GO
    (cd "$output" && GO111MODULE=off go build -o app .)
    "$output/app"
    which=1
    for name in $faults; do
        if "$output/app" "$which" 2> /dev/null; then
            echo "$name touched lanes outside its memory in Go" >&2
            exit 1
        fi
        which=$((which + 1))
    done
done
cmp "$work/source.zx" "$work/saved.zx"
echo "simd_run_ranges: proven SIMD runs drop their checks; the rest trap on C, Go, native and the VM"
