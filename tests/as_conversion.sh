#!/bin/sh
set -eu

ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/lib.zi" <<'ZI'
LibWidth :: #as (value: s32) -> float32 {
    return cast(float32)value * 2.0
}
ZI
cat > "$work/app.zi" <<'ZI'
Width :: #as (value: s32) -> float32 {
    return cast(float32)value * 2.0
}

Double :: (value: float32) -> s32 {
    return cast(s32)(value * 2.0)
}

FromFloat :: #as (value: float32) -> s32 {
    return cast(s32)value / 2
}

Meters :: struct { value: s32 }
ToMeters :: #as (value: s32) -> Meters { return .{value = value} }
Sum3 :: (a: s32, b: s32, c: s32) -> s32 { return a + b + c }

#program_export
Answer :: () -> s32 {
    count: s32 = 21
    explicit: float32 = Width(count)
    if explicit != 42.0 { return 0 }
    implicit: float32 = count
    if implicit != 42.0 { return 0 }
    passed: s32 = Double(count)
    if passed != 84 { return 0 }
    back: s32 = explicit
    if back != 21 { return 0 }
    // A converted literal keeps later call arguments in the checked graph.
    distance: Meters = 7
    if distance.value + Sum3(1, 2, 3) != 13 { return 0 }
    return 1
}
ZI

cat > "$work/imported.zi" <<'ZI'
using Lib :: #import "lib";

#program_export
Imported :: () -> s32 {
    count: s32 = 21
    imported: float32 = count
    if imported != 42.0 { return 0 }
    return 1
}
ZI
"$ziran" bundle --root "$work" --entry imported:Imported \
    -o "$work/imported.zib" "$work/imported.zi"
test "$("$ziran" run "$work/imported.zib")" = 1

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
        -o "$work/$input.zib" "$module"
    test "$("$ziran" run "$work/$input.zib")" = 1
    for target in c cpp go; do
        output="$work/$target-$input"
        if test "$target" = go; then
            "$ziran" build --target=go --pkg main --entry app:Answer \
                --root "$root" -o "$output" "$module"
            cat > "$output/main.go" <<'GO'
package main
func main() { if App_Answer() != 1 { panic("as conversion") } }
GO
            GO111MODULE=off go run "$output"/*.go
        elif test "$target" = c; then
            "$ziran" build --target=c --entry app:Answer --root "$root" \
                -o "$output" "$module"
            cat > "$output/main.c" <<'C'
#include "app.h"
int main(void) { return Answer() == 1 ? 0 : 1; }
C
            "${CC:-cc}" -std=c99 -pedantic-errors \
                -I"$repo/include" -I"$output" "$output"/*.c \
                -o "$output/app"
            "$output/app"
        else
            "$ziran" build --target=cpp --entry app:Answer --root "$root" \
                -o "$output" "$module"
            cat > "$output/main.cpp" <<'CPP'
#include "app.hpp"
int main() { return Answer() == 1 ? 0 : 1; }
CPP
            "${CXX:-c++}" -std=c++17 \
                -I"$repo/include" -I"$output" "$output"/*.cpp \
                -o "$output/app"
            "$output/app"
        fi
    done
done
cmp "$work/source.zib" "$work/saved.zib"

cat > "$work/bad.zi" <<'ZI'
Width :: #as (value: s32) -> float32 {
    return cast(float32)value * 2.0
}
#program_export
Bad :: () -> s32 {
    count: s32 = 21
    text: string = count
    return 0
}
ZI
if "$ziran" check --root "$work" "$work/bad.zi" \
    2> "$work/bad.err"; then
    echo 'a conversion with no matching #as was accepted' >&2
    exit 1
fi
rg -q 'initializer type mismatch' "$work/bad.err"
