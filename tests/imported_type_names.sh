#!/bin/sh
set -eu

ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/left.zi" <<'ZI'
Point :: struct { x: s64; }
Holder :: struct { point: Point; }
Get :: () -> Point { return Point.{.x = 20} }
Read :: (value: Point) -> s64 { return value.x }
ZI
cat > "$work/right.zi" <<'ZI'
Point :: struct { y: s64; }
Holder :: struct { point: Point; }
Get :: () -> Point { return Point.{.y = 22} }
Read :: (value: Point) -> s64 { return value.y }
ZI
cat > "$work/app.zi" <<'ZI'
Left :: #import "left";
Right :: #import "right";
left_global: Left.Point = Left.Point.{.x = 20};
right_global: Right.Point = Right.Point.{.y = 22};
#program_export
Answer :: () -> s64 {
    left: Left.Holder = Left.Holder.{.point = Left.Get()}
    right: Right.Holder = Right.Holder.{.point = Right.Get()}
    if left.point.x != 20 || right.point.y != 22 { return 0 }
    return Left.Read(left_global) + Right.Read(right_global)
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
        -o "$work/$input.zib" "$file"
    test "$("$ziran" run "$work/$input.zib")" = 42
    for target in c cpp go; do
        out="$work/$target-$input"
        if test "$target" = go; then
            "$ziran" build --target=go --pkg main --root "$root" \
                -o "$out" "$file"
            cat > "$out/main.go" <<'GO'
package main
func main() { if App_Answer() != 42 { panic("imported type names") } }
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
cmp "$work/source.zib" "$work/saved.zib"

cat > "$work/field_type.zi" <<'ZI'
Point :: struct { x: s64; }
ZI
cat > "$work/field_owner.zi" <<'ZI'
#import, file "field_type.zi";
Holder :: struct { point: Point; }
Make :: () -> Point { return Point.{.x = 20} }
ZI
cat > "$work/field_app.zi" <<'ZI'
Owner :: #import "field_owner";
#program_export
Answer :: () -> s64 {
    value: Owner.Holder = Owner.Holder.{.point = Owner.Make()}
    return value.point.x
}
ZI
"$ziran" check --root "$work" "$work/field_app.zi"
"$ziran" ir --root "$work" -o "$work/field-ir" "$work/field_app.zi"
for input in "$work/field_app.zi" "$work/field-ir/field_app.zir"; do
    case "$input" in
        *.zi) root=$work; suffix=field-source ;;
        *) root=$work/field-ir; suffix=field-saved ;;
    esac
    "$ziran" build --target=c --root "$root" -o "$work/$suffix" "$input"
    cat > "$work/$suffix/main.c" <<'C'
#include "field_app.h"
int main(void) { return Answer() == 20 ? 0 : 1; }
C
    "${CC:-cc}" -std=c11 -I"$repo/include" -I"$work/$suffix" \
        "$work/$suffix"/*.c -o "$work/$suffix/program"
    "$work/$suffix/program"
done
