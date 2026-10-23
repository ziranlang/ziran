#!/bin/sh
set -eu

compiler=${1:-build/bin/zi2zir}
bin=$(CDPATH= cd -- "$(dirname -- "$compiler")" && pwd)
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/types.zi" <<'ZI'
Child :: struct {
    value: s32
}
Node :: struct {
    value: s32
    child: Child
    values: [4]s32
}
ZI

cat > "$work/read.zi" <<'ZI'
#import "types"
#program_export
Read :: (node: *Node) -> s32 {
    return node.value + node.child.value
}
#program_export
Write :: (node: *Node, value: s32) -> s32 {
    if node == null { return -1 }
    node.value = value
    node.child.value = value + 1
    return Read(node)
}
#program_export
Forward :: (node: *Node) -> *Node {
    return node
}
#program_export
WriteForward :: (node: *Node, value: s32) -> s32 {
    if node == null { return -1 }
    Forward(node).value = value
    return node.value
}
#program_export
BorrowArray :: (node: *Node) -> s32 {
    if node == null { return -1 }
    values := node.values[:]
    values[1] = 42
    return node.values[1]
}
#program_export
ReadIndexed :: (values: *s32, index: s32) -> s32 {
    if values == null || index < 0 || index >= 4 { return -1 }
    return values[index]
}
#program_export
WriteIndexed :: (values: *s32, index: s32, value: s32) -> s32 {
    if values == null || index < 0 || index >= 4 { return -1 }
    values[index] = value
    return values[index]
}
NextIndex :: (counter: *s32) -> s32 {
    counter.* += 1
    return 1
}
#program_export
ReadNextIndex :: (values: *s32, counter: *s32) -> s32 {
    return values[NextIndex(counter)]
}
ZI
"$compiler" --check-only --root "$work" "$work/read.zi"
"$compiler" --root "$work" -o "$work/ir" "$work/read.zi"
for input in source saved; do
    if test "$input" = source; then
        root=$work
        file=$work/read.zi
    else
        root=$work/ir
        file=$work/ir/read.zir
    fi
    output=$work/$input

    "$bin/zi2c" --no-main --root "$root" -o "$output/c" "$file"
    cat > "$output/c/main.c" <<'C'
#include "read.h"
int main(void) {
    Node node = {0};
    int32_t reads = 0;
    return Write(&node, 20) == 41 && Read(&node) == 41 &&
           WriteForward(&node, 23) == 23 && node.value == 23 &&
           node.child.value == 21 && BorrowArray(&node) == 42 &&
           node.values[1] == 42 &&
           ReadIndexed(&node.values[0], 1) == 42 &&
           WriteIndexed(&node.values[0], 2, 37) == 37 &&
           node.values[2] == 37 &&
           ReadNextIndex(&node.values[0], &reads) == 42 && reads == 1 &&
           ReadIndexed(&node.values[0], 4) == -1 ? 0 : 1;
}
C
    "${CC:-cc}" -std=c11 -I"$repo/include" -I"$output/c" \
        "$output/c/read.c" "$output/c/main.c" \
        -o "$output/c/test"
    env -u DISPLAY -u WAYLAND_DISPLAY "$output/c/test"

    "$bin/zi2cpp" --no-main --root "$root" -o "$output/cpp" "$file"
    cat > "$output/cpp/main.cpp" <<'CPP'
#include "read.hpp"
int main() {
    Node node = {};
    int32_t reads = 0;
    return Write(&node, 20) == 41 && Read(&node) == 41 &&
           WriteForward(&node, 23) == 23 && node.value == 23 &&
           node.child.value == 21 && BorrowArray(&node) == 42 &&
           node.values[1] == 42 &&
           ReadIndexed(&node.values[0], 1) == 42 &&
           WriteIndexed(&node.values[0], 2, 37) == 37 &&
           node.values[2] == 37 &&
           ReadNextIndex(&node.values[0], &reads) == 42 && reads == 1 &&
           ReadIndexed(&node.values[0], 4) == -1 ? 0 : 1;
}
CPP
    "${CXX:-c++}" -std=c++17 -I"$repo/include" -I"$output/cpp" \
        "$output/cpp/read.cpp" "$output/cpp/main.cpp" \
        -o "$output/cpp/test"
    env -u DISPLAY -u WAYLAND_DISPLAY "$output/cpp/test"

    "$bin/zi2go" --no-main --root "$root" -o "$output/go" "$file"
    cat > "$output/go/read_test.go" <<'GO'
