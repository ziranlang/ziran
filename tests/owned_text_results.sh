#!/bin/sh
# A procedure that returns owned Text works for both kinds of caller. A
# caller that names the Text keeps it and frees it with its owner; a caller
# written for string results, as in `name := Join(...)`, still gets a string
# that keeps its bytes. BuilderFinish gives a Text where one is expected. A
# module that never imports std/vec can use either. Source and
# saved IR agree on C (allocation-counted and under AddressSanitizer), Go and
# portable `.zx`.
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/joined.zi" <<'ZI'
#import "vec"
// A producer written the old way, returning Text.
Join :: (a: string, b: string) -> Text {
    out: Vec(u8)
    if !BuilderAppend(out, a) || !BuilderAppend(out, b) { return .{} }
    return BuilderFinish(out)
}
ZI

# Uses Join as a string and never imports std/vec.
cat > "$work/strings.zi" <<'ZI'
p :: #import "joined"
Holder :: struct { s: string; }
Take :: (s: string) -> s64 { return s.count }
Old :: () -> string { return p.Join("o", "ld") }
Keep :: () -> string { x := p.Join("k", "eep"); return x }
Strings :: () -> bool {
    a := p.Join("ab", "c")
    if a != "abc" || a.count != 3 || Take(p.Join("x", "yz")) != 3 { return false }
    h := Holder.{s = p.Join("h", "i")}
    s: string = p.Join("s", "")
    s = p.Join("t", "u")
    p.Join("dropped", "")
    return h.s == "hi" && s == "tu" && Old() == "old" && Keep() == "keep" &&
        p.Join("ab", "cd").count == 4 && p.Join("z", "")[0] == #char "z"
}
ZI

cat > "$work/results.zi" <<'ZI'
#import "vec"
p :: #import "joined"
s :: #import "strings"

Take :: (value: string) -> s64 { return value.count }
Measure :: (text: Text) -> s64 { return TextView(text).count }
Finish :: () -> Text { b: Vec(u8); BuilderAppend(b, "fin"); return BuilderFinish(b) }

// Results used as strings keep their bytes for the rest of the program.
#program_export
Detached :: (rounds: s64) -> s64 {
    total: s64 = 0
    round: s64 = 0
    while round < rounds {
        written := p.Join("{\"a\": ", "[1]}")
        total += written.count + Take(p.Join("0.", "5")) + p.Join("ab", "").count
        round += 1
    }
    return total
}

// The same work keeping each result as a Text frees every one.
#program_export
Owned :: (rounds: s64) -> s64 {
    total: s64 = 0
    round: s64 = 0
    while round < rounds {
        written: Text = p.Join("{\"a\": ", "[1]}")
        number: Text = p.Join("0.", "5")
        total += Take(written) + TextView(number).count + Measure(p.Join("ab", ""))
        round += 1
    }
    return total
}

#program_export
Check :: () -> s32 {
    if !s.Strings() { return 1 }
    if Detached(3) != Owned(3) || Owned(1) != 15 { return 2 }
    t: Text = p.Join("ab", "c")
    if TextView(t) != "abc" || Take(t) != 3 || Measure(p.Join("x", "y")) != 2 { return 3 }
    names: Vec(Text)
    VecPush(names, p.Join("n", "1"))
    VecPush(names, Finish())
    if TextView(names[0]) != "n1" || TextView(names[1]) != "fin" { return 4 }
    b: Vec(u8)
    BuilderAppend(b, "built")
    u: Text = BuilderFinish(b)
    if Take(u) != 5 { return 5 }
    p.Join("gone", "")
    kept := TextDetach(TextCopy("kept"))
    bytes: Vec(u8)
    empty: Text = TextFromBuilder(bytes)
    if kept != "kept" || TextView(empty) != "" { return 6 }
    return 42
}
ZI

cat > "$work/fresh.zi" <<'ZI'
p :: #import "joined"
F :: () -> s64 { return TextView(p.Join("a", "b")).count }
ZI
if "$ziran" check --root "$work" --module-path "$repo/std" "$work/fresh.zi" \
    2> "$work/fresh.err"
then
    echo "TextView of a fresh Text result was accepted" >&2
    exit 1
fi
grep -Fq 'TextView borrows a Text in place; bind the Text to a local first' \
    "$work/fresh.err" || { cat "$work/fresh.err" >&2; exit 1; }

"$ziran" check --root "$work" --module-path "$repo/std" "$work/results.zi"
"$ziran" ir --root "$work" --module-path "$repo/std" \
    -o "$work/ir" "$work/results.zi"
for kind in source saved; do
    if test "$kind" = source; then
        root=$work
        module=$work/results.zi
    else
        root=$work/ir
        module=$work/ir/results.zir
    fi
    "$ziran" bundle --entry results:Check --root "$root" \
        --module-path "$repo/std" -o "$work/$kind.zx" "$module"
    test "$("$ziran" run "$work/$kind.zx")" = 42

    c_output=$work/$kind-c
    "$ziran" build --target=c --entry results:Check --root "$root" \
        --module-path "$repo/std" -o "$c_output" "$module"
    cat > "$work/count.c" <<'C'
#include "results.h"
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
int main(int argc, char **argv) {
    long rounds = argc > 1 ? atol(argv[1]) : 1000;
    long before = outstanding;
    if (Detached(rounds) <= 0) return 1;
    long detached = outstanding - before;
    before = outstanding;
    if (Owned(rounds) <= 0) return 1;
    long owned = outstanding - before;
    printf("%ld rounds: %ld allocations kept as strings, %ld kept as Text\n",
           rounds, detached, owned);
    if (Check() != 42) return 1;
    return owned == 0 && detached >= 3 * rounds ? 0 : 2;
}
C
    "${CC:-cc}" -std=c11 -I"$repo/include" -I"$c_output" \
        "$c_output"/*.c "$work/count.c" -Wl,--wrap=realloc -Wl,--wrap=malloc \
        -Wl,--wrap=calloc -Wl,--wrap=free -o "$work/count-$kind"
    "$work/count-$kind" 1000
    cat > "$work/plain.c" <<'C'
#include "results.h"
int main(void) { return Owned(100) > 0 && Check() == 42 ? 0 : 1; }
C
    "${CC:-cc}" -std=c11 -g -fsanitize=address,undefined \
        -fno-sanitize-recover=undefined -fno-omit-frame-pointer \
        -I"$repo/include" -I"$c_output" "$c_output"/*.c "$work/plain.c" \
        -o "$work/asan-$kind"
    # Check's string results keep their bytes; only owned paths must be clean.
    ASAN_OPTIONS=detect_leaks=0 "$work/asan-$kind"

    go_output=$work/$kind-go
    "$ziran" build --target=go --pkg main --entry results:Check \
        --root "$root" --module-path "$repo/std" \
        -o "$go_output" "$module"
    cat > "$go_output/main.go" <<'GO'
package main
func main() { if Results_Check() != 42 { panic("owned text results") } }
GO
    GO111MODULE=off go run "$go_output"/*.go
done

cmp "$work/source.zx" "$work/saved.zx"
cmp "$work/source-c/results.c" "$work/saved-c/results.c"
cmp "$work/source-go/results.go" "$work/saved-go/results.go"
