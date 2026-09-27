#!/bin/sh
set -eu

ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/numeric.zi" <<'ZI'
#program_export
Answer :: () -> s32 {
    byte: u8 = cast(u8)1
    half: u16 = cast(u16)1
    negative_byte: s8 = cast(s8)(-1)
    negative_half: s16 = cast(s16)(-1)
    if (byte << cast(u8)7) != cast(u8)128 ||
       (half << cast(u16)15) != cast(u16)32768 ||
       (negative_byte >> cast(s8)1) != negative_byte ||
       (negative_half >> cast(s16)1) != negative_half { return 1 }
    if cast(u16)(65535.0) != cast(u16)65535 ||
       cast(s8)(-128.0) != cast(s8)(-128) { return 2 }
    one: float64 = 1.0
    zero: float64 = 0.0
    infinity: float64 = one / zero
    nan: float64 = zero / zero
    if infinity <= one || nan == nan { return 3 }
    small_one: float32 = 1.0
    small_zero: float32 = 0.0
    small_infinity: float32 = small_one / small_zero
    if small_infinity <= small_one { return 4 }
    signed_byte: s8 = cast(s8)127
    unsigned_byte: u8 = cast(u8)255
    signed_word: s32 = cast(s32)2147483647
    if signed_byte + cast(s8)1 != cast(s8)(-128) ||
       unsigned_byte + cast(u8)1 != cast(u8)0 ||
       cast(u8)0 - cast(u8)1 != unsigned_byte ||
       cast(s8)100 * cast(s8)3 != cast(s8)44 ||
       signed_word + cast(s32)1 != cast(s32)(-2147483647 - 1) {
        return 5
    }
    minimum_word: s32 = cast(s32)(-2147483647 - 1)
    minimum_long: s64 = cast(s64)(-9223372036854775807 - 1)
    minus_one_word: s32 = cast(s32)(-1)
    minus_one_long: s64 = cast(s64)(-1)
    if minimum_word / minus_one_word != minimum_word ||
       minimum_word % minus_one_word != cast(s32)0 ||
       minimum_long / minus_one_long != minimum_long ||
       minimum_long % minus_one_long != cast(s64)0 { return 6 }
    if cast(s8)(cast(u8)200) != cast(s8)(-56) ||
       cast(u8)(cast(s8)(-1)) != cast(u8)255 ||
       cast(s16)(cast(u16)40000) != cast(s16)(-25536) ||
       cast(u16)(cast(s16)(-1)) != cast(u16)65535 ||
       cast(s32)(cast(u32)4294967295) != cast(s32)(-1) ||
       cast(u32)(cast(s32)(-1)) != cast(u32)4294967295 ||
       cast(s64)(cast(u64)18446744073709551615) != cast(s64)(-1) ||
       cast(u64)(cast(s64)(-1)) != cast(u64)18446744073709551615 {
        return 7
    }
    return 42
}

#program_export
Bad8 :: () -> s32 {
    value: u8 = cast(u8)1
    amount: u8 = cast(u8)8
    return cast(s32)(value << amount)
}

#program_export
Bad16 :: () -> s32 {
    value: u16 = cast(u16)1
    amount: u16 = cast(u16)16
    return cast(s32)(value << amount)
}

#program_export
BadNegative :: () -> s32 {
    value: s16 = cast(s16)1
    amount: s16 = cast(s16)(-1)
    return cast(s32)(value << amount)
}
ZI

"$ziran" ir --root "$work" -o "$work/ir" "$work/numeric.zi"

for entry in Answer Bad8 Bad16 BadNegative; do
    for input in source saved; do
        if test "$input" = source; then
            root=$work
            module=$work/numeric.zi
        else
            root=$work/ir
            module=$work/ir/numeric.zir
        fi
        "$ziran" bundle --root "$root" --entry "numeric:$entry" \
            -o "$work/$input-$entry.zib" "$module"
    done
    cmp "$work/source-$entry.zib" "$work/saved-$entry.zib"
done

for input in source saved; do
    if test "$("$ziran" run "$work/$input-Answer.zib")" != 42; then
        echo 'portable numeric result differs from native targets' >&2
        exit 1
    fi
    for entry in Bad8 Bad16 BadNegative; do
        if "$ziran" run "$work/$input-$entry.zib" > "$work/vm-$entry.log" 2>&1; then
            echo "portable runner accepted an oversized $entry shift" >&2
            exit 1
        fi
    done
done

for input in source saved; do
    if test "$input" = source; then
        root=$work
        module=$work/numeric.zi
    else
        root=$work/ir
        module=$work/ir/numeric.zir
    fi
    for target in c cpp go; do
        output=$work/$target-$input
        if test "$target" = go; then
            "$ziran" build --target=go --pkg main --no-main --root "$root" \
                -o "$output" "$module"
            cat > "$output/main.go" <<'GO'
package main
import "os"
func main() {
    if len(os.Args) > 1 {
        if os.Args[1] == "8" { Numeric_Bad8(); return }
        if os.Args[1] == "negative" { Numeric_BadNegative(); return }
        Numeric_Bad16(); return
    }
    if Numeric_Answer() != 42 { panic("numeric result") }
}
GO
            GO111MODULE=off go build -o "$output/app" "$output/numeric.go" "$output/main.go"
        elif test "$target" = c; then
            "$ziran" build --target=c --no-main --root "$root" \
                -o "$output" "$module"
            cat > "$output/main.c" <<'C'
#include "numeric.h"
int main(int argc, char **argv) {
    if (argc > 1) {
        if (argv[1][0] == '8') return Bad8();
        if (argv[1][0] == 'n') return BadNegative();
        return Bad16();
    }
    return Answer() == 42 ? 0 : 1;
}
C
            "${CC:-cc}" -std=c99 -I"$repo/include" -I"$output" \
                "$output/numeric.c" "$output/main.c" -lm -o "$output/app"
        else
            "$ziran" build --target=cpp --no-main --root "$root" \
                -o "$output" "$module"
            cat > "$output/main.cpp" <<'CPP'
#include "numeric.hpp"
int main(int argc, char **argv) {
    if (argc > 1) {
        if (argv[1][0] == '8') return Bad8();
        if (argv[1][0] == 'n') return BadNegative();
        return Bad16();
    }
    return Answer() == 42 ? 0 : 1;
}
CPP
            "${CXX:-c++}" -std=c++17 -I"$repo/include" -I"$output" \
                "$output/numeric.cpp" "$output/main.cpp" -lm -o "$output/app"
        fi
        "$output/app"
        for amount in 8 16 negative; do
            if "$output/app" "$amount" > "$output/bad-$amount.log" 2>&1; then
                echo "$target accepted an oversized $amount-bit shift" >&2
                exit 1
            fi
        done
    done
done
