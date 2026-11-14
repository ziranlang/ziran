#!/bin/sh
# A Vec can hold records that own Vecs: VecPush moves the element in, and
# indexing, member paths, `for *`, mutation through an index, VecPop, VecClear
# and scope exit all reach or drop the inner storage. Copying an owned element
# out stays rejected. A Vec directly inside a Vec works the same way; wasm
# keeps rejecting Vec calls. Source and saved IR agree on C (allocation-counted and
# under AddressSanitizer leak checking), Go and portable `.zx`.
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/groups.zi" <<'ZI'
#import "vec"
#import "option"

Group :: struct { name: string; parts: Vec(string); }
Shelf :: struct { label: string; groups: Vec(Group); }

MakeGroup :: (name: string, part: string) -> Group {
    parts: Vec(string)
    VecPush(parts, part)
    return .{name = name, parts = parts}
}

CountParts :: (groups: *Vec(Group)) -> s64 {
    total: s64 = 0
    i: s64 = 0
    while i < groups.count {
        total += groups.*[i].parts.count
        i += 1
    }
    return total
}

Describe :: (line: *Vec(u8), groups: *Vec(Group), index: s64) {
    BuilderPrint(line, "%:%", groups.*[index].name, groups.*[index].parts[0])
}

Strings :: (value: string) -> Vec(string) {
    values: Vec(string)
    VecPush(values, value)
    return values
}

Row :: (value: s32) -> Vec(s32) {
    row: Vec(s32)
    VecPush(row, value)
    return row
}

#program_export
Check :: () -> s32 {
    groups: Vec(Group)
    first: Group
    first.name = "one"
    VecPush(first.parts, "x")
    VecPush(first.parts, "y")
    VecPush(groups, first)
    VecPush(groups, MakeGroup("two", "a"))
    parts: Vec(string)
    VecPush(parts, "p")
    VecPush(groups, .{name = "three", parts = parts})
    if groups.count != 3 || groups[0].parts.count != 2 { return 1 }
    if groups[0].parts[1] != "y" || groups[1].parts[0] != "a" { return 2 }
    groups[0].parts[1] = "z"
    VecPush(groups[1].parts, "b")
    groups[2].name = "third"
    if groups[0].parts[1] != "z" || groups[1].parts[1] != "b" { return 3 }
    if groups[2].name != "third" || groups[2].parts[0] != "p" { return 4 }
    seen: s64 = 0
    for * groups {
        seen += it.parts.count
        VecPush(it.parts, it.name)
    }
    if seen != 5 || groups[2].parts[1] != "third" { return 5 }
    i: s64 = 0
    while i < groups.count {
        j: s64 = 0
        while j < groups[i].parts.count {
            if groups[i].parts[j].count == 0 { return 6 }
            j += 1
        }
        i += 1
    }
    label: Vec(u8)
    Describe(*label, *groups, 1)
    if label.count != 5 || label[0] != #char "t" || label[4] != #char "a" { return 7 }
    popped := VecPop(groups)
    if !popped.has_value || popped.value.name != "third" { return 8 }
    if popped.value.parts.count != 2 { return 8 }
    VecPush(groups, MakeGroup("fourth", "f"))
    shelves: Vec(Shelf)
    shelf: Shelf
    shelf.label = "top"
    VecPush(shelf.groups, MakeGroup("deep", "d"))
    VecPush(shelves, shelf)
    VecPush(shelves[0].groups[0].parts, "e")
    if shelves[0].groups[0].parts[1] != "e" { return 9 }
    VecClear(shelves)
    VecPush(shelves, .{label = "again", groups = groups})
    VecPush(shelves[0].groups, .{name = "fifth", parts = Strings("g")})
    if CountParts(*shelves[0].groups) != 8 { return 10 }
    VecPop(shelves)
    if shelves.count != 0 { return 10 }
    MakeGroup("dropped", "d")
    grid: [..][..]s32
    row: [..]s32
    array_add(*row, 1)
    VecPush(grid, row)
    VecPush(grid, Row(3))
    grid[1][0] += 1
    VecPush(grid[0], 5)
    cells: s64 = 0
    for * grid { cells += it.count }
    if cells != 3 || grid[0][1] != 5 || grid[1][0] != 4 { return 11 }
    dropped := VecPop(grid)
    if !dropped.has_value || dropped.value[0] != 4 { return 12 }
    VecResize(grid, 3)
    if grid.count != 3 || grid[2].count != 0 { return 13 }
    VecFree(grid)
    return 42
}
ZI

