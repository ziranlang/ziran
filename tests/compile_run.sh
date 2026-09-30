#!/bin/sh
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
BASE :: 10;

Double :: (value: s64) -> s64 { return value * 2 }
Combine :: (left: s64, right: s64) -> s64 {
    return Double(left) + right
}
Width :: () -> s64 { return size_of(s32) }
Count :: () -> s64 {
    current: s64 = 0
    while current < 4 { current += 1 }
    return current
}

VALUE :: #run Combine(Double(BASE), 1);
SELECTED :: #ifx Combine(2, 3) == 7 then 1 else MissingValue();
#assert Combine(2, 3) == 7
#assert Width() == 4
#if Width() == Count() {
FLAG :: 0;
} else {
FLAG :: MissingValue();
}

#program_export
Answer :: () -> s32 { return VALUE + SELECTED + FLAG }
ZI

"$ziran" check --root "$work" "$work/app.zi"
"$ziran" ir --root "$work" -o "$work/ir" "$work/app.zi"

for input in "$work/app.zi" "$work/ir/app.zir"; do
    case "$input" in
        *.zi) suffix=source ; root=$work ;;
        *) suffix=saved ; root=$work/ir ;;
    esac
    for target in c cpp go; do
        out="$work/$target-$suffix"
        if test "$target" = go; then
            "$ziran" build --target=go --pkg main --root "$root" \
                -o "$out" "$input"
            cat > "$out/main.go" <<'GO'
