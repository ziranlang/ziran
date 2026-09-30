#!/bin/sh
# print shows an enum value by its member name, and a value outside the enum
# as "(invalid Enum)". Enums from open and named imports print the same way,
# from source and saved IR, on every target.
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/palette.zi" <<'ZI'
Shade :: enum { LIGHT; DARK; }
ZI
cat > "$work/app.zi" <<'ZI'
#import "palette";
Pal :: #import "palette";
Color :: enum { RED; GREEN; BLUE; }
Mode :: enum u8 { OFF :: 0; ON :: 5; }

#program_export
Answer :: () -> s32 {
    c := Color.GREEN;
    print("% % % %\n", c, Color.BLUE, Mode.ON, cast(Color) 7);
    print("% %\n", Shade.DARK, Pal.Shade.LIGHT);
    return 42;
}
ZI
expected='GREEN BLUE ON (invalid Color)
DARK LIGHT'

"$ziran" check --root "$work" "$work/app.zi"
"$ziran" ir --root "$work" -o "$work/ir" "$work/app.zi"
for input in source saved; do
    if test "$input" = source; then
        module=$work/app.zi
        root=$work
    else
        module=$work/ir/app.zir
        root=$work/ir
    fi
    "$ziran" bundle --root "$root" --entry app:Answer -o "$work/$input.zib" "$module"
    test "$("$ziran" run "$work/$input.zib")" = "$expected
42"
    output="$work/c-$input"
    "$ziran" build --target=c --exe --entry app:Answer --root "$root" -o "$output" "$module"
    test "$("$output/app" || true)" = "$expected"
    output="$work/cpp-$input"
    "$ziran" build --target=cpp --root "$root" -o "$output" "$module"
    cat > "$output/main.cpp" <<'CPP'
#include "app.hpp"
int main() { return Answer() == 42 ? 0 : 1; }
CPP
    "${CXX:-c++}" -I"$repo/include" -I"$output" "$output"/*.cpp -o "$output/app"
    test "$("$output/app")" = "$expected"
    output="$work/go-$input"
    "$ziran" build --target=go --pkg main --root "$root" -o "$output" "$module"
    cat > "$output/main.go" <<'GO'
package main
func main() { if App_Answer() != 42 { panic("wrong result") } }
GO
    test "$(GO111MODULE=off go run "$output"/*.go)" = "$expected"
done
cmp "$work/source.zib" "$work/saved.zib"
