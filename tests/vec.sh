#!/bin/sh
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
#import "vec"
#import "option"

Item :: struct {
    value: s32
    label: string
}

items: Vec(Item);

PopDrain :: () -> s32 {
    values: Vec(s32)
    VecPush(values, 10)
    VecPush(values, 20)
    VecPush(values, 30)
    result: s32 = 0
    popped: Option(s32) = VecPop(values)
    if !popped.has_value { VecFree(values); return -10 }
    result += popped.value
    found: Option(s32) = VecGet(values, 0)
    if !found.has_value { VecFree(values); return -11 }
    result += found.value
    missing: Option(s32) = VecGet(values, 9)
    if missing.has_value { VecFree(values); return -12 }
    while values.count > 0 {
        tail: Option(s32) = VecPop(values)
        if !tail.has_value { VecFree(values); return -13 }
        result += tail.value
    }
    drained: Option(s32) = VecPop(values)
    if drained.has_value { VecFree(values); return -14 }
    VecFree(values)
    return result
}

BuiltText :: () -> s32 {
    builder: Vec(u8)
    if !BuilderAppend(builder, "hello, ") { VecFree(builder); return -20 }
    if !BuilderAppend(builder, "ziran") { VecFree(builder); return -21 }
    text: string = BuilderFinish(builder)
    if text.count != 12 { return -22 }
    empty: Vec(u8)
    if !BuilderAppend(empty, "") { VecFree(empty); return -24 }
    if BuilderFinish(empty).count != 0 { return -25 }
    total: s64 = 0
    index: s64 = 0
    while index < text.count {
        total += cast(s64)text[index]
        index += 1
    }
    return cast(s32)total
}

RecordPop :: () -> s32 {
    entries: Vec(Item)
    entry: Item
    entry.value = 5
    entry.label = "five"
    VecPush(entries, entry)
    entry.value = 6
    entry.label = "six"
    VecPush(entries, entry)
    taken: Option(Item) = VecPop(entries)
    if !taken.has_value { VecFree(entries); return -30 }
    if taken.value.value != 6 { VecFree(entries); return -31 }
    if taken.value.label.count != 3 { VecFree(entries); return -32 }
    first: Option(Item) = VecGet(entries, 0)
    if !first.has_value { VecFree(entries); return -33 }
    if first.value.label.count != 4 { VecFree(entries); return -34 }
    VecFree(entries)
    return first.value.value + taken.value.value
}

ConsumeAll :: (values: Vec(s32)) -> s32 {
    total: s32 = 0
    while values.count > 0 {
        tail: Option(s32) = VecPop(values)
        if !tail.has_value { VecFree(values); return -40 }
        total += tail.value
    }
    VecFree(values)
    return total
}

MakeValues :: () -> Vec(s32) {
    fresh: Vec(s32)
    VecPush(fresh, 5)
    VecPush(fresh, 6)
    return fresh
}

Handoff :: (values: Vec(s32)) -> Vec(s32) {
    VecPush(values, 100)
    return values
}

AutoDropped :: () -> s32 {
    result: s32 = 0
    {
        inner: Vec(s32)
        VecPush(inner, 3)
        result += inner[0]
    }
    made: Vec(s32) = MakeValues()
    VecPush(made, 7)
    result += ConsumeAll(made)
    looped: Vec(s32)
    index: s32 = 0
    while index < 3 {
        VecPush(looped, index)
        if index == 2 { break }
        index += 1
    }
    result += looped[2]
    returned: Vec(s32) = MakeValues()
    result += returned[0]
    return result
}

