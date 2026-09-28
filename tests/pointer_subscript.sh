#!/bin/sh
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

# Subscripts apply to the dereferenced or cast pointer, not to the operand
# before the dereference or cast: pp.*[1] reads the second byte behind *pp,
# and (cast(*u8)raw)[1] indexes the byte view of raw.
cat > "$work/subscript.zi" <<'ZI'
Second :: (pp: **u8) -> u8 {
    return pp.*[1]
}

Advance :: (pp: **u8) {
    pp.* = *pp.*[1]
}

ByteAt :: (raw: *void, index: s32) -> u8 {
    return (cast(*u8)raw)[index]
}

bytes: [4]u8;

#program_export
Answer :: () -> s32 {
    bytes[0] = 10
    bytes[1] = 20
    bytes[2] = 30
    cursor: *u8 = *bytes[0]
    if Second(*cursor) != 20 { return 1 }
    Advance(*cursor)
    if cursor.* != 20 { return 2 }
    if ByteAt(cast(*void)*bytes[0], 2) != 30 { return 3 }
    return 42
}
ZI

"$ziran" ir --root "$work" --entry subscript:Answer -o "$work/ir" \
    "$work/subscript.zi"
for input in source saved; do
    if test "$input" = source; then
        source=$work/subscript.zi
        root=$work
    else
        source=$work/ir/subscript.zir
        root=$work/ir
    fi
    for target in c cpp; do
        output="$work/$target-$input"
        "$ziran" build --target="$target" --root "$root" \
            --entry subscript:Answer -o "$output" "$source"
        if test "$target" = c; then
            cat > "$output/main.c" <<'C'
#include "subscript.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
            "${CC:-cc}" -std=c11 -I"$repo/include" -I"$output" \
                "$output"/*.c -o "$output/app"
        else
            cat > "$output/main.cpp" <<'CPP'
#include "subscript.hpp"
int main() { return Answer() == 42 ? 0 : 1; }
CPP
            "${CXX:-c++}" -std=c++17 -I"$repo/include" -I"$output" \
                "$output"/*.cpp -o "$output/app"
        fi
        "$output/app"
    done
done
echo 'subscripts after a dereference or cast: C and C++ passed'
