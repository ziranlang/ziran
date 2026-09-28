#!/bin/sh
set -eu

ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/numeric.zi" <<'ZI'
// conformance: numeric.compile.div_min.s32
COMPILE_MIN_WORD :: #run cast(s32)(-2147483647 - 1) / cast(s32)(-1);
// conformance: numeric.compile.div_min.s64
COMPILE_MIN_LONG :: #run cast(s64)(-9223372036854775807 - 1) / cast(s64)(-1);
// conformance: numeric.compile.cast_wrap.u8
COMPILE_WRAPPED_BYTE :: #run cast(s8)(cast(u8)200);
// conformance: numeric.compile.wrap_add.u8
COMPILE_WRAPPED_ADD :: #run cast(u8)200 + cast(u8)100;
// conformance: numeric.compile.wrap_add.s8
COMPILE_SIGNED_ADD :: #run cast(s8)127 + cast(s8)1;
#assert COMPILE_MIN_WORD == cast(s32)(-2147483647 - 1)
#assert COMPILE_MIN_LONG == cast(s64)(-9223372036854775807 - 1)
#assert COMPILE_WRAPPED_BYTE == cast(s8)(-56)
#assert COMPILE_WRAPPED_ADD == cast(u8)44
#assert COMPILE_SIGNED_ADD == cast(s8)(-128)

#program_export
Answer :: () -> s32 {
    byte: u8 = cast(u8)1
    half: u16 = cast(u16)1
    negative_byte: s8 = cast(s8)(-1)
    negative_half: s16 = cast(s16)(-1)
    // conformance: numeric.shift.left.u8
    // conformance: numeric.shift.left.u16
    // conformance: numeric.shift.right.arithmetic.s8
    // conformance: numeric.shift.right.arithmetic.s16
    if (byte << cast(u8)7) != cast(u8)128 ||
       (half << cast(u16)15) != cast(u16)32768 ||
       (negative_byte >> cast(s8)1) != negative_byte ||
       (negative_half >> cast(s16)1) != negative_half { return 1 }
    // conformance: numeric.float.cast.u16
    // conformance: numeric.float.cast.s8
    if cast(u16)(65535.0) != cast(u16)65535 ||
       cast(s8)(-128.0) != cast(s8)(-128) { return 2 }
    one: float64 = 1.0
    zero: float64 = 0.0
    infinity: float64 = one / zero
    nan: float64 = zero / zero
    // conformance: numeric.float.divide.zero.f64
    // conformance: numeric.float.compare.nan
    if infinity <= one || nan == nan { return 3 }
    small_one: float32 = 1.0
    small_zero: float32 = 0.0
    small_infinity: float32 = small_one / small_zero
    // conformance: numeric.float.divide.zero.f32
    if small_infinity <= small_one { return 4 }
    signed_byte: s8 = cast(s8)127
    unsigned_byte: u8 = cast(u8)255
    signed_word: s32 = cast(s32)2147483647
    // conformance: numeric.wrap.add.u8
    // conformance: numeric.wrap.add.s8
    // conformance: numeric.wrap.sub.u8
    // conformance: numeric.wrap.mul.s8
    // conformance: numeric.wrap.add.s32
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
    // conformance: numeric.div_min.s32
    // conformance: numeric.rem_min.s32
    // conformance: numeric.div_min.s64
    // conformance: numeric.rem_min.s64
    if minimum_word / minus_one_word != minimum_word ||
       minimum_word % minus_one_word != cast(s32)0 ||
       minimum_long / minus_one_long != minimum_long ||
       minimum_long % minus_one_long != cast(s64)0 { return 6 }
    // conformance: numeric.cast.width_pairs.all
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
    // conformance: numeric.mixed.add.u16_to_u8
    // conformance: numeric.mixed.add.u8_to_u16
    if cast(u8)(cast(u16)300 + cast(u16)12) != cast(u8)56 ||
       cast(u16)(cast(u8)255 + cast(u8)2) != cast(u16)1 { return 8 }
    bits_left: u8 = cast(u8)12
    bits_right: u8 = cast(u8)10
    // conformance: numeric.bitwise.and.u8
    // conformance: numeric.bitwise.or.u8
    // conformance: numeric.bitwise.xor.u8
    if (bits_left & bits_right) != cast(u8)8 ||
       (bits_left | bits_right) != cast(u8)14 ||
       (bits_left ^ bits_right) != cast(u8)6 { return 9 }
    return 42
}

#program_export
Bad8 :: () -> s32 {
    value: u8 = cast(u8)1
    amount: u8 = cast(u8)8
    // conformance: numeric.shift.reject.width
    return cast(s32)(value << amount)
}

#program_export
Bad16 :: () -> s32 {
    value: u16 = cast(u16)1
    amount: u16 = cast(u16)16
    // conformance: numeric.shift.reject.width
    return cast(s32)(value << amount)
}

#program_export
BadNegative :: () -> s32 {
    value: s16 = cast(s16)1
    amount: s16 = cast(s16)(-1)
    // conformance: numeric.shift.reject.negative
    return cast(s32)(value << amount)
}
ZI

"$ziran" capabilities --json > "$work/capabilities.json"
python3 - "$work/numeric.zi" "$work/capabilities.json" \
         "$repo/tests/numeric_conformance.json" <<'PY'
import json
import re
from pathlib import Path
import sys

source, capabilities_path, manifest_path = map(Path, sys.argv[1:])
ids = []
for match in re.finditer(r'// conformance: ([A-Za-z0-9_.-]+)', source.read_text()):
    identifier = match.group(1)
    if identifier not in ids:
        ids.append(identifier)
manifest = json.loads(manifest_path.read_text())
capability = json.loads(capabilities_path.read_text())['numeric_conformance']
manifest_ids = [case['id'] for case in manifest['cases']]
assert ids == manifest_ids, (ids, manifest_ids)
assert capability['ids'] == manifest_ids
assert len(manifest_ids) == len(set(manifest_ids))
assert manifest['targets'] == ['c', 'cpp', 'go', 'rust', 'py', 'zib']
PY

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

    # Rust builds one executable per entry: Answer returns 42 as its exit
    # status, and each bad shift must fail at run time.
    for entry in Answer Bad8 Bad16 BadNegative; do
        output=$work/rust-$input-$entry
        "$ziran" build --target=rust --exe --entry "numeric:$entry" \
            --root "$root" -o "$output" "$module"
        CARGO_TARGET_DIR=$work/rust-target cargo build --quiet \
            --manifest-path "$output/Cargo.toml"
        cp "$work/rust-target/debug/ziran_generated" "$output/app"
        if test "$entry" = Answer; then
            status=0
            "$output/app" || status=$?
            test "$status" = 42
        elif "$output/app" > "$output/bad.log" 2>&1; then
            echo "rust accepted an oversized shift in $entry" >&2
            exit 1
        fi
    done

    # Python builds one program per entry the same way.
    for entry in Answer Bad8 Bad16 BadNegative; do
        output=$work/py-$input-$entry
        "$ziran" build --target=py --exe --entry "numeric:$entry" \
            --root "$root" -o "$output" "$module"
        if test "$entry" = Answer; then
            status=0
            python3 "$output" || status=$?
            test "$status" = 42
        elif python3 "$output" > "$output.log" 2>&1; then
            echo "python accepted an oversized shift in $entry" >&2
            exit 1
        fi
    done
done
