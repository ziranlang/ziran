#!/bin/sh
set -eu

ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/left.zi" <<'ZI'
Get :: () -> s64 { return 20 }
Const :: () -> s64 { return 0 }
#program_export
Public :: () -> s64 { return 0 }
ZI
cat > "$work/right.zi" <<'ZI'
left_Get: s64 = 8;
Left_Get: s64 = 7;
left_Const :: 3;
Left_Const :: 4;
Public: s64 = 0;
ZI
cat > "$work/app.zi" <<'ZI'
Left :: #import "left";
Right :: #import "right";
#program_export
Answer :: () -> s64 {
    return Left.Get() + Left.Const() + Left.Public() +
           Right.left_Get + Right.Left_Get + Right.left_Const +
           Right.Left_Const + Right.Public
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
func main() { if App_Answer() != 42 { panic("function/value names") } }
GO
            GO111MODULE=off go run "$out"/*.go
        elif test "$target" = c; then
            "$ziran" build --target=c --root "$root" -o "$out" "$file"
            cat > "$out/main.c" <<'C'
#include "app.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
            "${CC:-cc}" -std=c11 -Wall -Werror \
                -I"$repo/include" -I"$out" "$out"/*.c -o "$out/program"
            "$out/program"
        else
            "$ziran" build --target=cpp --root "$root" -o "$out" "$file"
            cat > "$out/main.cpp" <<'CPP'
#include "app.hpp"
int main() { return Answer() == 42 ? 0 : 1; }
CPP
            "${CXX:-c++}" -std=c++17 -Wall -Werror \
                -I"$repo/include" -I"$out" "$out"/*.cpp -o "$out/program"
            "$out/program"
        fi
    done
done
cmp "$work/source.zx" "$work/saved.zx"
