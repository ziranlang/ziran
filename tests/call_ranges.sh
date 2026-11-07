#!/bin/sh
# Generated C proves accesses through calls. A call's result lies in what
# the callee's return statements give with each parameter in its argument's
# range, so table[Small(n)] and table[Pass(i)] for i: 0..3 need no check. An
# access in a procedure no other module can call needs none when every call
# keeps it inside its memory: Load4(view, 0) after view.count < 8 returns
# first, Get(view, at) under the caller's guard. Results that may leave the
# table, an increment that may wrap, a caller passing one lane too few or
# an empty, short or one-lane-short view, a public helper, one used as a
# value, and an argument read after a call that may change it keep their
# checks and still trap on C, Go, native and the portable VM, from source
# and saved IR.
set -eu

ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
run_vm="$(dirname "$ziran")/zi2zx"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
Simd :: #import "std/simd"

Box :: struct { at: s64; }

// Return ranges. Proven: x & 3 is 0..3; Pass returns what it is passed.
Small :: (x: s32) -> s32 { return x & 3 }
Pass :: (x: s32) -> s32 { return x }
Inc :: (x: s32) -> s32 { return x + 1 }
Table :: (n: s32) -> s32 {
    table: [4]s32
    for i: 0..3 { table[i] = cast(s32)i * 10 }
    total := table[Small(n)]
    for i: 0..3 { total += table[Pass(cast(s32)i)] }
    for i: 0..2 { total += table[Inc(cast(s32)i)] }
    return total
}
// Kept: n itself may be anything, and n + 1 may wrap.
Wide :: (n: s32) -> s32 {
    table: [4]s32
    return table[Pass(n)]
}
Wrapped :: (n: s32) -> s32 {
    table: [4]s32
    return table[Inc(n)]
}

// Slice ranges. Proven: Clamp returns 0..4, so 2 + 5 * Clamp(x) leaves five
// entries of the 32; the guard leaves three of the view; the open range
// starts inside it.
Cdfs :: struct { values: [32]u16; }
Clamp :: (x: s32) -> s32 {
    v := x & 7
    if v > 4 { v = 4 }
    return v
}
Window :: (cdfs: *Cdfs, x: s32) -> s64 {
    at := 2 + Clamp(x) * 5
    view := cdfs.values[at:at + 5]
    return view.count + Head(view)
}
Head :: (view: []u16) -> s64 {
    if view.count < 1 { return -1 }
    return cast(s64)view[0]
}
Three :: (view: []u16, at: s64) -> s64 {
    if at < 0 || at > view.count - 3 { return -1 }
    part := view[at:at + 3]
    return part.count
}
Rest :: (view: []u16, at: s64) -> s64 {
    if at < 0 || at >= view.count { return -1 }
    part := view[at:]
    return part.count
}
// Kept: 8 * Clamp(x) reaches 32, five past the end; at + 3 may wrap s32; the
// guard lets a two-entry tail through.
Past :: (cdfs: *Cdfs, x: s32) -> s64 {
    at := Clamp(x) * 8
    view := cdfs.values[at:at + 5]
    return view.count
}
Narrow :: (view: []u16, at: s32) -> s64 {
    if at < 0 || cast(s64)at > view.count - 3 { return -1 }
    part := view[at:at + 3]
    return part.count
}
// Kept: unsigned addition can wrap to a nonnegative endpoint below at.
Narrow8 :: (view: []u16, at: u8) -> s64 {
    if cast(s64)at > view.count - 3 { return -1 }
    part := view[at:at + 3]
    return part.count
}
Fixed8 :: (at: u8) -> s64 {
    wide: [300]u16
    part := wide[at:at + 3]
    return part.count
}
Short3 :: (view: []u16, at: s64) -> s64 {
    if at < 0 || at > view.count - 2 { return -1 }
    part := view[at:at + 3]
    return part.count
}

#scope_file
// Proven: every call passes a view of at least eight lanes.
Load4 :: (view: []s32, at: s64) -> s64 {
    return cast(s64)Simd.ReduceAdd(Simd.Load(s32x4, view, at))
}
// Proven: every call keeps at inside the view.
Get :: (view: []s32, at: s64) -> s32 { return view[at] }
// Kept: Short lets a three-lane view through.
LoadShort :: (view: []s32) -> s64 {
    return cast(s64)Simd.ReduceAdd(Simd.Load(s32x4, view, 0))
}
// Kept: Late reads past the end.
GetLate :: (view: []s32, at: s64) -> s32 { return view[at] }
// Kept: Moved reads box.at after Bump changed it.
GetMoved :: (view: []s32, at: s64) -> s32 { return view[at] }
// Kept: a procedure value may be called with anything.
GetValue :: (view: []s32, at: s64) -> s32 { return view[at] }
Bump :: (box: *Box) -> s32 { box.at += 100; return 0 }

#scope_export
// Kept: other modules may call it.
GetPublic :: (view: []s32, at: s64) -> s32 { return view[at] }

