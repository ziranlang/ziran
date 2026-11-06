#!/bin/sh
# A Vec reached through record fields works like a local Vec: `for h.items`,
# `for * h.parts`, `for it.tags`, `h.items[1:]` iterate and slice it in place,
# and a record field holding Vecs moves out, as in `last := popped.value`.
# While a view is live, pushing to, freeing or moving that Vec, or a record
# around it, is rejected; element writes and other fields stay free. Source
# and saved IR agree on C (allocation-counted and under AddressSanitizer), Go
# and portable `.zx`.
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/fields.zi" <<'ZI'
#import "vec"
#import "option"
Item :: struct { value: s64; tags: Vec(s64); }
Holder :: struct { items: Vec(s64); parts: Vec(Item); name: string; }
Node :: struct { value: Holder; label: string; }

Sum :: (values: []s64) -> s64 { total: s64 = 0; for values { total += it }; return total }
Rename :: (h: *Holder) { h.name = "renamed" }
Fill :: (h: *Holder, n: s64) {
    i: s64 = 0
    while i < n {
        VecPush(h.items, i)
        part: Item
        part.value = i
        VecPush(part.tags, i)
        VecPush(h.parts, part)
        i += 1
    }
}

#program_export
Loops :: (rounds: s64) -> s64 {
    total: s64 = 0
    round: s64 = 0
    while round < rounds {
        h: Holder
        Fill(*h, 4)
        for h.items { total += it }
        for < h.items { total += it * 10 }
        for x, i: h.items { total += x * i }
        for * h.parts { it.value += 1; VecPush(it.tags, 5) }
        for * h.parts { for it.tags { total += it } }
        for * h.parts { total += it.value }
        // Element writes, another field's Vec and other fields stay free.
        for h.items { h.items[0] = 9; VecPush(h.parts[0].tags, it); Rename(*h) }
        total += Sum(h.items[1:]) + Sum(h.items[:2]) + h.items[0]
        if h.name != "renamed" || h.parts[0].tags.count != 6 { return -1 }
        round += 1
    }
    return total
}

#program_export
Moves :: (rounds: s64) -> s64 {
    total: s64 = 0
    round: s64 = 0
    while round < rounds {
        n: Node
        Fill(*n.value, 3)
        n.label = "node"
        last := n.value
        total += last.items.count + n.label.count
        back: Node = .{value = last, label = "back"}
        stack: Vec(Node)
        VecPush(stack, back)
        popped := VecPop(stack)
        if popped.has_value {
            kept := popped.value
            inner := kept.value
            total += inner.parts.count + Sum(inner.items[:])
        }
        round += 1
    }
    return total
}

#program_export
Check :: () -> s32 {
    if Loops(1) != 141 { return 1 }
    if Moves(1) != 13 { return 2 }
    return 42
}
ZI

# Each body changes the viewed Vec's storage, or the record holding it.
reject() {
    name=$1 body=$2 message=$3
    mkdir "$work/$name"
    cat > "$work/$name/bad.zi" <<ZI
#import "vec"
Holder :: struct { items: Vec(s64); name: string; }
Node :: struct { value: Holder; }
Grow :: (h: *Holder) { VecPush(h.items, 1) }
F :: () -> s64 {
    n: Node
    h: Holder
    VecPush(h.items, 1)
    VecPush(n.value.items, 1)
    total: s64 = 0
    $body
    return total + h.items.count + n.value.items.count
}
ZI
    if "$ziran" check --root "$work/$name" --module-path "$repo/std" \
        "$work/$name/bad.zi" 2> "$work/$name/err"
    then
        echo "$name was accepted" >&2
        exit 1
    fi
    grep -Fq "$message" "$work/$name/err" || { cat "$work/$name/err" >&2; exit 1; }
}
reject push 'for h.items { VecPush(h.items, it) }' \
    'cannot mutate a Vec with a live borrowed view'
reject call 'for h.items { Grow(*h) }' \
    'cannot mutate a Vec with a live borrowed view'
reject nested 'for n.value.items { Grow(*n.value) }' \
    'cannot mutate a Vec with a live borrowed view'
reject free 'for h.items { VecFree(h.items) }' \
    'cannot move a Vec with a live borrowed view'
reject move 'for n.value.items { last := n.value; total += last.items.count }' \
    'cannot move a Vec with a live borrowed view'
reject moved 'last := n.value; total += n.value.items.count + last.items.count' \
    'owned record field is used after moving'

"$ziran" check --root "$work" --module-path "$repo/std" "$work/fields.zi"
"$ziran" ir --root "$work" --module-path "$repo/std" \
    -o "$work/ir" "$work/fields.zi"
for kind in source saved; do
    if test "$kind" = source; then
        root=$work
        module=$work/fields.zi
    else
        root=$work/ir
        module=$work/ir/fields.zir
    fi
    "$ziran" bundle --entry fields:Check --root "$root" \
        --module-path "$repo/std" -o "$work/$kind.zx" "$module"
    test "$("$ziran" run "$work/$kind.zx")" = 42

    c_output=$work/$kind-c
    "$ziran" build --target=c --entry fields:Check --root "$root" \
        --module-path "$repo/std" -o "$c_output" "$module"
    cat > "$work/count.c" <<'C'
#include "fields.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
void *__real_realloc(void *, size_t);
void *__real_malloc(size_t);
void *__real_calloc(size_t, size_t);
void __real_free(void *);
static long outstanding;
void *__wrap_malloc(size_t size) {
    void *result = __real_malloc(size);
    if (result != NULL) outstanding++;
    return result;
}
void *__wrap_calloc(size_t count, size_t size) {
    void *result = __real_calloc(count, size);
    if (result != NULL) outstanding++;
    return result;
}
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
    long before = outstanding;
    if (Loops(1000) != 141000 || Moves(1000) != 13000 || Check() != 42)
        return 1;
    printf("1000 rounds: %ld allocations left\n", outstanding - before);
    return outstanding == before ? 0 : 2;
}
C
    "${CC:-cc}" -std=c11 -I"$repo/include" -I"$c_output" \
        "$c_output"/*.c "$work/count.c" -Wl,--wrap=realloc -Wl,--wrap=malloc \
        -Wl,--wrap=calloc -Wl,--wrap=free -o "$work/count-$kind"
    "$work/count-$kind"
    cat > "$work/plain.c" <<'C'
#include "fields.h"
int main(void) { return Loops(50) == 7050 && Moves(50) == 650 && Check() == 42 ? 0 : 1; }
C
    "${CC:-cc}" -std=c11 -g -fsanitize=address,undefined \
        -fno-sanitize-recover=undefined -fno-omit-frame-pointer \
        -I"$repo/include" -I"$c_output" "$c_output"/*.c "$work/plain.c" \
        -o "$work/asan-$kind"
    "$work/asan-$kind"

    go_output=$work/$kind-go
    "$ziran" build --target=go --pkg main --entry fields:Check \
        --root "$root" --module-path "$repo/std" \
        -o "$go_output" "$module"
    cat > "$go_output/main.go" <<'GO'
package main
func main() { if Fields_Check() != 42 { panic("vec field loops") } }
GO
    GO111MODULE=off go run "$go_output"/*.go
done

cmp "$work/source.zx" "$work/saved.zx"
cmp "$work/source-c/fields.c" "$work/saved-c/fields.c"
cmp "$work/source-go/fields.go" "$work/saved-go/fields.go"
