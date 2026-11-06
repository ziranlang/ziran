#!/bin/sh
# Division, remainder and shifts whose operands are not constants compile in
# C to a checked helper of their own width, not the general IntegerOp switch;
# Go shifts in the operand's own type after checking the count instead of
# calling integerOp. Both keep the language's results on C, Go and the portable VM: truncating
# division, MIN / -1 wrapping to MIN, arithmetic right shifts, and an abort
# for a zero divisor or a shift count outside 0..width-1.
set -eu

ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
run_vm="$(dirname "$ziran")/zi2zx-vm"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

for t in s8 s16 s32 s64 u8 u16 u32 u64; do
    T=$(printf '%s' "$t" | tr a-z A-Z)
    cat <<ZI
Div$T :: (a: $t, b: $t) -> $t { return a / b }
Rem$T :: (a: $t, b: $t) -> $t { return a % b }
Shl$T :: (a: $t, b: $t) -> $t { return a << b }
Shr$T :: (a: $t, b: $t) -> $t { return a >> b }
ZI
done > "$work/app.zi"
cat >> "$work/app.zi" <<'ZI'
Sar :: (a: s32) -> s32 { return a >> 6 }
Sar64 :: (a: s64) -> s64 { return a >> 63 }
#program_export
Answer :: () -> s64 {
    min32: s32 = -2147483647 - 1
    min64: s64 = -9223372036854775807 - 1
    if DivS8(-128, -1) != -128 || RemS8(-128, -1) != 0 || DivS8(-7, 2) != -3 || RemS8(-7, 2) != -1 { return 1 }
    if ShlS8(1, 7) != -128 || ShrS8(-128, 7) != -1 || ShrS8(-7, 1) != -4 || ShlS8(-1, 0) != -1 { return 2 }
    if DivS16(-32768, -1) != -32768 || ShlS16(3, 14) != -16384 || ShrS16(-32768, 15) != -1 || RemS16(-9, 4) != -1 { return 3 }
    if DivS32(min32, -1) != min32 || RemS32(min32, -1) != 0 || DivS32(-7, 2) != -3 || RemS32(7, -2) != 1 { return 4 }
    if ShlS32(-1, 31) != min32 || ShrS32(-1, 31) != -1 || ShrS32(min32, 4) != -134217728 || Sar(-100) != -2 || Sar(100) != 1 { return 5 }
    if DivS64(min64, -1) != min64 || RemS64(min64, -1) != 0 || DivS64(-9, 4) != -2 || ShlS64(1, 63) != min64 { return 6 }
    if ShrS64(min64, 63) != -1 || Sar64(-5) != -1 || Sar64(5) != 0 || ShrS64(-1000, 3) != -125 { return 7 }
    if DivU8(200, 7) != 28 || RemU8(200, 7) != 4 || ShlU8(255, 1) != 254 || ShrU8(255, 7) != 1 { return 8 }
    if DivU16(65535, 2) != 32767 || ShlU16(65535, 15) != 32768 || ShrU16(65535, 15) != 1 { return 9 }
    top: u32 = 4294967295
    if DivU32(top, 3) != 1431655765 || RemU32(top, 10) != 5 || ShlU32(top, 31) != 2147483648 || ShrU32(top, 31) != 1 { return 10 }
    big: u64 = 1
    big = big << 63
    if DivU64(big, 2) != big >> 1 || RemU64(big + 5, 8) != 5 || ShlU64(3, 63) != big || ShrU64(big, 63) != 1 { return 11 }
    return 42
}
#program_export
Fail :: (which: s64) -> s64 {
    if which == 1 { return cast(s64) DivS32(1, 0) }
    if which == 2 { return cast(s64) RemU64(1, 0) }
    if which == 3 { return cast(s64) ShlS32(1, 32) }
    if which == 4 { return cast(s64) ShrS32(1, -1) }
    if which == 5 { return cast(s64) ShrS8(1, 8) }
    if which == 6 { return cast(s64) ShlU64(1, 64) }
    return cast(s64) DivU8(1, 0)
}
FailZeroDivide :: () -> s64 { return Fail(1) }
FailZeroRemainder :: () -> s64 { return Fail(2) }
FailWideShift :: () -> s64 { return Fail(3) }
FailNegativeShift :: () -> s64 { return Fail(4) }
FailByteShift :: () -> s64 { return Fail(5) }
FailLongShift :: () -> s64 { return Fail(6) }
ZI

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
    for name in ZeroDivide ZeroRemainder WideShift NegativeShift ByteShift LongShift; do
        "$ziran" bundle --root "$root" --entry "app:Fail$name" \
            -o "$work/$name-$input.zx" "$module"
        if "$run_vm" run "$work/$name-$input.zx" > /dev/null 2>&1; then
            echo "$name did not abort on the VM" >&2
            exit 1
        fi
        if "$ziran" run "$work/$name-$input.zx" > /dev/null 2>&1; then
            echo "$name did not abort natively" >&2
            exit 1
        fi
    done

    output=$work/c-$input
    "$ziran" build --target=c --root "$root" -o "$output" "$module"
    cat > "$output/main.c" <<'C'
#include <stdlib.h>
#include "app.h"
int main(int argc, char **argv) {
    if(argc > 1) return (int)Fail(atoi(argv[1]));
    return Answer() == 42 ? 0 : 1;
}
C
    "${CC:-cc}" -std=c99 -pedantic-errors -O2 \
        -I"$repo/include" -I"$output" "$output"/*.c -o "$output/app"
    "$output/app"
    for which in 1 2 3 4 5 6 7; do
        if "$output/app" "$which" 2> /dev/null; then
            echo "C case $which did not abort" >&2
            exit 1
        fi
    done
    if grep -q 'IntegerOp(' "$output/app.c"; then
        echo 'a division or shift still went through IntegerOp' >&2
        exit 1
    fi
    sed -n '/^app_Sar(/,/^}/p' "$output/app.c" | grep -q 'ZiShrS32(a, (uint64_t)(6), 32)'
    sed -n '/^app_DivS64(/,/^}/p' "$output/app.c" | grep -q 'ZiDivS64(a, b, INT64_MIN)'
    sed -n '/^app_ShlS8(/,/^}/p' "$output/app.c" | grep -q '(int8_t)ZiShl32(a, (uint64_t)(b), 8)'

    output=$work/go-$input
    "$ziran" build --target=go --pkg main --root "$root" -o "$output" "$module"
    cat > "$output/main.go" <<'GO'
package main
import "os"
func main() {
    if len(os.Args) > 1 {
        which := int64(os.Args[1][0] - '0')
        App_Fail(which)
        return
    }
    if App_Answer() != 42 { panic("checked integer operations") }
}
GO
    (cd "$output" && GO111MODULE=off go build -o app .)
    "$output/app"
    for which in 1 2 3 4 5 6 7; do
        if "$output/app" "$which" 2> /dev/null; then
            echo "Go case $which did not abort" >&2
            exit 1
        fi
    done
    if grep -q 'integerOp(' "$output/app.go"; then
        echo 'a Go shift still went through integerOp' >&2
        exit 1
    fi
    sed -n '/^func App_ShlS8(/,/^}/p' "$output/app.go" | grep -q 'a << shiftCount(uint64(b), 8)'
    sed -n '/^func App_ShrU64(/,/^}/p' "$output/app.go" | grep -q 'a >> shiftCount(uint64(b), 64)'
done
cmp "$work/source.zx" "$work/saved.zx"
