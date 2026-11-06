#!/bin/sh
# Standard text producers return owned Text. A caller written for a string,
# as in `s := TextReplace(...)`, still gets a string; one that is only read,
# or kept as Text, is freed. Source and saved IR agree on C (allocation-counted
# and under AddressSanitizer), Go and portable `.zx`.
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/producers.zi" <<'ZI'
#import "vec"
#import "text_tools"
#import "float_text"
#import "json_document"
#import "unicode"

Take :: (value: string) -> s64 { return value.count }

// Written for string results: only the Float64Repr read after the Take call
// keeps its bytes; `replaced`, TextLower and JsonQuote are only read and are
// freed with their block.
#program_export
Detached :: (rounds: s64) -> s64 {
    total: s64 = 0
    round: s64 = 0
    while round < rounds {
        replaced := TextReplace("a-b-c", "-", "+")
        total += replaced.count + Take(TextLower("ABC")) + Float64Repr(0.5).count
        total += JsonQuote("q").count
        round += 1
    }
    return total
}

// The same work keeping each result as Text frees every one.
#program_export
Owned :: (rounds: s64) -> s64 {
    total: s64 = 0
    round: s64 = 0
    while round < rounds {
        replaced: Text = TextReplace("a-b-c", "-", "+")
        lower: Text = TextLower("ABC")
        number: Text = Float64Repr(0.5)
        quoted: Text = JsonQuote("q")
        total += Take(replaced) + Take(lower) + TextView(number).count + Take(quoted)
        round += 1
    }
    return total
}

#program_export
Check :: () -> s32 {
    if Detached(3) != Owned(3) { return 1 }
    r: Text = TextReplace("a-b", "-", "+")
    f: Text = Float64Repr(0.5)
    q: Text = JsonQuote("q")
    if TextView(r) != "a+b" || TextView(f) != "0.5" || TextView(q) != "\"q\"" { return 2 }
    if Lower("ÄB") != "äb" { return 3 }
    return 42
}
ZI

"$ziran" check --root "$work" --module-path "$repo/std" "$work/producers.zi"
"$ziran" ir --root "$work" --module-path "$repo/std" -o "$work/ir" "$work/producers.zi"
for kind in source saved; do
    if test "$kind" = source; then root=$work; module=$work/producers.zi
    else root=$work/ir; module=$work/ir/producers.zir; fi
    "$ziran" bundle --entry producers:Check --root "$root" \
        --module-path "$repo/std" -o "$work/$kind.zx" "$module"
    test "$("$ziran" run "$work/$kind.zx")" = 42
    c_output=$work/$kind-c
    "$ziran" build --target=c --entry producers:Check --root "$root" \
        --module-path "$repo/std" -o "$c_output" "$module"
    cat > "$work/count.c" <<'C'
#include "producers.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
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
    long before = outstanding;
    if (Detached(1000) <= 0) return 1;
    long detached = outstanding - before;
    before = outstanding;
    if (Owned(1000) <= 0) return 1;
    long owned = outstanding - before;
    printf("1000 rounds: %ld allocations kept as strings, %ld kept as Text\n", detached, owned);
    if (Check() != 42) return 1;
    return owned == 0 && detached == 1000 ? 0 : 2;
}
C
    "${CC:-cc}" -std=c11 -I"$repo/include" -I"$c_output" "$c_output"/*.c "$work/count.c" \
        -Wl,--wrap=realloc -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=free \
        -lm -o "$work/count-$kind"
    "$work/count-$kind"
    cat > "$work/plain.c" <<'C'
#include "producers.h"
int main(void) { return Owned(100) > 0 && Check() == 42 ? 0 : 1; }
C
    "${CC:-cc}" -std=c11 -g -fsanitize=address,undefined -fno-sanitize-recover=undefined \
        -fno-omit-frame-pointer -I"$repo/include" -I"$c_output" "$c_output"/*.c \
        "$work/plain.c" -lm -o "$work/asan-$kind"
    ASAN_OPTIONS=detect_leaks=0 "$work/asan-$kind"
    go_output=$work/$kind-go
    "$ziran" build --target=go --pkg main --entry producers:Check --root "$root" \
        --module-path "$repo/std" -o "$go_output" "$module"
    printf 'package main\nfunc main() { if Producers_Check() != 42 { panic("std text") } }\n' \
        > "$go_output/main.go"
    GO111MODULE=off go run "$go_output"/*.go
done
cmp "$work/source.zx" "$work/saved.zx"
cmp "$work/source-c/producers.c" "$work/saved-c/producers.c"
cmp "$work/source-go/producers.go" "$work/saved-go/producers.go"
