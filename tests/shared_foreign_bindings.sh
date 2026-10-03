#!/bin/sh
# Modules that bind the same foreign function under the same name can be
# included together; a clashing binding of that name still fails to compile.
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

for name in first second; do
    cat > "$work/$name.zi" <<ZI
libc :: #system_library "libc";
Absolute :: (value: s32) -> s32 #foreign libc "abs";
Print :: (format: *u8, args: ..any) -> s32 #foreign libc "printf";
${name}_magnitude :: (value: s32) -> s32 { return Absolute(value) }
ZI
done
cat > "$work/main.zi" <<'ZI'
using First :: #import "first";
using Second :: #import "second";
#program_export
main :: () -> s32 { return First.first_magnitude(-20) + Second.second_magnitude(-22) - 42 }
ZI
"$ziran" ir --entry main:main --root "$work" -o "$work/ir" "$work/main.zi"
for form in source saved; do
    input="$work/main.zi"
    if test "$form" = saved; then input="$work/ir/main.zir"; fi
    for target in c cpp; do
        output="$work/$form-$target"
        "$ziran" build "--target=$target" --entry main:main --no-main --root "$work" -o "$output" "$input"
        if test "$target" = c; then compiler=${CC:-cc}; standard=c99; extension=c
        else compiler=${CXX:-c++}; standard=c++17; extension=cpp; fi
        "$compiler" -std="$standard" -pedantic-errors -Wall -Werror -Wno-unused-function \
            -I"$repo/include" -I"$output" "$output"/*."$extension" -o "$output/app"
        "$output/app"
    done
done

# A shared name with a different symbol or ABI must remain a real conflict.
cp "$work/second.zi" "$work/second-original.zi"
for clash in symbol signature; do
    if test "$clash" = symbol; then
        sed 's/"abs"/"labs"/' "$work/second-original.zi" > "$work/second.zi"
    else
        sed 's/Absolute :: (value: s32)/Absolute :: (value: s64)/' "$work/second-original.zi" > "$work/second.zi"
    fi
    for target in c cpp; do
        output="$work/$clash-$target"
        "$ziran" build "--target=$target" --entry main:main --no-main --root "$work" -o "$output" "$work/main.zi"
        if test "$target" = c; then compiler=${CC:-cc}; standard=c99; extension=c
        else compiler=${CXX:-c++}; standard=c++17; extension=cpp; fi
        if "$compiler" -std="$standard" -I"$repo/include" -I"$output" \
            "$output"/*."$extension" -o "$output/app" > "$output/errors" 2>&1; then
            echo "Two foreign bindings named Absolute with different $clash compiled together ($target)" >&2
            exit 1
        fi
        rg -q 'redefinition.*Absolute|conflicting.*zir_foreign_Absolute' "$output/errors"
    done
done
echo 'Shared foreign bindings passed C/C++ source and saved IR execution; symbol and ABI clashes rejected'
