#!/bin/sh
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

# A named import spells an imported record as Lib.Box. Pointers and slices
# of it in the importer match the *Box and []Box of the declaring module's
# signatures.
cat > "$work/lib.zi" <<'ZI'
Box :: struct {
    value: s32
}

storage: [2]Box;

New :: () -> *Box {
    storage[0].value = 20
    return *storage[0]
}

Read :: (box: *Box) -> s32 {
    return box.value
}

Total :: (boxes: []Box) -> s32 {
    return boxes[0].value + boxes[1].value
}
ZI
cat > "$work/qualified.zi" <<'ZI'
Lib :: #import "lib";

held: *Lib.Box;

#program_export
Answer :: () -> s32 {
    local: *Lib.Box = Lib.New()
    held = Lib.New()
    pair: [2]Lib.Box
    pair[0].value = 1
    pair[1].value = 1
    return Lib.Read(held) + Lib.Read(local) + Lib.Total(pair[:])
}
ZI

"$ziran" ir --root "$work" --entry qualified:Answer -o "$work/ir" \
    "$work/qualified.zi"
for input in source saved; do
    if test "$input" = source; then
        source=$work/qualified.zi
        root=$work
    else
        source=$work/ir/qualified.zir
        root=$work/ir
    fi
    for target in c cpp go; do
        output="$work/$target-$input"
        "$ziran" build --target="$target" --root "$root" \
            --entry qualified:Answer -o "$output" "$source"
        if test "$target" = go; then
            cat > "$output/main_test.go" <<'GO'
package ziran
import "testing"
func TestQualified(t *testing.T) {
    if Qualified_Answer() != 42 { t.Fatal(Qualified_Answer()) }
}
GO
            GO111MODULE=off go test "$output"/*.go
        elif test "$target" = c; then
            cat > "$output/main.c" <<'C'
#include "qualified.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
            "${CC:-cc}" -std=c11 -I"$repo/include" -I"$output" \
                "$output"/*.c -o "$output/app"
            "$output/app"
        else
            cat > "$output/main.cpp" <<'CPP'
#include "qualified.hpp"
int main() { return Answer() == 42 ? 0 : 1; }
CPP
            "${CXX:-c++}" -std=c++17 -I"$repo/include" -I"$output" \
                "$output"/*.cpp -o "$output/app"
            "$output/app"
        fi
    done
done
echo 'qualified pointers and slices of imported records: C, C++, and Go passed'
