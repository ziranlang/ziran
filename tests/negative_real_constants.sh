#!/bin/sh
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

# Named real constants may be negative or signed, directly or through an
# alias, and stay correct when negated again or used in arithmetic.
cat > "$work/signed.zi" <<'ZI'
Side :: -90.0;
Front :: +12.5;
Alias :: Side;
Again :: -Side;

#program_export
Answer :: () -> s32 {
    side: float64 = Side
    if side != -90.0 { return 1 }
    if Alias != -90.0 { return 2 }
    if Again != 90.0 { return 3 }
    if -Side != 90.0 { return 4 }
    if 1.0 - Side != 91.0 { return 5 }
    turn: float64 = Front - Alias
    if turn != 102.5 { return 6 }
    small: float32 = Side
    if small != -90.0 { return 7 }
    return 42
}
ZI

"$ziran" ir --root "$work" --entry signed:Answer -o "$work/ir" "$work/signed.zi"
for input in source saved; do
    if test "$input" = source; then
        source=$work/signed.zi
        root=$work
    else
        source=$work/ir/signed.zir
        root=$work/ir
    fi
    "$ziran" bundle --root "$root" --entry signed:Answer \
        -o "$work/$input.zib" "$source"
    test "$("$ziran" run "$work/$input.zib")" = 42
    for target in c cpp; do
        output="$work/$target-$input"
        "$ziran" build --target="$target" --root "$root" \
            --entry signed:Answer -o "$output" "$source"
        if test "$target" = c; then
            cat > "$output/main.c" <<'C'
#include "signed.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
            "${CC:-cc}" -std=c11 -I"$repo/include" -I"$output" \
                "$output"/*.c -o "$output/app"
        else
            cat > "$output/main.cpp" <<'CPP'
#include "signed.hpp"
int main() { return Answer() == 42 ? 0 : 1; }
CPP
            "${CXX:-c++}" -std=c++17 -I"$repo/include" -I"$output" \
                "$output"/*.cpp -o "$output/app"
        fi
        "$output/app"
    done
done

echo 'signed real constants: VM, C and C++ passed'
