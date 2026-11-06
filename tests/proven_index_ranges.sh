#!/bin/sh
# Generated C indexes arrays, slices and strings without a bounds check when
# the index provably lies inside them: a counter its loop condition bounds,
# a value its type or a mask, shift or remainder bounds. Every index the
# proof cannot cover keeps its check and still aborts on C, Go, native and
# the portable VM from source and saved IR.
set -eu

ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
run_vm="$(dirname "$ziran")/zi2zx-vm"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
N :: 8;
Opaque :: (value: s64) -> s64 { return value }
Bump :: (p: *s64) { p.* = 9 }
Fill :: () -> [8]s64 {
    out: [8]s64
    for i: 0..N-1 { out[i] = i + 1 }
    return out
}
Proven :: (t: [8]s64, bytes: [256]u8, view: []s64, text: string, x: u32, b: u8) -> s64 {
    r: s64 = 0
    for i: 0..7 { r += t[i] }
    for i: 0..N-1 { r += t[(i * 3) % 8] }
    k: s64 = 0
    while k < 8 && r > 0 { r += t[k]; k += 1 }
    r += t[x & 7] + t[x >> 29] + cast(s64) bytes[b] + cast(s64) bytes[x >> 24]
    for i: 0..view.count-1 { r += view[i]; view[i] = 0 }
    for v: view { r += v }
    j: s64 = 0
    while j < view.count { r += view[j]; j += 1 }
    for i: 0..text.count-1 { r += cast(s64) text[i] }
    {
        // A shadowing counter in an inner block is its own binding.
        k := 3
        r += t[k]
    }
    return r
}
// The counter moves before the read.
Early :: () -> s64 {
    t := Fill()
    k: s64 = 0
    r: s64 = 0
    while k < 8 { k += 1; r += t[k] }
    return r
}
Inclusive :: () -> s64 {
    t := Fill()
    k: s64 = 0
    r: s64 = 0
    while k <= 8 { r += t[k]; k += 1 }
    return r
}
// The inner loop moves the outer loop's counter between reads.
Nested :: () -> s64 {
    t := Fill()
    k: s64 = 0
    r: s64 = 0
    while k < 8 {
        m: s64 = 0
        while m < 3 { r += t[k]; k += 4; m += 1 }
    }
    return r
}
Address :: () -> s64 {
    t := Fill()
    k: s64 = 0
    r: s64 = 0
    while k < 8 { Bump(*k); r += t[k]; k += 1 }
    return r
}
Rebound :: () -> s64 {
    long := Fill()
    short := Fill()
    view: []s64 = long[0:8]
    i: s64 = 0
    r: s64 = 0
    while i < view.count {
        if i == 3 { view = short[0:2] }
        r += view[i]
        i += 1
    }
    return r
}
Mask :: () -> s64 { t := Fill(); return t[Opaque(9) & 15] }
Remainder :: () -> s64 { t := Fill(); return t[Opaque(-3) % 8] }
Shift :: () -> s64 { t := Fill(); return t[Opaque(-32) >> 2] }
Shadow :: () -> s64 {
    t := Fill()
    r: s64 = 0
    for i: 0..7 {
        {
            // The read names the inner binding, not the bounded counter.
            i := Opaque(9)
            r += t[i]
        }
    }
    return r
}
Past :: () -> s64 {
    text := "abc"
    r: s64 = 0
    for i: 0..text.count { r += cast(s64) text[i] }
    return r
}
#program_export
Answer :: () -> s64 {
    t := Fill()
    bytes: [256]u8
    for i: 0..255 { bytes[i] = cast(u8) (i & 3) }
    values: [3]s64
    values[0] = 1
    values[1] = 2
    values[2] = 3
    // Three passes over t, the masked and shifted reads, the view's sum
    // before it is cleared, "abc" and t[3].
    got := Proven(t, bytes, values[0:3], "abc", 0x01000000, 7)
    if got != 36 + 36 + 36 + 6 + 6 + 294 + 4 { return got }
    return 42
}
#program_export
Fault :: (which: s64) -> s64 {
    if which == 1 { return Early() }
    if which == 2 { return Inclusive() }
    if which == 3 { return Nested() }
    if which == 4 { return Address() }
    if which == 5 { return Rebound() }
    if which == 6 { return Mask() }
    if which == 7 { return Remainder() }
    if which == 8 { return Shift() }
    if which == 9 { return Shadow() }
    if which == 10 { return Past() }
    return 0
}
ZI
faults="Early Inclusive Nested Address Rebound Mask Remainder Shift Shadow Past"
for name in $faults; do
    printf '#program_export\nRun%s :: () -> s64 { return %s() }\n' "$name" "$name" >> "$work/app.zi"
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
            echo "$name read outside its array on the VM or native" >&2
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
            echo "$name read outside its array in C" >&2
            exit 1
        fi
        which=$((which + 1))
    done
    sed -n '/^app_Proven(/,/^}/p' "$output/app.c" > "$work/proven.c"
    grep -q 'view.data)\[i\]' "$work/proven.c"
    grep -q '(uint8_t)text.data\[i\]' "$work/proven.c"
    if grep -q 'ZIRAN_INDEX\|SliceIndex' "$work/proven.c"; then
        echo 'a proven index kept its bounds check' >&2
        grep 'ZIRAN_INDEX\|SliceIndex' "$work/proven.c" >&2
        exit 1
    fi
    sed -n '/^app_Fill(/,/^}/p' "$output/app.c" > "$work/fill.c"
    if grep -q 'ZIRAN_INDEX' "$work/fill.c"; then
        echo 'a proven store kept its bounds check' >&2
        exit 1
    fi
    for name in $faults; do
        sed -n "/^app_$name(/,/^}/p" "$output/app.c" > "$work/kept.c"
        grep -q 'ZIRAN_INDEX\|SliceIndex' "$work/kept.c"
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
    if App_Answer() != 42 { panic("proven index ranges") }
}
GO
    (cd "$output" && GO111MODULE=off go build -o app .)
    "$output/app"
    which=1
    for name in $faults; do
        if "$output/app" "$which" 2> /dev/null; then
            echo "$name read outside its array in Go" >&2
            exit 1
        fi
        which=$((which + 1))
    done
done
cmp "$work/source.zx" "$work/saved.zx"
