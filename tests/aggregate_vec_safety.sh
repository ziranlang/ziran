#!/bin/sh
set -eu

ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/owned.zi" <<'ZI'
#import "vec"
Inner :: struct { items: Vec(s32); }
Outer :: struct { inners: [2]Inner; empty: [0]Inner; extra: Vec(s32); }
FieldOuter :: struct { inner: Inner; other: Vec(s32); }
Make :: () -> Outer {
    value: Outer
    VecPush(value.inners[0].items, 3)
    VecPush(value.inners[1].items, 4)
    VecPush(value.inners[0].items, 5)
    VecPush(value.inners[1].items, 6)
    VecPush(value.extra, 7)
    return value
}
Take :: (value: Outer) -> s32 {
    return value.inners[0].items[0] + value.inners[0].items[1] +
           value.inners[1].items[0] + value.inners[1].items[1] +
           value.extra[0]
}
Relay :: (value: Outer) -> Outer { return value }
Count :: (value: Vec(s32)) -> s64 { return value.count }
FieldMake :: () -> Inner {
    items: Vec(s32)
    VecPush(items, 11)
    return Inner.{items = items}
}
BranchMove :: (value: Inner) -> s32 {
    if value.items.count > 0 {
        moved := value.items
        return moved[0]
    }
    return 0
}
#program_export
Check :: () -> s32 {
    made := Make()
    first := Relay(made)
    grown: Outer
    VecPush(grown.inners[0].items, 8)
    VecPush(grown.inners[1].items, 9)
    VecPush(grown.inners[0].items, 10)
    VecPush(grown.inners[1].items, 11)
    VecPush(grown.extra, 12)
    field := FieldMake()
    literal_items: Vec(s32)
    VecPush(literal_items, 13)
    literal := Inner.{items = literal_items}
    field_source: FieldOuter
    VecPush(field_source.inner.items, 14)
    VecPush(field_source.other, 15)
    field_first := field_source.inner.items
    field_second := field_source.other
    branch_source: Inner
    VecPush(branch_source.items, 16)
    branch := BranchMove(branch_source)
    return Take(first) + Take(grown) + branch + field.items[0] +
           literal.items[0] + cast(s32)Count(field_first) +
           cast(s32)Count(field_second)
}
ZI
cat > "$work/field_use_after_move.zi" <<'ZI'
#import "vec"
Inner :: struct { items: Vec(s32); }
Check :: () -> s32 {
    items: Vec(s32)
    VecPush(items, 1)
    value := Inner.{items = items}
    return cast(s32)items.count + value.items[0]
}
ZI

cat > "$work/field_sibling_and_use_after_move.zi" <<'ZI'
#import "vec"
Inner :: struct { items: Vec(s32); other: Vec(s32); }
Check :: () -> s32 {
    value: Inner
    VecPush(value.items, 1)
    VecPush(value.other, 2)
    moved := value.items
    return moved[0] + value.items[0]
}
ZI

cat > "$work/field_move_enclosing_record.zi" <<'ZI'
#import "vec"
Inner :: struct { items: Vec(s32); }
Check :: () -> s32 {
    value: Inner
    VecPush(value.items, 1)
    moved := value.items
    whole := value
    unused whole
    return moved[0]
}
ZI

cat > "$work/global_nested_field_move.zi" <<'ZI'
#import "vec"
Inner :: struct { items: Vec(s32); }
Box :: struct { inner: Inner; }
box: Box;
Check :: () -> s32 {
    moved := box.inner.items
    return cast(s32)moved.count
}
ZI

cat > "$work/indexed_field_move.zi" <<'ZI'
#import "vec"
Inner :: struct { items: Vec(s32); }
Box :: struct { inners: [2]Inner; }
Check :: () -> s32 {
    value: Box
    VecPush(value.inners[0].items, 1)
    moved := value.inners[0].items
    return moved[0]
}
ZI

cat > "$work/pointer_field_move.zi" <<'ZI'
#import "vec"
Inner :: struct { items: Vec(s32); }
Box :: struct { inner: *Inner; }
Check :: () -> s32 {
    storage: Inner
    VecPush(storage.items, 1)
    value: Box
    value.inner = *storage
    moved := value.inner.items
    return moved[0]
}
ZI

cat > "$work/global_field_move.zi" <<'ZI'
#import "vec"
Inner :: struct { items: Vec(s32); }
items: Vec(s32);
Check :: () -> s32 {
    value := Inner.{items = items}
    return cast(s32)value.items.count
}
ZI

cat > "$work/use_after_move.zi" <<'ZI'
#import "vec"
Inner :: struct { items: Vec(s32); }
Outer :: struct { inner: Inner; }
Check :: () -> s32 {
    first: Outer
    VecPush(first.inner.items, 1)
    second := first
    VecFree(first.inner.items)
    return second.inner.items[0]
}
ZI
cat > "$work/overwrite.zi" <<'ZI'
#import "vec"
Inner :: struct { items: Vec(s32); }
Outer :: struct { inner: Inner; }
Check :: () -> s32 {
    first: Outer
    second: Outer
    VecPush(first.inner.items, 1)
    VecPush(second.inner.items, 2)
    second = first
    return 0
}
ZI

