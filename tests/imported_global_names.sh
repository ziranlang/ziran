#!/bin/sh
set -eu

ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/left.zi" <<'ZI'
value: s64 = 20;
range: s64 = 1;
Get :: () -> s64 { return value }
ZI
cat > "$work/right.zi" <<'ZI'
value: s64 = 22;
ziran_keyword_range_0: s64 = 2;
Get :: () -> s64 { return value }
ZI
cat > "$work/app.zi" <<'ZI'
Left :: #import "left";
Right :: #import "right";
#program_export
Answer :: () -> s64 {
    if Left.value != 20 || Right.value != 22 { return 0 }
    if Left.range + Right.ziran_keyword_range_0 != 3 { return 0 }
    Left.value = 21
    Right.value = 21
    return Left.Get() + Right.Get()
}
ZI

"$ziran" ir --root "$work" -o "$work/ir" "$work/app.zi"
for input in source saved; do
    if test "$input" = source; then
        root=$work
        file=$work/app.zi
    else
        root=$work/ir
        file=$work/ir/app.zir
    fi
    "$ziran" bundle --root "$root" --entry app:Answer \
        -o "$work/$input.zx" "$file"
    test "$("$ziran" run "$work/$input.zx")" = 42
    for target in c cpp go; do
        out="$work/$target-$input"
        if test "$target" = go; then
            "$ziran" build --target=go --pkg main --root "$root" \
                -o "$out" "$file"
            cat > "$out/main.go" <<'GO'
package main
func main() { if App_Answer() != 42 { panic("imported global names") } }
GO
            GO111MODULE=off go run "$out"/*.go
        elif test "$target" = c; then
            "$ziran" build --target=c --root "$root" -o "$out" "$file"
            cat > "$out/main.c" <<'C'
#include "app.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
            "${CC:-cc}" -std=c11 -I"$repo/include" -I"$out" \
                "$out"/*.c -o "$out/program"
            "$out/program"
        else
            "$ziran" build --target=cpp --root "$root" -o "$out" "$file"
            cat > "$out/main.cpp" <<'CPP'
#include "app.hpp"
int main() { return Answer() == 42 ? 0 : 1; }
CPP
            "${CXX:-c++}" -std=c++17 -I"$repo/include" -I"$out" \
                "$out"/*.cpp -o "$out/program"
            "$out/program"
        fi
    done
done
cmp "$work/source.zx" "$work/saved.zx"
