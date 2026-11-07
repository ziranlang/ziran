#!/bin/sh
# A field path of an owned local record, such as `shelf.label` or
# `shelf.bag.items`, takes owned values like a global does: assigning it
# completes the new value, frees the old one and stores it, and a field moved
# out can be assigned again. By-value parameters own their fields too. A whole
# local still has to be moved before it is replaced. Replacing a field while a
# view of it is live, assigning into a moved binding or below a moved field,
# and storing through a pointer stay rejected. Source and saved IR agree; C
# runs allocation-counted and under AddressSanitizer, and native and VM
# bundles give the same results.
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
zi2zx=$(dirname -- "$ziran")/zi2zx
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
cat > "$work/fields.zi" <<'ZI'
#import "std/vec"
Bag :: struct { items: Text; counts: Vec(s32); }
Shelf :: struct { bag: Bag; label: Text; }
Width :: (s: string) -> s64 { return s.count }
Make :: (a: string) -> Bag { return Bag.{.items = TextCopy(a)} }
Relabel :: (shelf: Shelf, label: string) -> Shelf {
    shelf.label = TextCopy(label)
    return shelf
}

#program_export
Fill :: (rounds: s64) -> s64 {
    total: s64 = 0
    round: s64 = 0
    while round < rounds {
        shelf: Shelf
        shelf.label = TextCopy("abc")
        shelf.label = TextCopy("de")
        shelf.bag.items = TextCopy("fgh")
        counts: Vec(s32)
        VecPush(counts, 1)
        shelf.bag.counts = counts
        taken := shelf.bag.items
        shelf.bag.items = TextCopy("ij")
        shelf.bag = Make("klmn")
        total += Width(shelf.label) + Width(taken) + Width(shelf.bag.items) +
            shelf.bag.counts.count
        shelf.bag.items = shelf.label
        shelf.label = TextCopy("z")
        total += Width(shelf.bag.items) + Width(shelf.label)
        moved := Relabel(shelf, "xyz")
        total += Width(moved.label) + Width(moved.bag.items)
        round += 1
    }
    return total
}

#program_export
Run :: () -> s64 { return Fill(10) }
ZI
"$ziran" check --root "$work" --module-path "$repo" "$work/fields.zi"
"$ziran" ir --root "$work" --module-path "$repo" -o "$work/ir" "$work/fields.zi"
for kind in source saved; do
    root=$work; module=$work/fields.zi
    if test "$kind" = saved; then root=$work/ir; module=$work/ir/fields.zir; fi
    output=$work/$kind-c
    "$ziran" build --target=c --root "$root" --module-path "$repo" -o "$output" "$module"
    cat > "$work/count.c" <<'C'
#include "fields.h"
#include <stddef.h>
#include <stdio.h>
void *__real_realloc(void *, size_t);
void *__real_malloc(size_t);
void *__real_calloc(size_t, size_t);
void __real_free(void *);
static long outstanding;
void *__wrap_malloc(size_t size) { void *r = __real_malloc(size); if (r) outstanding++; return r; }
void *__wrap_calloc(size_t n, size_t size) { void *r = __real_calloc(n, size); if (r) outstanding++; return r; }
void *__wrap_realloc(void *p, size_t size) { void *r = __real_realloc(p, size); if (!p && r) outstanding++; return r; }
void __wrap_free(void *p) { if (p) outstanding--; __real_free(p); }
int main(void) {
    /* (2 + 3 + 4 + 0) + (2 + 1) + (3 + 2) for each round */
    if (Fill(1000) != 17000) return 1;
    printf("1000 rounds of local field stores: %ld allocations left\n", outstanding);
    return outstanding == 0 ? 0 : 2;
}
C
    "${CC:-cc}" -std=c11 -I"$repo/include" -I"$output" "$output"/*.c "$work/count.c" \
        -Wl,--wrap=realloc -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=free \
        -o "$work/count-$kind"
    "$work/count-$kind"
    printf '#include "fields.h"\nint main(void) { return Fill(50) == 850 && Run() == 170 ? 0 : 1; }\n' > "$work/plain.c"
    "${CC:-cc}" -std=c11 -g -fsanitize=address,undefined -fno-sanitize-recover=undefined \
        -fno-omit-frame-pointer -I"$repo/include" -I"$output" "$output"/*.c \
        "$work/plain.c" -o "$work/asan-$kind"
    ASAN_OPTIONS=detect_leaks=0 "$work/asan-$kind"
    "$ziran" bundle --entry fields:Run --root "$root" --module-path "$repo" -o "$work/$kind.zx" "$module"
    test "$("$zi2zx" run "$work/$kind.zx")" = 170
    test "$("$zi2zx" run --vm "$work/$kind.zx")" = 170
done
cmp "$work/source-c/fields.c" "$work/saved-c/fields.c"

mkdir "$work/bad"
reject() {
    name=$1 expected=$2
    cat > "$work/bad/$name.zi"
    if "$ziran" check --root "$work/bad" --module-path "$repo" "$work/bad/$name.zi" 2> "$work/bad/$name.err"; then
        echo "$name was accepted" >&2
        exit 1
    fi
    grep -q "$expected" "$work/bad/$name.err"
}
reject whole 'assignment over an owned Vec leaks it; free or move it first' <<'ZI'
#import "std/vec"
Bag :: struct { items: Text; }
Main :: () -> s64 {
    bag: Bag
    bag = Bag.{.items = TextCopy("de")}
    return 0
}
ZI
reject view 'mutating text backing storage while its view is live' <<'ZI'
#import "std/vec"
Bag :: struct { items: Text; }
Main :: () -> s64 {
    bag: Bag
    bag.items = TextCopy("abc")
    view: string = bag.items
    bag.items = TextCopy("de")
    return view.count
}
ZI
reject moved 'Vec binding is used after moving' <<'ZI'
#import "std/vec"
Bag :: struct { items: Text; }
Width :: (s: string) -> s64 { return s.count }
Main :: () -> s64 {
    bag: Bag
    moved := bag
    bag.items = TextCopy("de")
    return Width(moved.items)
}
ZI
reject below 'owned record field is used after moving' <<'ZI'
#import "std/vec"
Bag :: struct { items: Text; }
Shelf :: struct { bag: Bag; }
Width :: (s: string) -> s64 { return s.count }
Main :: () -> s64 {
    shelf: Shelf
    taken := shelf.bag
    shelf.bag.items = TextCopy("de")
    return Width(taken.items)
}
ZI
reject pointer 'Vec assignment requires a binding, a field path of one or a global destination' <<'ZI'
#import "std/vec"
Bag :: struct { items: Text; }
Main :: () -> s64 {
    bag: Bag
    p := *bag
    p.items = TextCopy("de")
    return 0
}
ZI
echo 'local record field stores: the old value is freed, moved fields are assigned again, 0 allocations left; whole locals, live views, moved bindings and pointers are rejected; source and saved IR, native and VM agree; ASan clean'
