#!/bin/sh
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

# A module can declare far more than 256 procedures, and a module that
# imports it by name reaches every one of them, not only the first 256.
{
    index=1
    while test "$index" -le 300; do
        printf 'Step%d :: (value: s32) -> s32 {\n    return value + 1\n}\n' "$index"
        index=$((index + 1))
    done
} > "$work/many.zi"
# The linked build keeps only procedures the entry reaches, so the caller
# reaches all of them.
{
    printf 'Many :: #import "many";\n\n#program_export\nAnswer :: () -> s32 {\n'
    printf '    total: s32 = 0\n'
    index=1
    while test "$index" -le 300; do
        printf '    total = Many.Step%d(total)\n' "$index"
        index=$((index + 1))
    done
    printf '    return total - 258\n}\n'
} > "$work/caller.zi"

"$ziran" ir --root "$work" --entry caller:Answer -o "$work/ir" "$work/caller.zi"
for input in source saved; do
    if test "$input" = source; then
        source=$work/caller.zi
        root=$work
    else
        source=$work/ir/caller.zir
        root=$work/ir
    fi
    "$ziran" bundle --root "$root" --entry caller:Answer \
        -o "$work/$input.zib" "$source"
    test "$("$ziran" run "$work/$input.zib")" = 42
    for target in c cpp go; do
        output="$work/$target-$input"
        "$ziran" build --target="$target" --root "$root" \
            --entry caller:Answer -o "$output" "$source"
        if test "$target" = go; then
            cat > "$output/main_test.go" <<'GO'
package ziran
import "testing"
func TestMany(t *testing.T) {
    if Caller_Answer() != 42 { t.Fatal(Caller_Answer()) }
}
GO
            GO111MODULE=off go test "$output"/*.go
        elif test "$target" = c; then
            cat > "$output/main.c" <<'C'
#include "caller.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
            "${CC:-cc}" -std=c11 -I"$repo/include" -I"$output" \
                "$output"/*.c -o "$output/app"
            "$output/app"
        else
            cat > "$output/main.cpp" <<'CPP'
#include "caller.hpp"
int main() { return Answer() == 42 ? 0 : 1; }
CPP
            "${CXX:-c++}" -std=c++17 -I"$repo/include" -I"$output" \
                "$output"/*.cpp -o "$output/app"
            "$output/app"
        fi
    done
done
echo 'procedures past the 256th, called by name: VM, C, C++, and Go passed'
