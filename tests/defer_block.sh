#!/bin/sh
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/deferred.zi" <<'ZI'
trace: s32;
Record :: (value: s32) {
    trace = trace * 10 + value
}
Run :: (early: bool) {
    trace = 0
    defer {
        Record(1)
        if early { Record(2) }
    }
    defer Record(3)
    if early { return }
    Record(4)
}
Nested :: () {
    trace = 0
    {
        defer {
            Record(5)
            Record(6)
        }
        defer { Record(7) }
    }
}
Loop :: () {
    trace = 0
    index: s32 = 0
    while index < 2 {
        value: s32 = index + 1
        defer { Record(value) }
        index += 1
        if index == 1 { continue }
        break
    }
}
#program_export
Answer :: () -> s32 {
    defer {}
    Run(true)
    if trace != 312 { return 0 }
    Run(false)
    if trace != 431 { return 0 }
    Nested()
    if trace != 756 { return 0 }
    Loop()
    if trace != 12 { return 0 }
    return 42
}
ZI

"$ziran" ir --root "$work" -o "$work/ir" "$work/deferred.zi"
for input in source saved; do
    if test "$input" = source; then
        set -- "$work/deferred.zi"
    else
        set -- "$work/ir/deferred.zir"
    fi
    "$ziran" bundle --root "$work" --entry deferred:Answer \
        -o "$work/$input.zx" "$@"
    test "$("$ziran" run "$work/$input.zx")" = 42
    for target in c cpp go; do
        out="$work/$input-$target"
        if test "$target" = go; then
            "$ziran" build --target=go --pkg main --root "$work" \
                -o "$out" "$@"
        else
            "$ziran" build --target="$target" --root "$work" \
                -o "$out" "$@"
        fi
        if test "$target" = c; then
            cat > "$out/main.c" <<'C'
#include "deferred.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
            "${CC:-cc}" -std=c11 -I"$repo/include" -I"$out" \
                "$out/deferred.c" "$out/main.c" -o "$out/app"
            "$out/app"
        elif test "$target" = cpp; then
            cat > "$out/main.cpp" <<'CPP'
#include "deferred.hpp"
int main() { return Answer() == 42 ? 0 : 1; }
CPP
            "${CXX:-c++}" -std=c++17 -I"$repo/include" -I"$out" \
                "$out/deferred.cpp" "$out/main.cpp" -o "$out/app"
            "$out/app"
        else
            cat > "$out/main.go" <<'GO'
package main
func main() { if Deferred_Answer() != 42 { panic("defer block") } }
GO
            GO111MODULE=off go run "$out/deferred.go" "$out/main.go"
        fi
    done
done
cmp "$work/source.zx" "$work/saved.zx"
