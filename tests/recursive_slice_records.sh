#!/bin/sh
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

# A slice is a pointer and a count, so a record may hold a slice of itself,
# as it may hold a pointer to itself.
cat > "$work/tree.zi" <<'ZI'
Node :: struct {
    value: s32
    children: []Node
}

Sum :: (nodes: []Node) -> s32 {
    total: s32 = 0
    index: s32 = 0
    while index < cast(s32)nodes.count {
        total += nodes[index].value + Sum(nodes[index].children)
        index += 1
    }
    return total
}

#program_export
Answer :: () -> s32 {
    leaves: [2]Node
    leaves[0].value = 10
    leaves[1].value = 20
    branch: [1]Node
    branch[0].value = 5
    branch[0].children = leaves[:]
    root: [2]Node
    root[0].value = 4
    root[0].children = branch[:]
    root[1].value = 3
    if root[0].children[0].children[1].value != 20 { return 0 }
    return Sum(root[:])
}
ZI

cat > "$work/invalid_recursive_record.zi" <<'ZI'
Bad :: struct {
    value: s32
    child: Bad
}
ZI
if "$ziran" check --root "$work" "$work/invalid_recursive_record.zi" \
    2> "$work/invalid_recursive_record.err"; then
    echo 'record embedded itself by value' >&2
    exit 1
fi

"$ziran" bundle --root "$work" --entry tree:Answer \
    -o "$work/tree.zib" "$work/tree.zi"
test "$("$ziran" run "$work/tree.zib")" = 42
for target in c cpp go; do
    output="$work/$target"
    "$ziran" build --target="$target" --root "$work" \
        -o "$output" "$work/tree.zi"
    if test "$target" = go; then
        cat > "$output/main_test.go" <<'GO'
package ziran
import "testing"
func TestRecursiveSlice(t *testing.T) {
    if Tree_Answer() != 42 { t.Fatal("recursive slice result") }
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
echo 'recursive slice records: VM, C, C++, and Go passed'
