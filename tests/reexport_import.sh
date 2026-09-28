#!/bin/sh
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

# A public module entry re-exports other modules with `using`. Importing it
# without `using` still brings those names, so entry linking must keep the
# entry module and its import even though the entry declares nothing.
# Integer, real, and string constants pass through the same chain.
cat > "$work/inner.zi" <<'ZI'
Pair :: struct {
    value: s32
}
InnerStep :: 1;
InnerScale :: 0.5;
InnerLabel :: "ok";
#program_export
InnerValue :: () -> s32 { return 40 }
ZI
cat > "$work/Surface.zi" <<'ZI'
using InnerModule :: #import "inner";
ZI
cat > "$work/reexport.zi" <<'ZI'
#import "Surface"
#program_export
Answer :: () -> s32 {
    pair: Pair
    pair.value = InnerStep + cast(s32)(InnerScale * 2.0)
    if InnerLabel.count != 2 { return 0 }
    return InnerValue() + pair.value
}
ZI

"$ziran" ir --root "$work" --entry reexport:Answer -o "$work/ir" \
    "$work/reexport.zi"
for input in source saved; do
    if test "$input" = source; then
        source=$work/reexport.zi
        root=$work
    else
        source=$work/ir/reexport.zir
        root=$work/ir
    fi
    "$ziran" bundle --root "$root" --entry reexport:Answer \
        -o "$work/$input.zib" "$source"
    test "$("$ziran" run "$work/$input.zib")" = 42
    for target in c cpp go; do
        output="$work/$target-$input"
        "$ziran" build --target="$target" --root "$root" \
            --entry reexport:Answer -o "$output" "$source"
        if test "$target" = go; then
            cat > "$output/main_test.go" <<'GO'
package ziran
import "testing"
func TestReexport(t *testing.T) {
    if Reexport_Answer() != 42 { t.Fatal("re-exported result") }
}
GO
            GO111MODULE=off go test "$output"/*.go
        elif test "$target" = c; then
            cat > "$output/main.c" <<'C'
#include "reexport.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
            "${CC:-cc}" -std=c11 -I"$repo/include" -I"$output" \
                "$output"/*.c -o "$output/app"
            "$output/app"
        else
            cat > "$output/main.cpp" <<'CPP'
#include "reexport.hpp"
int main() { return Answer() == 42 ? 0 : 1; }
CPP
            "${CXX:-c++}" -std=c++17 -I"$repo/include" -I"$output" \
                "$output"/*.cpp -o "$output/app"
            "$output/app"
        fi
    done
done
echo 'plain import of a re-exporting module and its constants: VM, C, C++, and Go passed'