ClonesAndViews :: () -> s32 {
    original: Vec(s32)
    VecPush(original, 4)
    VecPush(original, 5)
    VecPush(original, 6)
    copy: Vec(s32)
    if !VecClone(copy, original) { VecFree(original); VecFree(copy); return -60 }
    total: s32 = 0
    good: bool = true
    {
        view: []s32 = VecSlice(original, 1, 3)
        if view.count != 2 { good = false }
        if good { total += view[0] + view[1] }
        if good && VecGet(original, 0).value != 4 { good = false }
    }
    if !good { VecFree(original); VecFree(copy); return -61 }
    VecPush(copy, 7)
    if original.count != 3 || copy.count != 4 { VecFree(original); VecFree(copy); return -62 }
    total += copy[3] + original[0]
    VecFree(original)
    VecFree(copy)
    return total
}

DeferredWork :: () -> s32 {
    values: Vec(s32)
    defer { VecFree(values) }
    VecPush(values, 3)
    VecPush(values, 4)
    if values.count != 2 { return -50 }
    if VecGet(values, 1).value != 4 { return -51 }
    return 7
}

Moves :: () -> s32 {
    made: Vec(s32) = MakeValues()
    VecPush(made, 1)
    result: s32 = ConsumeAll(made)
    refill: Vec(s32) = MakeValues()
    VecPush(refill, 2)
    transferred: Vec(s32) = refill
    result += ConsumeAll(transferred)
    batch: Vec(s32) = MakeValues()
    batch = Handoff(batch)
    result += ConsumeAll(batch)
    return result
}

#program_export
Answer :: () -> s32 {
    index: s32 = 0
    while index < 100 {
        item: Item
        item.value = index
        item.label = "entry"
        if !VecPush(items, item) { return -1 }
        index += 1
    }
    if items.count != 100 || items.capacity < items.count {
        return -2
    }
    items[5].value += 1
    result: s32 = items[5].value + items[99].value
    VecClear(items)
    if items.count != 0 { return -3 }
    item: Item
    item.value = 1
    if !VecPush(items, item) { return -4 }
    result += items[0].value
    VecFree(items)
    if items.count != 0 || items.capacity != 0 { return -5 }
    if PopDrain() != 70 { return -6 }
    if BuiltText() != 1156 { return -7 }
    if RecordPop() != 11 { return -8 }
    if Moves() != 136 { return -9 }
    if DeferredWork() != 7 { return -10 }
    if ClonesAndViews() != 22 { return -11 }
    if AutoDropped() != 28 { return -12 }
    return result
}
ZI

"$ziran" ir --root "$work" --module-path "$repo/std" \
    -o "$work/ir" "$work/app.zi"
for input in source saved; do
    if test "$input" = source; then
        module=$work/app.zi
        root=$work
    else
        module=$work/ir/app.zir
        root=$work/ir
    fi
    "$ziran" bundle --root "$root" --module-path "$repo/std" \
        --entry app:Answer -o "$work/$input.zib" "$module"
    test "$("$ziran" run "$work/$input.zib")" = 106
    for target in c cpp go; do
        output="$work/$target-$input"
        if test "$target" = go; then
            "$ziran" build --target=go --pkg main --entry app:Answer \
                --root "$root" --module-path "$repo/std" \
                -o "$output" "$module"
            cat > "$output/main.go" <<'GO'