package main
func main() { if App_Answer() != 42 { panic("compile-time call result") } }
GO
            GO111MODULE=off go run "$out"/*.go
        elif test "$target" = c; then
            "$ziran" build --target=c --root "$root" -o "$out" "$input"
            cat > "$out/main.c" <<'C'
#include "app.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
            "${CC:-cc}" -std=c11 -I"$repo/include" -I"$out" \
                "$out"/*.c -o "$out/app"
            "$out/app"
        else
            "$ziran" build --target=cpp --root "$root" -o "$out" "$input"
            cat > "$out/main.cpp" <<'CPP'
#include "app.hpp"
int main() { return Answer() == 42 ? 0 : 1; }
CPP
            "${CXX:-c++}" -std=c++17 -I"$repo/include" -I"$out" \
                "$out"/*.cpp -o "$out/app"
            "$out/app"
        fi
    done
done

# Imported calls and multi-statement procedures are resolved while translating
# the linked module graph. The literal result must survive saved IR and every
# output path.
mkdir -p "$work/linked"
cat > "$work/linked/lib.zi" <<'ZI'
Pair :: struct {
    left: s32
    right: s32
}
Factorial :: (n: s64) -> s64 {
    result: s64 = 1
    while n > 1 {
        result *= n
        n -= 1
    }
    return result
}
Even :: (n: s64) -> bool { return n % 2 == 0 }
Small :: (n: u8) -> u8 { return n + 1 }
Sum :: () -> s64 {
    result: s64 = 0
    for i: 1..4 { result += i }
    return result
}
ZI
cat > "$work/linked/app.zi" <<'ZI'
Lib :: #import "lib";
#if Lib.Factorial(4) == 24 && size_of(Lib.Pair) == 8 {
FLAG :: 0;
} else {
FLAG :: Missing();
#import "not_present"
}
Choice :: struct {
    #if Lib.Even(4) {
        value: s64
    } else {
        missing: MissingType
    }
}
BASE :: #run Lib.Factorial(4);
BONUS :: #ifx Lib.Even(4) then 18 else Missing();
Choose :: (n: s64) -> s64 {
    if n < 0 { return 0 }
    else if Lib.Even(n) { return BASE + BONUS }
    else { return 0 }
}
VALUE :: #run Choose(4);
SUM :: #run Lib.Sum();
SMALL :: #run Lib.Small(41);
WIDTH :: #run size_of(Lib.Pair);
#assert VALUE == 42
#assert SUM == 10
#assert SMALL == 42
#assert WIDTH == 8
SIZED :: #ifx size_of(Lib.Pair) == 8 then 0 else Missing();
#program_export
Answer :: () -> s64 {
    #if Lib.Even(4) {
        return #ifx Lib.Even(4) then VALUE + FLAG + SIZED else Missing()
    } else {
        return Missing()
    }
}
ZI
"$ziran" check --root "$work/linked" "$work/linked/app.zi"
cat > "$work/linked/sized_app.zi" <<'ZI'
Lib :: #import "lib";
Choose :: () -> s64 {
    return #ifx size_of(Lib.Pair) == 8 then 42 else Missing()
}
VALUE :: #run Choose();
#assert VALUE == 42
Answer :: () -> s64 { return VALUE }
ZI
"$ziran" check --root "$work/linked" "$work/linked/sized_app.zi"
"$ziran" ir --root "$work/linked" -o "$work/linked/ir" \
    "$work/linked/app.zi"
"$ziran" bundle --root "$work/linked" --entry app:Answer \
    -o "$work/linked/source.zib" "$work/linked/app.zi"
"$ziran" bundle --root "$work/linked/ir" --entry app:Answer \
    -o "$work/linked/saved.zib" "$work/linked/ir/app.zir"
cmp "$work/linked/source.zib" "$work/linked/saved.zib"
test "$("$ziran" run "$work/linked/saved.zib")" = 42
for input in source saved; do
    if test "$input" = source; then
        root=$work/linked
        file=$work/linked/app.zi
    else
        root=$work/linked/ir
        file=$work/linked/ir/app.zir
    fi
    for target in c cpp go; do
        out=$work/linked/$target-$input
        if test "$target" = go; then
            "$ziran" build --target=go --pkg main --root "$root" \
                -o "$out" "$file"
            cat > "$out/main.go" <<'GO'
package main
func main() { if App_Answer() != 42 { panic("linked #run result") } }
GO
            GO111MODULE=off go run "$out"/*.go
        elif test "$target" = c; then
            "$ziran" build --target=c --root "$root" -o "$out" "$file"
            cat > "$out/main.c" <<'C'
#include "app.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
            "${CC:-cc}" -std=c11 -I"$repo/include" -I"$out" \
                "$out"/*.c -o "$out/app"
            "$out/app"
        else
            "$ziran" build --target=cpp --root "$root" -o "$out" "$file"
            cat > "$out/main.cpp" <<'CPP'
#include "app.hpp"
int main() { return Answer() == 42 ? 0 : 1; }
CPP
            "${CXX:-c++}" -std=c++17 -I"$repo/include" -I"$out" \
                "$out"/*.cpp -o "$out/app"
            "$out/app"
        fi
    done
done

cat > "$work/effect.zi" <<'ZI'
counter: s64;
Touch :: (value: s64) -> s64 {
    counter += 1
    return value
}
VALUE :: #run Touch(42);
ZI
if "$ziran" check --root "$work" "$work/effect.zi" \
    2> "$work/effect.err"; then
    echo 'effectful compile-time call was accepted' >&2
    exit 1
fi
rg -q '#run expression is not a constant' "$work/effect.err"

cat > "$work/recursive.zi" <<'ZI'
Loop :: (value: s64) -> s64 { return Loop(value) }
VALUE :: #run Loop(1);
ZI
if "$ziran" check --root "$work" "$work/recursive.zi" \
    2> "$work/recursive.err"; then
    echo 'unbounded compile-time recursion was accepted' >&2
    exit 1
fi
rg -q '#run expression is not a constant' "$work/recursive.err"

cat > "$work/foreign_local.zi" <<'ZI'
Read :: () -> s64 { return secret }
Call :: () -> s64 {
    secret: s64 = 42
    return Read()
}
VALUE :: #run Call();
ZI
if "$ziran" check --root "$work" "$work/foreign_local.zi" \
    2> "$work/foreign_local.err"; then
    echo 'compile-time callee captured a caller local' >&2
    exit 1
fi
rg -q '#run expression is not a constant' "$work/foreign_local.err"

# A local without an initializer holds its type's zero, at compile time as
# in every target's generated code.
cat > "$work/zero_local.zi" <<'ZI'
Read :: () -> s64 {
    value: s64;
    return value
}
VALUE :: #run Read();
#law ZeroLocal custom VALUE == 0;
ZI
"$ziran" check --root "$work" "$work/zero_local.zi" > "$work/zero_local.json"
rg -q '"status":"proved"' "$work/zero_local.json"

cat > "$work/unbounded_loop.zi" <<'ZI'
Loop :: () -> s64 {
    while true {}
    return 1
}
VALUE :: #run Loop();
ZI
if "$ziran" check --root "$work" "$work/unbounded_loop.zi" \
    2> "$work/unbounded_loop.err"; then
    echo 'unbounded compile-time loop was accepted' >&2
    exit 1
fi
rg -q '#run expression is not a constant' "$work/unbounded_loop.err"

cat > "$work/shared_fuel.zi" <<'ZI'
Spin :: (limit: s64) -> s64 {
    i: s64 = 0
    while i < limit { i += 1 }
    return i
}
Single :: #run Spin(600);
ZI
"$ziran" check --root "$work" "$work/shared_fuel.zi"
cat >> "$work/shared_fuel.zi" <<'ZI'
Pair :: () -> s64 { return Spin(600) + Spin(600) }
Combined :: #run Pair();
ZI
if "$ziran" check --root "$work" "$work/shared_fuel.zi" \
    2> "$work/shared_fuel.err"; then
    echo 'nested compile-time calls reset the work limit' >&2
    exit 1
fi
rg -q '#run expression is not a constant' "$work/shared_fuel.err"

cat > "$work/impure_assert.zi" <<'ZI'
counter: s64;
Touch :: () -> s64 {
    counter += 1
    return counter
}
#assert Touch() == 1
ZI
if "$ziran" check --root "$work" "$work/impure_assert.zi" \
    2> "$work/impure_assert.err"; then
    echo 'effectful #assert was accepted' >&2
    exit 1
fi
rg -q '#assert requires a compile-time constant condition' \
    "$work/impure_assert.err"

cat > "$work/overflow.zi" <<'ZI'
AddOne :: (value: s64) -> s64 { return value + 1 }
VALUE :: #run AddOne(9223372036854775807);
ZI
if "$ziran" check --root "$work" "$work/overflow.zi" \
    2> "$work/overflow.err"; then
    echo 'overflowing compile-time call was accepted' >&2
    exit 1
fi
rg -q '#run expression is not a constant' "$work/overflow.err"

# Compile-time branches and assertions are resolved by Ziran. They must never
# leave C/C++ preprocessor checks or a Go-only validation path behind.
cat > "$work/assert_selected.zi" <<'ZI'
#if true {
#assert 1, "checked marker"
} else {
#error "inactive branch"
}
#if false {
#assert 0
} else {
#assert 1
}
#program_export
Answer :: () -> s32 { return 42 }
ZI
"$ziran" check --root "$work" "$work/assert_selected.zi"
"$ziran" ir --root "$work" -o "$work/assert-ir" "$work/assert_selected.zi"
python3 - "$work/assert-ir/assert_selected.zir" "$work/assert-forged.zir" <<'PY'
from pathlib import Path
from struct import pack
import sys

data = bytearray(Path(sys.argv[1]).read_bytes())
message = b'"checked marker"'
needle = pack('<I', 1) + b'1' + pack('<I', len(message)) + message
assert data.count(needle) == 1
data[data.index(needle) + 4] = ord('0')
Path(sys.argv[2]).write_bytes(data)
PY
if "$ziran" check --root "$work" "$work/assert-forged.zir" \
    2> "$work/assert-forged.err"; then
    echo 'saved IR accepted a falsified compile-time assertion' >&2
    exit 1
fi
rg -q '#assert failed' "$work/assert-forged.err"
for input in "$work/assert_selected.zi" "$work/assert-ir/assert_selected.zir"; do
    case "$input" in
        *.zi) root=$work; suffix=source ;;
        *) root=$work/assert-ir; suffix=saved ;;
    esac
    for target in c cpp go; do
        out="$work/assert-$target-$suffix"
        "$ziran" build "--target=$target" --root "$root" -o "$out" "$input"
        if rg -q '^#error ' "$out"; then
            echo 'compile-time assertion leaked into native output' >&2
            exit 1
        fi
    done
done

cat > "$work/assert_failed.zi" <<'ZI'
#if true {
#assert 0, "selected failure"
}
Answer :: () -> s32 { return 42 }
ZI
if "$ziran" check --root "$work" "$work/assert_failed.zi" \
    2> "$work/assert_failed.err"; then
    echo 'selected false assertion passed frontend checking' >&2
    exit 1
fi
rg -q '#assert failed' "$work/assert_failed.err"

cat > "$work/error_selected.zi" <<'ZI'
#if true {
#error "selected failure"
}
Answer :: () -> s32 { return 42 }
ZI
if "$ziran" check --root "$work" "$work/error_selected.zi" \
    2> "$work/error_selected.err"; then
    echo 'selected #error passed frontend checking' >&2
    exit 1
fi
rg -q '#error:' "$work/error_selected.err"
