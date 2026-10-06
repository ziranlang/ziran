#!/bin/sh
# Modules that bind the same foreign function under the same name can be
# included together; each module also owns distinct symbols and signatures.
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

for name in first second; do
    cat > "$work/$name.zi" <<ZI
libc :: #system_library "libc";
Absolute :: (${name}_argument: s32) -> s32 #foreign libc "abs";
abs :: (value: s32) -> s32 #foreign libc "abs";
Print :: (${name}_format: *u8, ${name}_arguments: ..any) -> s32 #foreign libc "printf";
${name}_magnitude :: (value: s32) -> s32 { return Absolute(value) + abs(0) }
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

# Independent modules may use the same local name for different real ABIs.
cat > "$work/second.zi" <<'ZI'
libc :: #system_library "libc";
Absolute :: (value: s64) -> s64 #foreign libc "labs";
second_magnitude :: (value: s32) -> s32 { return cast(s32)Absolute(cast(s64)value) }
ZI
"$ziran" ir --entry main:main --root "$work" -o "$work/distinct-ir" "$work/main.zi"
for form in source saved; do
    input="$work/main.zi"
    if test "$form" = saved; then input="$work/distinct-ir/main.zir"; fi
    for target in c cpp; do
        output="$work/distinct-$form-$target"
        "$ziran" build "--target=$target" --entry main:main --no-main --root "$work" -o "$output" "$input"
        if test "$target" = c; then compiler=${CC:-cc}; standard=c99; extension=c
        else compiler=${CXX:-c++}; standard=c++17; extension=cpp; fi
        "$compiler" -std="$standard" -pedantic-errors -Wall -Werror -Wno-unused-function \
            -I"$repo/include" -I"$output" "$output"/*."$extension" -o "$output/app"
        "$output/app"
    done
done
echo 'Module-owned foreign bindings passed C/C++ source and saved IR with shared and distinct symbols/ABIs'
