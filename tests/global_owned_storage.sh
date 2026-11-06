#!/bin/sh
# A global holding a Vec or Text, alone or in a record, takes owned values:
# assigning it, or a field path of it, frees the old value first, and a
# declaration, assignment or return that moves it takes the storage and
# leaves the global empty. Passing a global to an owning call, pushing it,
# or replacing it while a view of it is live is rejected. Source and saved
# IR agree; C runs allocation-counted and under AddressSanitizer, and native
# and VM bundles give the same results.
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
zi2zx=$(dirname -- "$ziran")/zi2zx
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
cat > "$work/store.zi" <<'ZI'
#import "std/vec"
Bag :: struct { items: Text; valid: bool; }
Shelf :: struct { bag: Bag; counts: Vec(s32); }
Make :: (a: string) -> Bag { return Bag.{.items = TextCopy(a), .valid = true} }
Width :: (s: string) -> s64 { return s.count }

bag: Bag;
shelf: Shelf;
Take :: () -> Bag { return bag }

#program_export
Fill :: (rounds: s64) -> s64 {
    total: s64 = 0
    round: s64 = 0
    while round < rounds {
        bag = Make("abc")
        bag.items = TextCopy("de")
        shelf.bag = bag
        total += Width(shelf.bag.items) + Width(bag.items)
        counts: Vec(s32)
        VecPush(counts, 1)
        shelf.counts = counts
        moved := shelf
        total += moved.counts.count + Width(shelf.bag.items)
        shelf = moved
        shelf = shelf
        round += 1
    }
    return total
}

#program_export
Clear :: () -> s64 {
    bag = Make("fgh")
    last := Take()
    kept := Shelf.{.bag = shelf.bag, .counts = shelf.counts}
    return Width(last.items) * 10 + Width(bag.items) +
        Width(kept.bag.items) + kept.counts.count + Width(shelf.bag.items)
}

#program_export
Run :: () -> s64 { return Fill(100) + Clear() }
ZI
"$ziran" check --root "$work" --module-path "$repo" "$work/store.zi"
"$ziran" ir --root "$work" --module-path "$repo" -o "$work/ir" "$work/store.zi"
for kind in source saved; do
    root=$work; module=$work/store.zi
    if test "$kind" = saved; then root=$work/ir; module=$work/ir/store.zir; fi
    output=$work/$kind-c
    "$ziran" build --target=c --root "$root" --module-path "$repo" -o "$output" "$module"
    cat > "$work/count.c" <<'C'
#include "store.h"
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
    /* 2 + 0 + 1 + 0 for each round; the shelf keeps its Text and Vec. */
    if (Fill(1000) != 3000) return 1;
    long filled = outstanding;
    /* 30 + 0 + 2 + 1 + 0, and both globals end empty. */
    if (Clear() != 33) return 1;
    printf("1000 rounds: %ld allocations held by the globals, %ld after moving them out\n",
           filled, outstanding);
    return filled == 2 && outstanding == 0 ? 0 : 2;
}
C
    "${CC:-cc}" -std=c11 -I"$repo/include" -I"$output" "$output"/*.c "$work/count.c" \
        -Wl,--wrap=realloc -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=free \
        -o "$work/count-$kind"
    "$work/count-$kind"
    printf '#include "store.h"\nint main(void) { return Fill(50) == 150 && Clear() == 33 && Run() == 333 ? 0 : 1; }\n' > "$work/plain.c"
    "${CC:-cc}" -std=c11 -g -fsanitize=address,undefined -fno-sanitize-recover=undefined \
        -fno-omit-frame-pointer -I"$repo/include" -I"$output" "$output"/*.c \
        "$work/plain.c" -o "$work/asan-$kind"
    ASAN_OPTIONS=detect_leaks=0 "$work/asan-$kind"
    "$ziran" bundle --entry store:Run --root "$root" --module-path "$repo" -o "$work/$kind.zx" "$module"
    test "$("$zi2zx" run "$work/$kind.zx")" = 333
    test "$("$zi2zx" run --vm "$work/$kind.zx")" = 333
done
cmp "$work/source-c/store.c" "$work/saved-c/store.c"

# A global still cannot move as a side effect of a call or VecPush, and it
# cannot be replaced or moved while a view of its storage is live.
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
reject argument 'global Vec storage would move into the call and empty the global' <<'ZI'
#import "std/vec"
Bag :: struct { items: Text; }
Keep :: (b: Bag) -> s32 { return 0 }
bag: Bag;
Main :: () -> s32 { return Keep(bag) }
ZI
reject pushed 'global Vec storage would move into the Vec and empty the global' <<'ZI'
#import "std/vec"
Bag :: struct { items: Text; }
bag: Bag;
Main :: () -> s64 {
    bags: Vec(Bag)
    VecPush(bags, bag)
    return bags.count
}
ZI
reject replaced 'mutating text backing storage while its view is live' <<'ZI'
#import "std/vec"
Bag :: struct { items: Text; }
bag: Bag;
Main :: () -> s64 {
    view: string = bag.items
    bag = Bag.{.items = TextCopy("de")}
    return view.count
}
ZI
reject moved 'moving owned storage while its text view is live' <<'ZI'
#import "std/vec"
Bag :: struct { items: Text; }
bag: Bag;
Main :: () -> s64 {
    view: string = bag.items
    moved := bag
    return view.count
}
ZI
echo 'global Vec and Text storage: assignment frees the old value, moves leave the global empty, 2 allocations held and 0 after moving out; calls, pushes and live views are rejected; source and saved IR, native and VM agree; ASan clean'
