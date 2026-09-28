#!/bin/sh
set -eu

ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/left.zi" <<'ZI'
Color :: enum { Red :: 20; }
Get :: () -> Color { return Color.Red }
Read :: (value: Color) -> s64 { return cast(s64)value }
ZI
cat > "$work/right.zi" <<'ZI'
Color :: enum { Red :: 22; }
Get :: () -> Color { return Color.Red }
Read :: (value: Color) -> s64 { return cast(s64)value }
ZI
cat > "$work/app.zi" <<'ZI'
Left :: #import "left";
Right :: #import "right";
#program_export
Answer :: () -> s64 {
    left: Left.Color = Left.Color.Red
    right: Right.Color = Right.Color.Red
    if Left.Read(Left.Get()) != 20 || Right.Read(Right.Get()) != 22 {
        return 0
    }
    return Left.Read(left) + Right.Read(right)
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
    for target in c cpp go rust; do
        out="$work/$target-$input"
        if test "$target" = rust; then
            "$ziran" build --target=rust --entry app:Answer --root "$root" \
                --exe -o "$out" "$file"
            cargo build --quiet --manifest-path "$out/Cargo.toml"
            set +e
            "$out/target/debug/ziran_generated"
            status=$?
            set -e
            test "$status" -eq 42
        elif test "$target" = go; then
            "$ziran" build --target=go --pkg main --root "$root" \
                -o "$out" "$file"
            cat > "$out/main.go" <<'GO'
package main
func main() { if App_Answer() != 42 { panic("imported enum names") } }
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

cat > "$work/bad.zi" <<'ZI'
Left :: #import "left";
Answer :: () -> Left.Color { return Left.Color.Missing }
ZI
if "$ziran" check --root "$work" "$work/bad.zi" >"$work/bad.out" 2>&1; then
    echo "unknown qualified enum member was accepted" >&2
    exit 1
fi
grep -Fq 'unknown enum member: Missing' "$work/bad.out"
