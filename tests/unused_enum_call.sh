#!/bin/sh
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

# A call whose result goes unused is a statement of its own. Go writes an
# enum result as a conversion, which cannot stand alone as a statement.
cat > "$work/unused.zi" <<'ZI'
Status :: enum {
    StatusOk :: 0
    StatusBad :: 1
}

calls: s32 = 0;

Begin :: () -> Status {
    calls += 1
    return cast(Status)0
}

Count :: () -> s32 {
    calls += 10
    return calls
}

#program_export
Answer :: () -> s32 {
    Begin()
    Count()
    Begin()
    return calls + 10
}
ZI

"$ziran" ir --root "$work" --entry unused:Answer -o "$work/ir" "$work/unused.zi"
for input in source saved; do
    if test "$input" = source; then
        source=$work/unused.zi
        root=$work
    else
        source=$work/ir/unused.zir
        root=$work/ir
    fi
    "$ziran" bundle --root "$root" --entry unused:Answer \
        -o "$work/$input.zib" "$source"
    test "$("$ziran" run "$work/$input.zib")" = 22
    for target in c cpp go; do
        output="$work/$target-$input"
        "$ziran" build --target="$target" --root "$root" \
            --entry unused:Answer -o "$output" "$source"
        if test "$target" = go; then
            cat > "$output/main_test.go" <<'GO'
package ziran
import "testing"
func TestUnused(t *testing.T) {
    if Unused_Answer() != 22 { t.Fatal(Unused_Answer()) }
}
GO
            GO111MODULE=off go vet "$output"/*.go
            GO111MODULE=off go test "$output"/*.go
        elif test "$target" = c; then
            cat > "$output/main.c" <<'C'
#include "unused.h"
int main(void) { return Answer() == 22 ? 0 : 1; }
C
            "${CC:-cc}" -std=c11 -Wall -Werror -I"$repo/include" -I"$output" \
                "$output"/*.c -o "$output/app"
            "$output/app"
        else
            cat > "$output/main.cpp" <<'CPP'
#include "unused.hpp"
int main() { return Answer() == 22 ? 0 : 1; }
CPP
            "${CXX:-c++}" -std=c++17 -Wall -Werror -I"$repo/include" -I"$output" \
                "$output"/*.cpp -o "$output/app"
            "$output/app"
        fi
    done
done
echo 'unused call results, including enum results: VM, C, C++, and Go passed'
