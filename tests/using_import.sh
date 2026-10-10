#!/bin/sh
set -eu

ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/lib.zi" <<'ZI'
Limit :: 7
Scale :: 3
Point :: struct {
    x: s32
    y: s32
}
MakePoint :: (x: s32, y: s32) -> Point {
    return Point.{.x = x, .y = y}
}
Combine :: (a: Point, b: Point) -> s32 {
    return a.x * Scale + b.y
}
ZI
cat > "$work/other.zi" <<'ZI'
Limit :: 100
ZI
cat > "$work/app.zi" <<'ZI'
using Lib :: #import "lib";

#program_export
Answer :: () -> s32 {
    p: Point = MakePoint(Limit, 4)
    q := Lib.MakePoint(1, Limit)
    if p.x != 7 { return 0 }
    if q.y != 7 { return 0 }
    return Combine(p, q) + Limit
}
ZI

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
    test "$("$ziran" run "$work/$input.zib")" = 35
    for target in c cpp go; do
        output="$work/$target-$input"
        if test "$target" = go; then
            "$ziran" build --target=go --pkg main --entry app:Answer \
                --root "$root" -o "$output" "$module"
            cat > "$output/main.go" <<'GO'
package main
func main() { if App_Answer() != 35 { panic("using import") } }
GO
            GO111MODULE=off go run "$output"/*.go
        elif test "$target" = c; then
            "$ziran" build --target=c --entry app:Answer --root "$root" \
                -o "$output" "$module"
            cat > "$output/main.c" <<'C'
#include "app.h"
int main(void) { return Answer() == 35 ? 0 : 1; }
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
int main() { return Answer() == 35 ? 0 : 1; }
CPP
            "${CXX:-c++}" -std=c++17 \
                -I"$repo/include" -I"$output" "$output"/*.cpp \
                -o "$output/app"
            "$output/app"
        fi
    done
done
cmp "$work/source.zib" "$work/saved.zib"

cat > "$work/shadow.zi" <<'ZI'
using Lib :: #import "lib";
Limit :: 40;
#program_export
Answer :: () -> s32 {
    return Limit
}
ZI
"$ziran" bundle --root "$work" --entry shadow:Answer \
    -o "$work/shadow.zib" "$work/shadow.zi"
test "$("$ziran" run "$work/shadow.zib")" = 40

cat > "$work/clash.zi" <<'ZI'
using A :: #import "lib";
using B :: #import "other";
#program_export
Bad :: () -> s32 {
    return Limit
}
ZI
if "$ziran" check --root "$work" "$work/clash.zi" \
    2> "$work/clash.err"; then
    echo 'ambiguous using-import name was accepted' >&2
    exit 1
fi
rg -q 'ambiguous' "$work/clash.err"

# A module's own procedure wins over a type of the same name that the module
# opens with using, also when callers qualify it with the module alias.
cat > "$work/maker.zi" <<'ZI'
using Lib :: #import "lib";
Point :: () -> s32 {
    return MakePoint(9, 1).x
}
ZI
cat > "$work/caller.zi" <<'ZI'
Maker :: #import "maker";
#program_export
Answer :: () -> s32 {
    return Maker.Point()
}
ZI
"$ziran" bundle --root "$work" --entry caller:Answer \
    -o "$work/caller.zib" "$work/caller.zi"
test "$("$ziran" run "$work/caller.zib")" = 9
"$ziran" build --target=c --entry caller:Answer --root "$work" \
    -o "$work/caller-c" "$work/caller.zi"
cat > "$work/caller-c/main.c" <<'C'
#include "caller.h"
int main(void) { return Answer() == 9 ? 0 : 1; }
C
"${CC:-cc}" -std=c99 -pedantic-errors -I"$repo/include" -I"$work/caller-c" \
    "$work/caller-c"/*.c -o "$work/caller-c/app"
"$work/caller-c/app"
"$ziran" build --target=cpp --entry caller:Answer --root "$work" \
    -o "$work/caller-cpp" "$work/caller.zi"
cat > "$work/caller-cpp/main.cpp" <<'CPP'
#include "caller.hpp"
int main() { return Answer() == 9 ? 0 : 1; }
CPP
"${CXX:-c++}" -std=c++17 -I"$repo/include" -I"$work/caller-cpp" \
    "$work/caller-cpp"/*.cpp -o "$work/caller-cpp/app"
"$work/caller-cpp/app"