reject() {
    name=$1
    message=$2
    cat > "$work/$name.zi"
    if "$ziran" check --root "$work" --module-path "$repo/std" \
        "$work/$name.zi" 2> "$work/$name.err"; then
        echo "$name was accepted" >&2
        exit 1
    fi
    grep -Fq "$message" "$work/$name.err" || {
        cat "$work/$name.err" >&2
        exit 1
    }
}

reject use_after_push 'used after moving' <<'ZI'
#import "vec"
Group :: struct { parts: Vec(s32); }
Check :: () -> s64 {
    groups: Vec(Group)
    group: Group
    VecPush(group.parts, 1)
    VecPush(groups, group)
    return group.parts.count
}
ZI
reject copied_element 'owned Vec elements cannot be copied' <<'ZI'
#import "vec"
Group :: struct { parts: Vec(s32); }
Check :: () -> s64 {
    groups: Vec(Group)
    for groups { if it.parts.count > 0 { return 1 } }
    return 0
}
ZI
reject copied_row 'owned Vec elements cannot be copied' <<'ZI'
#import "vec"
Check :: () -> s64 {
    grid: Vec(Vec(s32))
    VecResize(grid, 1)
    row := grid[0]
    return row.count
}
ZI
reject indexed_source 'moves a binding or takes a call result' <<'ZI'
#import "vec"
Group :: struct { parts: Vec(s32); }
Check :: () -> s64 {
    groups: Vec(Group)
    others: Vec(Group)
    VecPush(others, .{})
    VecPush(groups, others[0])
    return groups.count
}
ZI
reject global_source 'global Vec storage would move into the Vec' <<'ZI'
#import "vec"
Group :: struct { parts: Vec(s32); }
shared: Group;
Check :: () -> s64 {
    groups: Vec(Group)
    VecPush(groups, shared)
    return groups.count
}
ZI
"$ziran" check --root "$work" --module-path "$repo/std" "$work/groups.zi"
"$ziran" ir --root "$work" --module-path "$repo/std" \
    -o "$work/ir" "$work/groups.zi"
for kind in source saved; do
    if test "$kind" = source; then
        root=$work
        module=$work/groups.zi
    else
        root=$work/ir
        module=$work/ir/groups.zir
    fi
    "$ziran" bundle --entry groups:Check --root "$root" \
        --module-path "$repo/std" -o "$work/$kind.zx" "$module"
    test "$("$ziran" run "$work/$kind.zx")" = 42

    c_output=$work/$kind-c
    "$ziran" build --target=c --entry groups:Check --root "$root" \
        --module-path "$repo/std" -o "$c_output" "$module"
    cat > "$work/count.c" <<'C'
#include "groups.h"
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
    if (Check() != 42) return 1;
    return outstanding == 0 ? 0 : 2;
}
C
    "${CC:-cc}" -std=c11 -I"$repo/include" -I"$c_output" \
        "$c_output"/*.c "$work/count.c" -Wl,--wrap=realloc -Wl,--wrap=free \
        -o "$work/count-$kind"
    "$work/count-$kind"
    cat > "$work/plain.c" <<'C'
#include "groups.h"
int main(void) { return Check() == 42 ? 0 : 1; }
C
    "${CC:-cc}" -std=c11 -g -fsanitize=address,undefined \
        -fno-sanitize-recover=undefined -fno-omit-frame-pointer \
        -I"$repo/include" -I"$c_output" "$c_output"/*.c "$work/plain.c" \
        -o "$work/asan-$kind"
    ASAN_OPTIONS=detect_leaks=1 "$work/asan-$kind"

    go_output=$work/$kind-go
    "$ziran" build --target=go --pkg main --entry groups:Check \
        --root "$root" --module-path "$repo/std" \
        -o "$go_output" "$module"
    cat > "$go_output/main.go" <<'GO'
package main
func main() { if Groups_Check() != 42 { panic("nested Vec elements") } }
GO
    GO111MODULE=off go run "$go_output"/*.go
done

cmp "$work/source.zx" "$work/saved.zx"
cmp "$work/source-c/groups.c" "$work/saved-c/groups.c"
cmp "$work/source-go/groups.go" "$work/saved-go/groups.go"

cat > "$work/wasm.zi" <<'ZI'
#import "vec"
Group :: struct { parts: Vec(s32); }
#program_export
Check :: () -> s32 {
    groups: Vec(Group)
    group: Group
    VecPush(group.parts, 1)
    VecPush(groups, group)
    return groups[0].parts[0]
}
ZI
# Wasm keeps nested vectors on its heap like the other routes.
"$(dirname "$ziran")/zi2asm" --root "$work" --module-path "$repo/std" \
    --entry wasm:Check --emit wasm -o "$work/wasm.wasm" "$work/wasm.zi"
test "$(node "$repo/scripts/site_target_wasm.cjs" "$work/wasm.wasm")" = 1
