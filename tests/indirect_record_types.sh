#!/bin/sh
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

# A user reads fields of an imported record whose field types come from a
# module it does not import, and keeps a returned record without naming its
# type, as a widget library's results are used through one public module.
cat > "$work/inner.zi" <<'ZI'
Inner :: struct {
    flag: bool
    count: s32
}
ZI
cat > "$work/outer.zi" <<'ZI'
#import "inner"
Outer :: struct {
    inner: Inner
    value: s32
}
#program_export
MakeOuter :: () -> Outer {
    result: Outer
    result.inner.flag = true
    result.inner.count = 40
    result.value = 1
    return result
}
#program_export
MakeInner :: () -> Inner {
    result: Inner
    result.count = 1
    return result
}
ZI
cat > "$work/tree.zi" <<'ZI'
#import "outer"
#program_export
Answer :: () -> s32 {
    made: Outer = MakeOuter()
    if !made.inner.flag { return 0 }
    loose := MakeInner()
    copy := made.inner
    return made.inner.count + made.value + loose.count + copy.count - 40
}
ZI
cat > "$work/hidden.zi" <<'ZI'
#import "outer"
#program_export
Hidden :: () -> s32 {
    named: Inner = MakeInner()
    return named.count
}
ZI

"$ziran" check --root "$work" "$work/tree.zi"
if "$ziran" check --root "$work" "$work/hidden.zi" 2> "$work/hidden.err"; then
    echo 'an unimported type name resolved without a qualifier' >&2
    exit 1
fi
"$ziran" ir --root "$work" -o "$work/ir" "$work/tree.zi"
for input in source saved; do
    if test "$input" = source; then
        source=$work/tree.zi
        root=$work
    else
        source=$work/ir/tree.zir
        root=$work/ir
    fi
    "$ziran" bundle --root "$root" --entry tree:Answer \
        -o "$work/$input.zib" "$source"
    test "$("$ziran" run "$work/$input.zib")" = 42
    for target in c cpp go; do
        output="$work/$target-$input"
        "$ziran" build --target="$target" --root "$root" \
            -o "$output" "$source"
        if test "$target" = go; then
            cat > "$output/main_test.go" <<'GO'
package ziran
import "testing"
func TestIndirect(t *testing.T) {
    if Tree_Answer() != 42 { t.Fatal("indirect record result") }
}
GO
            GO111MODULE=off go test "$output"/*.go
        elif test "$target" = c; then
            cat > "$output/main.c" <<'C'
#include "tree.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
            "${CC:-cc}" -std=c11 -I"$repo/include" -I"$output" \
                "$output"/*.c -o "$output/app"
            "$output/app"
        else
            cat > "$output/main.cpp" <<'CPP'
#include "tree.hpp"
int main() { return Answer() == 42 ? 0 : 1; }
CPP
            "${CXX:-c++}" -std=c++17 -I"$repo/include" -I"$output" \
                "$output"/*.cpp -o "$output/app"
            "$output/app"
        fi
    done
done
echo 'indirect record types: checker, VM, C, C++, and Go passed'
