#!/bin/sh
# A fresh Text is freed with its block when its producer takes a pointer
# that it only reads through, as in `AwardsText(*awards)` under an `&&`
# condition. That holds even when the producer builds with BuilderPrint into
# a local Vec. It is also freed when it is the left operand of `&&` or `||`,
# or a local whose name another block also declares, even when an inner
# block reuses the name for a Vec and returns early. A producer that writes
# through its pointer keeps its place after the arguments before it, and a
# same-named local that is kept leaves the other binding freed. Source and
# saved IR agree; C runs allocation-counted and under AddressSanitizer, and
# native and VM bundles give the same results.
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
zi2zx=$(dirname -- "$ziran")/zi2zx
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
cat > "$work/producers.zi" <<'ZI'
#import "std/vec"
Awards :: struct { earned: [3]bool; valid: bool; count: s64; }
awards: Awards;
setting: string;
saved: s64;
flag: bool;
kept: string;

AwardsText :: (state: *Awards) -> Text {
    bytes: Vec(u8)
    BuilderPrint(*bytes, "%", "HEAD\n")
    for index: cast(s32)0..2 {
        if state.earned[index] { BuilderPrint(*bytes, "%\n", "award") }
    }
    return BuilderFinish(bytes)
}
// Writes through its pointer, so it runs after the read before it.
Bump :: (state: *Awards) -> Text { state.count += 1; return TextCopy("bump") }
Fresh :: (a: string) -> Text { return TextCopy(a) }
Read :: (s: string) -> s64 { return s.count }
Pair :: (n: s64, s: string) -> s64 { return n * 10 + s.count }
Replace :: (path: string, text: string) -> bool { saved += path.count + text.count; return true }
Keep :: (s: string) { kept = s }

#program_export
Pointers :: (rounds: s64) -> s64 {
    setting = "settings"
    awards.valid = true
    awards.earned[1] = true
    saved = 0
    round: s64 = 0
    while round < rounds {
        if setting != "" && awards.valid { unused Replace(setting, AwardsText(*awards)) }
        round += 1
    }
    return saved
}

#program_export
Ordered :: (rounds: s64) -> s64 {
    awards.count = 0
    total: s64 = 0
    round: s64 = 0
    while round < rounds {
        total += Pair(awards.count, Bump(*awards))
        round += 1
    }
    return total
}

#program_export
Conditions :: (rounds: s64) -> s64 {
    flag = true
    total: s64 = 0
    round: s64 = 0
    while round < rounds {
        if Read(Fresh("abc")) > 0 && flag { total += 1 }
        if Read(Fresh("de")) > 5 || flag { total += 1 }
        ok := Read(Fresh("f")) == 1 && flag
        if ok { total += 1 }
        round += 1
    }
    return total
}

Siblings :: (k: s64) -> s64 {
    if k == 0 {
        next := Fresh("zero")
        if Read(next) < 1 || Read(next) > 9 { return -1 }
        return Read(next)
    } else if k == 1 {
        next := Fresh("one!")
        return Read(next) + 1
    }
    next := Fresh("other")
    return Read(next)
}

#program_export
Locals :: (rounds: s64) -> s64 {
    total: s64 = 0
    round: s64 = 0
    while round < rounds {
        total += Siblings(round % 3)
        round += 1
    }
    return total
}

#program_export
Shadowed :: (rounds: s64) -> s64 {
    total: s64 = 0
    round: s64 = 0
    while round < rounds {
        if round >= 0 {
            next := Fresh("inner")
            Keep(next)
        }
        next := Fresh("outer!")
        total += Read(next)
        round += 1
    }
    return total + Read(kept)
}

// An inner block reuses the name of a live Text local for a Vec and
// returns early, so both are freed there; the Text is freed at its block's
// end otherwise.
Find :: (word: string, depth: s32) -> bool {
    if depth == 2 { return word == "Fxx" }
    for i: 0..2 {
        prefix := Fresh(word)
        if prefix != "F" && prefix != "Fx" { continue }
        if i > 0 {
            prefix: Vec(u8)
            BuilderPrint(*prefix, "%x", word)
            if Find(BuilderFinish(prefix), depth + 1) { return true }
        }
    }
    return false
}
Bag :: struct { items: Text; }
Nested :: (n: s64) -> s64 {
    bag := Bag.{.items = Fresh("abc")}
    if n > 0 {
        bag := Bag.{.items = Fresh("de")}
        if n > 1 { return Read(bag.items) }
    }
    return Read(bag.items)
}

