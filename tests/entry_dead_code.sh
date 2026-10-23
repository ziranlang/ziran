#!/bin/sh
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/flow.zi" <<'ZI'
hits: s32;

Dead :: () -> s32 {
    return 90
}
Live :: () -> s32 {
    return 7
}
Touch :: () -> bool {
    hits += 1
    return true
}
Choice :: (flag: bool) -> s32 {
    if flag {
        return 3
    } else {
        return 4
    }
    return Dead()
}
Nested :: () -> s32 {
    if true {
        if false {
            return Dead()
        } else {
            return 4
        }
    }
    return Dead()
}

#program_export
Answer :: () -> s32 {
    if false && Touch() {
        return Dead()
    }
    if false {
        return Dead()
    } else if true {
        hits += 0
    }
    while false {
        hits += Dead()
    }
    if Touch() && false {
        hits += 100
    }
    value: s32 = ifx true then Live() else Dead()
    return value + Choice(true) + Nested() + hits * 10
    hits += Dead()
}
ZI

"$ziran" ir --root "$work" -o "$work/ir" "$work/flow.zi"
for input in source saved; do
    if test "$input" = source; then module=$work/flow.zi
    else module=$work/ir/flow.zir; fi
    "$ziran" bundle --root "$work" --entry flow:Answer \
        -o "$work/$input.zx" "$module"
    test "$("$ziran" run "$work/$input.zx")" = 24
    for target in c cpp go; do
        output="$work/$target-$input"
        if test "$target" = go; then
            "$ziran" build --target=go --pkg main --entry flow:Answer \
                --root "$work" -o "$output" "$module"
            if rg -q 'func Flow_Dead\(' "$output/flow.go"; then exit 1; fi
            cat > "$output/main.go" <<'GO'
package main
func main() { if Flow_Answer() != 24 { panic("wrong result") } }
GO
            GO111MODULE=off go run "$output"/*.go
        elif test "$target" = c; then
            "$ziran" build --target=c --entry flow:Answer --root "$work" \
                -o "$output" "$module"
            if rg -q 'Dead\(' "$output/flow.c"; then exit 1; fi
            cat > "$output/main.c" <<'C'
#include "flow.h"
int main(void) { return Answer() == 24 ? 0 : 1; }
C
            "${CC:-cc}" -std=c99 -pedantic-errors -I"$repo/include" \
                -I"$output" "$output"/*.c -o "$output/app"
            "$output/app"
        else
            "$ziran" build --target=cpp --entry flow:Answer --root "$work" \
                -o "$output" "$module"
            if rg -q 'Dead\(' "$output/flow.cpp"; then exit 1; fi
            cat > "$output/main.cpp" <<'CPP'
#include "flow.hpp"
int main() { return Answer() == 24 ? 0 : 1; }
CPP
            "${CXX:-c++}" -std=c++17 -I"$repo/include" -I"$output" \
                "$output"/*.cpp -o "$output/app"
            "$output/app"
        fi
    done
done
cmp "$work/source.zx" "$work/saved.zx"