Sum :: (view: []s32) -> s64 {
    if view.count < 8 { return -1 }
    return Load4(view, 0) + Load4(view, 4)
}
Pick :: (view: []s32, at: s64) -> s32 {
    if at < 0 || at >= view.count { return -1 }
    return Get(view, at) + GetPublic(view, at)
}
Short :: (view: []s32) -> s64 {
    if view.count < 3 { return -1 }
    return LoadShort(view)
}
Late :: (view: []s32, at: s64) -> s32 {
    if at < 0 || at >= view.count { return -1 }
    return GetLate(view, at + 1)
}
Moved :: (view: []s32, box: *Box) -> s32 {
    if box.at < 0 || box.at >= view.count { return -1 }
    return Bump(box) + GetMoved(view, box.at)
}
Reader :: #type (view: []s32, at: s64) -> s32;
Through :: (view: []s32, at: s64) -> s32 {
    read: Reader = GetValue
    if at < 0 || at >= view.count { return -1 }
    return read(view, at)
}

#program_export
Answer :: () -> s64 {
    data: [8]s32
    for i: 0..7 { data[i] = cast(s32)i + 1 }
    view := data[:]
    if Table(6) != 20 + 60 + 60 { return 1 }
    if Wide(3) != 0 || Wrapped(2) != 0 { return 2 }
    // Eight lanes, one short, shorter and empty.
    if Sum(view) != 36 || Sum(data[0:7]) != -1 || Sum(data[0:4]) != -1 || Sum(data[0:0]) != -1 { return 3 }
    if Pick(view, 7) != 16 || Pick(view, 8) != -1 || Pick(data[0:0], 0) != -1 { return 4 }
    if Short(data[0:4]) != 10 || Short(data[0:2]) != -1 || Short(data[0:0]) != -1 { return 5 }
    if Late(view, 6) != 8 || Late(view, -1) != -1 { return 6 }
    box: Box
    box.at = 8
    if Moved(view, *box) != -1 { return 7 }
    if Through(view, 3) != 4 { return 8 }
    cdfs: Cdfs
    for i: 0..31 { cdfs.values[i] = cast(u16)i }
    entries := cdfs.values[:]
    if Window(*cdfs, 3) != 5 + 17 || Window(*cdfs, 6) != 5 + 22 || Past(*cdfs, 3) != 5 { return 9 }
    // Three entries, one short, and empty.
    if Three(entries, 29) != 3 || Three(entries, 30) != -1 || Three(entries[0:3], 0) != 3 ||
        Three(entries[0:2], 0) != -1 || Three(entries[0:0], 0) != -1 {
        return 10
    }
    if Rest(entries, 31) != 1 || Rest(entries, 32) != -1 || Rest(entries[0:0], 0) != -1 { return 11 }
    if Narrow(entries, 29) != 3 || Narrow8(entries, 29) != 3 ||
       Fixed8(29) != 3 || Short3(entries, 29) != 3 || Short3(entries[0:0], 0) != -1 { return 12 }
    return 42
}
#program_export
Fault :: (which: s64) -> s64 {
    data: [8]s32
    view := data[:]
    box: Box
    box.at = 2
    if which == 1 { return cast(s64)Wide(4) }
    if which == 2 { return cast(s64)Wrapped(2147483647) }
    if which == 3 { return Short(data[0:3]) }
    if which == 4 { return cast(s64)Late(view, 7) }
    if which == 5 { return cast(s64)Moved(view, *box) }
    cdfs: Cdfs
    if which == 6 { return Past(*cdfs, 4) }
    if which == 7 { return Short3(cdfs.values[0:2], 0) }
    if which == 8 {
        wide: [300]u16
        return Narrow8(wide[:], 254)
    }
    if which == 9 { return Fixed8(254) }
    return 0
}
ZI
faults="Wide Wrapped Short Late Moved Past Short3 Narrow8 Fixed8"
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
            echo "$name read outside its memory on the VM or native" >&2
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
            echo "$name read outside its memory in C" >&2
            exit 1
        fi
        which=$((which + 1))
    done
    checks='zi_simd_at\|SliceIndex\|ZIRAN_INDEX\|SliceRange('
    for name in Table Load4 Get Window Three Rest; do
        sed -n "/^app_$name(/,/^}/p" "$output/app.c" > "$work/proven.c"
        test -s "$work/proven.c"
        if grep -q "$checks" "$work/proven.c"; then
            echo "a proven access in $name kept its check" >&2
            grep "$checks" "$work/proven.c" >&2
            exit 1
        fi
    done
    for name in Wide Wrapped LoadShort GetLate GetMoved GetValue GetPublic Past Narrow Narrow8 Fixed8 Short3; do
        sed -n "/^app_$name(/,/^}/p" "$output/app.c" > "$work/kept.c"
        if ! grep -q "$checks" "$work/kept.c"; then
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
    if App_Answer() != 42 { panic("call ranges") }
}
GO
    (cd "$output" && GO111MODULE=off go build -o app .)
    "$output/app"
    which=1
    for name in $faults; do
        if "$output/app" "$which" 2> /dev/null; then
            echo "$name read outside its memory in Go" >&2
            exit 1
        fi
        which=$((which + 1))
    done
done
cmp "$work/source.zx" "$work/saved.zx"
echo "call_ranges: accesses proven through calls drop their checks; the rest trap on C, Go, native and the VM"