package main
func main() { if App_Answer() != 106 { panic("Vec result") } }
GO
            GO111MODULE=off go run "$output"/*.go
        elif test "$target" = c; then
            "$ziran" build --target=c --entry app:Answer --root "$root" \
                --module-path "$repo/std" -o "$output" "$module"
            cat > "$output/main.c" <<'C'
#include "app.h"
int main(void) { return Answer() == 106 ? 0 : 1; }
C
            "${CC:-cc}" -std=c99 -pedantic-errors -DZIRAN_BOUNDS_CHECK \
                -I"$repo/include" -I"$output" "$output"/*.c \
                -o "$output/app"
            "$output/app"
        else
            "$ziran" build --target=cpp --entry app:Answer --root "$root" \
                --module-path "$repo/std" -o "$output" "$module"
            cat > "$output/main.cpp" <<'CPP'
#include "app.hpp"
int main() { return Answer() == 106 ? 0 : 1; }
CPP
            "${CXX:-c++}" -std=c++17 -DZIRAN_BOUNDS_CHECK \
                -I"$repo/include" -I"$output" "$output"/*.cpp \
                -o "$output/app"
            "$output/app"
        fi
    done
done
cmp "$work/source.zib" "$work/saved.zib"

cat > "$work/private.zi" <<'ZI'
#import "vec"
items: Vec(s32);
#program_export
Bad :: () -> s32 { return items.data[0] }
ZI
if "$ziran" check --root "$work" --module-path "$repo/std" \
    "$work/private.zi" 2> "$work/private.err"; then
    echo 'Vec storage was accessible' >&2
    exit 1
fi
rg -q 'Vec storage is private' "$work/private.err"

cat > "$work/copy.zi" <<'ZI'
#import "vec"
items: Vec(s32);
#program_export
Bad :: () -> s32 {
    alias: Vec(s32) = items
    VecFree(alias)
    return 0
}
ZI
if "$ziran" check --root "$work" --module-path "$repo/std" \
    "$work/copy.zi" 2> "$work/copy.err"; then
    echo 'moving a global Vec was accepted' >&2
    exit 1
fi
rg -q 'global Vec storage cannot move' "$work/copy.err"

cat > "$work/usemove.zi" <<'ZI'
#import "vec"
Consume :: (values: Vec(s32)) -> s32 {
    VecFree(values)
    return 0
}
#program_export
Bad :: () -> s32 {
    values: Vec(s32)
    VecPush(values, 1)
    Consume(values)
    return cast(s32)values.count
}
ZI
if "$ziran" check --root "$work" --module-path "$repo/std" \
    "$work/usemove.zi" 2> "$work/usemove.err"; then
    echo 'use after move was accepted' >&2
    exit 1
fi
rg -q 'used after moving' "$work/usemove.err"

cat > "$work/owned.zi" <<'ZI'
#import "vec"
#program_export
Bad :: () -> s32 {
    a: Vec(s32)
    b: Vec(s32)
    VecPush(a, 1)
    a = b
    VecFree(a)
    VecFree(b)
    return 0
}
ZI
if "$ziran" check --root "$work" --module-path "$repo/std" \
    "$work/owned.zi" 2> "$work/owned.err"; then
    echo 'assignment over an owned Vec was accepted' >&2
    exit 1
fi
rg -q 'leaks it' "$work/owned.err"

cat > "$work/nested.zi" <<'ZI'
#import "vec"
Holder :: struct {
    items: Vec(s32)
}
Consume :: (values: Vec(s32)) -> s32 {
    VecFree(values)
    return 0
}
#program_export
Bad :: () -> s32 {
    holder: Holder
    VecPush(holder.items, 1)
    Consume(holder.items)
    return holder.items[0]
}
ZI
if "$ziran" check --root "$work" --module-path "$repo/std" \
    "$work/nested.zi" 2> "$work/nested.err"; then
    echo 'reusing moved nested Vec storage was accepted' >&2
    exit 1
fi
rg -q 'owned record field is used after moving' "$work/nested.err"

cat > "$work/badborrow.zi" <<'ZI'
#import "vec"
Consume :: (values: Vec(s32)) -> s32 {
    VecFree(values)
    return 0
}
#program_export
Bad :: () -> s32 {
    values: Vec(s32)
    VecPush(values, 1)
    view: []s32 = VecSlice(values, 0, 1)
    return Consume(values)
}
ZI
if "$ziran" check --root "$work" --module-path "$repo/std" \
    "$work/badborrow.zi" 2> "$work/badborrow.err"; then
    echo 'moving a borrowed Vec was accepted' >&2
    exit 1
fi
rg -q 'live borrowed view' "$work/badborrow.err"

cat > "$work/badpush.zi" <<'ZI'
#import "vec"
#program_export
Bad :: () -> s32 {
    values: Vec(s32)
    VecPush(values, 1)
    view: []s32 = VecSlice(values, 0, 1)
    VecPush(values, 2)
    VecFree(values)
    return 0
}
ZI
if "$ziran" check --root "$work" --module-path "$repo/std" \
    "$work/badpush.zi" 2> "$work/badpush.err"; then
    echo 'mutating a borrowed Vec was accepted' >&2
    exit 1
fi
rg -q 'live borrowed view' "$work/badpush.err"

cat > "$work/badclone.zi" <<'ZI'
#import "vec"
#program_export
Bad :: () -> s32 {
    a: Vec(s32)
    VecPush(a, 1)
    b: Vec(s32)
    VecPush(b, 2)
    VecClone(a, b)
    VecFree(a)
    VecFree(b)
    return 0
}
ZI
if "$ziran" check --root "$work" --module-path "$repo/std" \
    "$work/badclone.zi" 2> "$work/badclone.err"; then
    echo 'cloning into an owned Vec was accepted' >&2
    exit 1
fi
rg -q 'fresh or moved-from' "$work/badclone.err"

cat > "$work/count.zi" <<'ZI'
#import "vec"
items: Vec(s32);
#program_export
Bad :: () { items.count = 5 }
ZI
if "$ziran" check --root "$work" --module-path "$repo/std" \
    "$work/count.zi" 2> "$work/count.err"; then
    echo 'Vec count mutation was accepted' >&2
    exit 1
fi
rg -q 'read-only' "$work/count.err"

cat > "$work/nooption.zi" <<'ZI'
#import "vec"
#program_export
Bad :: () -> s32 {
    values: Vec(s32)
    VecPush(values, 1)
    popped: Option(s32) = VecPop(values)
    return 0
}
ZI
if "$ziran" check --root "$work" --module-path "$repo/std" \
    "$work/nooption.zi" 2> "$work/nooption.err"; then
    echo 'VecPop without option was accepted' >&2
    exit 1
fi
rg -q 'import option' "$work/nooption.err"

cat > "$work/badindex.zi" <<'ZI'
#import "vec"
#import "option"
#program_export
Bad :: () -> s32 {
    values: Vec(s32)
    found: Option(s32) = VecGet(values, "zero")
    return 0
}
ZI
if "$ziran" check --root "$work" --module-path "$repo/std" \
    "$work/badindex.zi" 2> "$work/badindex.err"; then
    echo 'VecGet with a string index was accepted' >&2
    exit 1
fi
rg -q 'integer index' "$work/badindex.err"

cat > "$work/badbuilder.zi" <<'ZI'
#import "vec"
#program_export
Bad :: () -> s32 {
    values: Vec(s32)
    VecPush(values, 1)
    text: string = BuilderFinish(values)
    return 0
}
ZI
if "$ziran" check --root "$work" --module-path "$repo/std" \
    "$work/badbuilder.zi" 2> "$work/badbuilder.err"; then
    echo 'BuilderFinish on a non-u8 Vec was accepted' >&2
    exit 1
fi
rg -q -F 'Vec(u8)' "$work/badbuilder.err"

# Reading a Vec and moving it in one statement would give different values
# on native targets, which empty it before the statement, and the portable
# runner; the checker rejects the read.
cat > "$work/moved_read.zi" <<'ZI'
#import "vec"
Take :: (values: Vec(s32)) -> s64 { return values.count }
Check :: () -> s64 {
    values: Vec(s32)
    VecPush(values, 1)
    return values.count + Take(values)
}
ZI
if "$ziran" check --root "$work" "$work/moved_read.zi" 2> "$work/moved_read.err"; then
    echo 'a Vec read and moved in one statement was accepted' >&2
    exit 1
fi
grep -q 'cannot also be read in it' "$work/moved_read.err"