for name in field_use_after_move field_sibling_and_use_after_move field_move_enclosing_record indexed_field_move pointer_field_move global_field_move global_nested_field_move use_after_move; do
    if "$ziran" check --diagnostics=json --root "$work" \
    --module-path "$repo/std" "$work/$name.zi" \
    > "$work/$name.out" 2> "$work/$name.err"; then
    echo "$name was accepted despite invalid Vec field ownership" >&2
    exit 1
fi
done
if "$ziran" check --diagnostics=json --root "$work" \
    --module-path "$repo/std" "$work/overwrite.zi" \
    > "$work/overwrite.out" 2> "$work/overwrite.err"; then
    echo 'assignment over live aggregate Vec storage was accepted' >&2
    exit 1
fi
python3 - "$work/field_use_after_move.err" \
         "$work/field_sibling_and_use_after_move.err" \
         "$work/field_move_enclosing_record.err" \
         "$work/indexed_field_move.err" \
         "$work/pointer_field_move.err" \
         "$work/global_field_move.err" \
         "$work/global_nested_field_move.err" \
         "$work/use_after_move.err" "$work/overwrite.err" <<'PY'
import json
from pathlib import Path
import sys

for path, expected in [(sys.argv[1], 'used after moving'),
                       (sys.argv[2], 'owned record field is used after moving'),
                       (sys.argv[3], 'owned aggregate is used after moving'),
                       (sys.argv[4], 'moves a binding or takes a call result'),
                       (sys.argv[5], 'moves a binding or takes a call result'),
                       (sys.argv[6], 'global Vec storage cannot move'),
                       (sys.argv[7], 'global Vec storage cannot move'),
                       (sys.argv[8], 'used after moving'),
                       (sys.argv[9], 'leaks it')]:
    diagnostics = [json.loads(line)
                   for line in Path(path).read_text().splitlines()]
    assert any(expected in item['message'] for item in diagnostics), diagnostics
PY

"$ziran" check --root "$work" --module-path "$repo/std" "$work/owned.zi"
"$ziran" ir --root "$work" --module-path "$repo/std" \
    -o "$work/ir" "$work/owned.zi"
for kind in source saved; do
    if test "$kind" = source; then
        root=$work
        module=$work/owned.zi
    else
        root=$work/ir
        module=$work/ir/owned.zir
    fi
    "$ziran" bundle --entry owned:Check --root "$root" \
        --module-path "$repo/std" -o "$work/$kind.zib" "$module"
    test "$("$ziran" run "$work/$kind.zib")" = 117

    c_output=$work/$kind-c
    "$ziran" build --target=c --entry owned:Check --root "$root" \
        --module-path "$repo/std" -o "$c_output" "$module"
    cat > "$c_output/main.c" <<'C'
#include "owned.h"
#include <stddef.h>
#include <stdlib.h>
void *__real_realloc(void *, size_t);
void __real_free(void *);
static int outstanding;
void *__wrap_realloc(void *p, size_t size) {
    void *result = __real_realloc(p, size);
    if (p == NULL && result != NULL) outstanding++;
    return result;
}
void __wrap_free(void *p) {
    if (p != NULL) outstanding--;
    __real_free(p);
}
int main(void) {
    if (Check() != 117) return 1;
    return outstanding == 0 ? 0 : 2;
}
C
    "${CC:-cc}" -std=c11 -I"$repo/include" -I"$c_output" \
        "$c_output"/*.c -Wl,--wrap=realloc -Wl,--wrap=free \
        -o "$work/test-$kind-c"
    "$work/test-$kind-c"

    cpp_output=$work/$kind-cpp
    "$ziran" build --target=cpp --entry owned:Check --root "$root" \
        --module-path "$repo/std" -o "$cpp_output" "$module"
    cat > "$cpp_output/main.cpp" <<'CPP'
#include "owned.hpp"
#include <cstddef>
extern "C" void *__real_realloc(void *, std::size_t);
extern "C" void __real_free(void *);
static int outstanding;
extern "C" void *__wrap_realloc(void *p, std::size_t size) {
    void *result = __real_realloc(p, size);
    if (p == nullptr && result != nullptr) outstanding++;
    return result;
}
extern "C" void __wrap_free(void *p) {
    if (p != nullptr) outstanding--;
    __real_free(p);
}
int main() {
    if (Check() != 117) return 1;
    return outstanding == 0 ? 0 : 2;
}
CPP
    "${CXX:-c++}" -std=c++17 -I"$repo/include" -I"$cpp_output" \
        "$cpp_output"/*.cpp -Wl,--wrap=realloc -Wl,--wrap=free \
        -o "$work/test-$kind-cpp"
    "$work/test-$kind-cpp"

    go_output=$work/$kind-go
    "$ziran" build --target=go --pkg main --entry owned:Check \
        --root "$root" --module-path "$repo/std" \
        -o "$go_output" "$module"
    cat > "$go_output/main.go" <<'GO'
package main
func main() { if Owned_Check() != 117 { panic("aggregate Vec ownership") } }
GO
    GO111MODULE=off go run "$go_output"/*.go
done

cmp "$work/source.zib" "$work/saved.zib"
cmp "$work/source-c/owned.c" "$work/saved-c/owned.c"
cmp "$work/source-c/owned.h" "$work/saved-c/owned.h"
cmp "$work/source-cpp/owned.cpp" "$work/saved-cpp/owned.cpp"
cmp "$work/source-go/owned.go" "$work/saved-go/owned.go"