#program_export
Shadows :: (rounds: s64) -> s64 {
    total: s64 = 0
    round: s64 = 0
    while round < rounds {
        if Find("F", 0) { total += 1 }
        total += Nested(round % 3)
        round += 1
    }
    return total
}

#program_export
Run :: () -> s64 {
    return Pointers(10) + Ordered(10) + Conditions(10) + Locals(10) + Shadowed(10) +
        Shadows(10)
}
ZI
"$ziran" check --root "$work" --module-path "$repo" "$work/producers.zi"
"$ziran" ir --root "$work" --module-path "$repo" -o "$work/ir" "$work/producers.zi"
for kind in source saved; do
    root=$work; module=$work/producers.zi
    if test "$kind" = saved; then root=$work/ir; module=$work/ir/producers.zir; fi
    output=$work/$kind-c
    "$ziran" build --target=c --root "$root" --module-path "$repo" -o "$output" "$module"
    cat > "$work/count.c" <<'C'
#include "producers.h"
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
static long left(long before) { return outstanding - before; }
int main(void) {
    long before = outstanding, pointers, ordered, conditions, locals, shadowed, shadows;
    /* "settings" and "HEAD\naward\n" for each round */
    if (Pointers(1000) != 19000) return 1;
    pointers = left(before); before = outstanding;
    /* the count before Bump: 10 * (0 + ... + 999) + 4 * 1000 */
    if (Ordered(1000) != 4999000) return 1;
    ordered = left(before); before = outstanding;
    if (Conditions(1000) != 3000) return 1;
    conditions = left(before); before = outstanding;
    /* 334 * 4 + 333 * 5 + 333 * 5 */
    if (Locals(1000) != 4666) return 1;
    locals = left(before); before = outstanding;
    if (Shadowed(1000) != 6005) return 1;
    shadowed = left(before); before = outstanding;
    /* 1000 + 334 * 3 + 333 * 3 + 333 * 2 */
    if (Shadows(1000) != 3667) return 1;
    shadows = left(before);
    /* Shadowed keeps only the inner local, which a global holds. */
    printf("1000 rounds: %ld allocations left by pointer producers, %ld by conditions, "
           "%ld by sibling locals, %ld by shadowed names; %ld kept by a writing "
           "producer, %ld by a local a global holds\n",
           pointers, conditions, locals, shadows, ordered, shadowed);
    return pointers == 0 && conditions == 0 && locals == 0 && shadows == 0 &&
        shadowed == 1000 && ordered == 1000 ? 0 : 2;
}
C
    "${CC:-cc}" -std=c11 -I"$repo/include" -I"$output" "$output"/*.c "$work/count.c" \
        -Wl,--wrap=realloc -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=free \
        -o "$work/count-$kind"
    "$work/count-$kind"
    printf '#include "producers.h"\nint main(void) { return Run() == 190 + 490 + 30 + 46 + 65 + 37 ? 0 : 1; }\n' > "$work/plain.c"
    "${CC:-cc}" -std=c11 -g -fsanitize=address,undefined -fno-sanitize-recover=undefined \
        -fno-omit-frame-pointer -I"$repo/include" -I"$output" "$output"/*.c \
        "$work/plain.c" -o "$work/asan-$kind"
    ASAN_OPTIONS=detect_leaks=0 "$work/asan-$kind"
    "$ziran" bundle --entry producers:Run --root "$root" --module-path "$repo" -o "$work/$kind.zx" "$module"
    test "$("$zi2zx" run "$work/$kind.zx")" = 858
    test "$("$zi2zx" run --vm "$work/$kind.zx")" = 858
done
cmp "$work/source-c/producers.c" "$work/saved-c/producers.c"
echo 'Text from pointer-reading producers, && and || left operands and same-named or shadowed locals: freed, 0 left; writing producers keep their order; source and saved IR, native and VM agree; ASan clean'