package ziran
import "testing"
func TestPointerMember(t *testing.T) {
    node := Node{}
    reads := int32(0)
    if Read_Write(&node, 20) != 41 || Read_Read(&node) != 41 ||
       Read_WriteForward(&node, 23) != 23 || node.Value != 23 ||
       node.Child.Value != 21 || Read_BorrowArray(&node) != 42 ||
       node.Values[1] != 42 ||
       Read_ReadIndexed(&node.Values[0], 1) != 42 ||
       Read_WriteIndexed(&node.Values[0], 2, 37) != 37 ||
       node.Values[2] != 37 ||
       Read_ReadNextIndex(&node.Values[0], &reads) != 42 || reads != 1 ||
       Read_ReadIndexed(&node.Values[0], 4) != -1 {
        t.Fatal("pointer member read/write changed")
    }
}
GO
    if ! GO111MODULE=off go test "$output/go"/*.go; then
        sed -n '35,65p' "$output/go/read.go" >&2
        exit 1
    fi
done

cat > "$work/escape_slice.zi" <<'ZI'
#import "types"
Bad :: (node: *Node) -> []s32 {
    return node.values[:]
}
ZI
if "$compiler" --check-only --root "$work" "$work/escape_slice.zi" \
    >"$work/out" 2>"$work/err"; then
    echo "slice borrowed through pointer escaped its call" >&2
    exit 1
fi
grep -q 'returned slice borrows local or temporary storage' "$work/err"

if "$bin/zi2zx" bundle --root "$work" --entry read:Write \
    -o "$work/read.zx" "$work/read.zi" 2>"$work/bundle.err"; then
    echo "native pointer entered portable bundle" >&2
    exit 1
fi

cat > "$work/wrong_arg.zi" <<'ZI'
#import "types"
Mutate :: (node: *Node) -> s32 {
    node.value = 1
    return node.value
}
Bad :: (node: Node) -> s32 {
    return Mutate(node)
}
ZI
if "$compiler" --check-only --root "$work" "$work/wrong_arg.zi" \
    >"$work/out" 2>"$work/err"; then
    echo "record value passed to pointer parameter" >&2
    exit 1
fi
grep -q 'argument type mismatch: Mutate' "$work/err"

cat > "$work/nested_bad.zi" <<'ZI'
#import "types"
ReadNested :: (node: **Node) -> s32 {
    return 1
}
BadNested :: (node: *Node) -> s32 {
    return ReadNested(node)
}
ZI
if "$compiler" --check-only --root "$work" "$work/nested_bad.zi" \
    >"$work/out" 2>"$work/err"; then
    echo "single pointer passed to double-pointer parameter" >&2
    exit 1
fi
if ! grep -q 'argument type mismatch: ReadNested' "$work/err"; then
    cat "$work/err" >&2
    exit 1
fi

sed 's/node.value/node.missing/' "$work/read.zi" > "$work/bad.zi"
if "$compiler" --check-only --root "$work" "$work/bad.zi" \
    >"$work/out" 2>"$work/err"; then
    echo "unknown pointer field unexpectedly passed checking" >&2
    exit 1
fi
grep -q 'unknown record field: missing' "$work/err"

cat > "$work/not_record.zi" <<'ZI'
Bad :: (value: s32) -> s32 {
    return value.field
}
ZI
if "$compiler" --check-only --root "$work" "$work/not_record.zi" \
    >"$work/out" 2>"$work/err"; then
    echo "scalar field access unexpectedly passed checking" >&2
    exit 1
fi
grep -q 'unknown record field: field' "$work/err"

cat > "$work/arrow.zi" <<'ZI'
#import "types"
Bad :: (node: *Node) -> s32 {
    return node->value
}
ZI
if "$compiler" --check-only --root "$work" "$work/arrow.zi" \
    >"$work/out" 2>"$work/err"; then
    echo "C-style pointer member access was accepted" >&2
    exit 1
fi
grep -q 'C-style pointer member access is not valid Jai syntax' "$work/err"

echo "pointer member checking and emission test passed"
