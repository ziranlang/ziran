#!/bin/sh
set -eu

ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/calls.zi" <<'ZI'
hits: s32;
Add :: (first: s32, second: s32) -> s32 { return first + second }
Both :: (first: bool, second: bool) -> bool { return first && second }
Next :: () -> s32 { hits += 1; return hits }
Never :: () -> s32 { unreachable }
Touch :: () -> bool { hits += 100; return true }

#program_export
Answer :: () -> s32 {
    if Add(ifx true then 20 else Never(), 22) != 42 { return 0 }
    if Add(ifx false then Never() else 20, 22) != 42 { return 0 }
    if Add(second = 22, first = ifx true then 20 else Never()) != 42 {
        return 0
    }
    flag: bool = true
    if Add(ifx flag then 20 else Never(), 22) != 42 { return 0 }
    if Add(ifx false then Never() else ifx flag then 20 else 0, 22) != 42 {
        return 0
    }
    if Add(#ifx true then 20 else MissingValue(), 22) != 42 { return 0 }
    if Both(first = true || Touch(), second = false && Touch()) { return 0 }
    if !Both(first = false || true, second = true && true) { return 0 }
    if hits != 0 { return 0 }
    if Add(ifx flag then Next() else Never(), 41) != 42 { return 0 }
    if hits != 1 { return 0 }
    return 42
}
ZI

"$ziran" check --root "$work" "$work/calls.zi"
"$ziran" ir --root "$work" -o "$work/ir" "$work/calls.zi"
"$ziran" bundle --root "$work" --entry calls:Answer \
    -o "$work/source.zx" "$work/calls.zi"
"$ziran" bundle --root "$work/ir" --entry calls:Answer \
    -o "$work/saved.zx" "$work/ir/calls.zir"
cmp "$work/source.zx" "$work/saved.zx"
test "$("$ziran" run "$work/source.zx")" = 42
test "$("$ziran" run "$work/saved.zx")" = 42

for input in "$work/calls.zi" "$work/ir/calls.zir"; do
    case "$input" in
        *.zi) suffix=source; root=$work ;;
        *) suffix=saved; root=$work/ir ;;
    esac
    for target in c cpp go; do
        out="$work/$target-$suffix"
        if test "$target" = go; then
            "$ziran" build --target=go --pkg main --entry calls:Answer \
                --root "$root" -o "$out" "$input"
            cat > "$out/main.go" <<'GO'
package main
func main() { if Calls_Answer() != 42 { panic("nested lazy call") } }
GO
            GO111MODULE=off go run "$out"/*.go
        elif test "$target" = c; then
            "$ziran" build --target=c --entry calls:Answer --root "$root" \
                -o "$out" "$input"
            cat > "$out/main.c" <<'C'
#include "calls.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
            "${CC:-cc}" -std=c99 -pedantic-errors \
                -I"$repo/include" -I"$out" "$out"/*.c -o "$out/app"
            "$out/app"
        else
            "$ziran" build --target=cpp --entry calls:Answer --root "$root" \
                -o "$out" "$input"
            cat > "$out/main.cpp" <<'CPP'
#include "calls.hpp"
int main() { return Answer() == 42 ? 0 : 1; }
CPP
            "${CXX:-c++}" -std=c++17 \
                -I"$repo/include" -I"$out" "$out"/*.cpp -o "$out/app"
            "$out/app"
        fi
    done
done
