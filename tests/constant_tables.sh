#!/bin/sh
# An index read of a constant array, such as TABLE :: #run s64.[...], reads
# one shared read-only table: a module-level static const in C, a package
# var in Go and a value the VM builds once. Go also declares a copy or
# passes the table by value straight from that var. Copies keep value
# semantics. Uses carry the value, so no header names a table in a macro.
# A function may read one table in many loops, beside widening casts and
# .count, without running out of room to bind it.
set -eu

ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
run_vm="$(dirname "$ziran")/zi2zx"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

big=$(awk 'BEGIN { for(i = 0; i < 4096; i++) printf "%s%d", i ? (i % 32 ? ", " : ",\n    ") : "", (i * 7) % 13 }')
cat > "$work/library.zi" <<ZI
NUMBERS :: #run s64.[40, 41, 42, 43];
BIG :: #run u8.[$big];
ZI
cat > "$work/app.zi" <<'ZI'
Lib :: #import "library";
TABLE :: #run s32.[3, 1, 4, 1, 5, 9, 2, 6];
FLAGS :: #run bool.[true, false, true];
SCALES :: #run float64.[0.5, -1.25, 2.0];
Pick :: (i: s64) -> s32 { return TABLE[i] }
Again :: (i: s64) -> s32 { return TABLE[i] + 0 }
BigSum :: (rounds: s64) -> s64 {
    total: s64 = 0
    for i: 0..rounds { total += cast(s64) Lib.BIG[i & 4095] }
    return total
}
// Each loop's s32 + s64 index widens before the next read binds TABLE.
Twice :: () -> s32 {
    sum: s32
    b: s32 = 1
    for j: 0..cast(s64)3 { sum += TABLE[b + j] }
    for j: 0..cast(s64)3 { sum += TABLE[b + j] }
    return sum
}
Grid :: (width: s32, height: s32) -> s32 {
    x_base := width - 4
    y_base := height - 4
    total: s32 = 0
    for i: 0..cast(s64)height - 1 {
        wy := TABLE[y_base + i]
        for j: 0..cast(s64)width - 1 {
            wx := TABLE[x_base + j]
            total += wx * wy
        }
    }
    return total
}
Taps :: (b: s32) -> s32 {
    s: s32 = 0
    for i: 0..cast(s64)1 {
        for j: 0..cast(s64)1 {
            for k: 0..cast(s64)1 { s += TABLE[b + i + j + k] }
        }
    }
    for i: 0..cast(s64)1 { s += TABLE[b + i] }
    return s
}
Bump :: (values: [8]s32) -> s32 { values[0] += 10; return values[0] }
Copied :: () -> [8]s32 { return TABLE }
Counted :: () -> s32 {
    sum: s32 = 0
    for i: 0..TABLE.count - 1 { sum += TABLE[i] }
    return sum + TABLE[TABLE.count - 1] + cast(s32) TABLE.count
}
#program_export
Answer :: () -> s64 {
    local: [8]s32 = TABLE
    local[0] = 100
    if TABLE[0] != 3 || local[0] != 100 || Pick(5) != 9 || Again(2) != 4 { return 1 }
    copy: [8]s32 = local
    copy[1] = 7
    if local[1] != 1 || TABLE[1] != 1 { return 2 }
    if !FLAGS[0] || FLAGS[1] || SCALES[1] != -1.25 { return 3 }
    sum: s32 = 0
    for i: 0..7 { sum += TABLE[i] }
    if sum != 31 { return 4 }
    // 256 passes over a 4096-entry table.
    expected: s64 = 0
    for i: 0..4095 { expected += (i * 7) % 13 }
    if BigSum(1048575) != expected * 256 { return 5 }
    if Twice() != 22 { return 6 }
    if Grid(4, 4) != 81 { return 7 }
    if Taps(1) != 26 { return 8 }
    if Counted() != 45 { return 9 }
    if Bump(TABLE) != 13 || TABLE[0] != 3 { return 10 }
    got := Copied()
    got[0] = 50
    if got[0] != 50 || got[7] != 6 || TABLE[0] != 3 { return 11 }
    return Lib.NUMBERS[2] + cast(s64) (Pick(0) - 3)
}
#program_export
Outside :: () -> s64 {
    index: s64 = 8
    return cast(s64) Pick(index)
}
ZI

"$ziran" ir --root "$work" -o "$work/ir" "$work/app.zi"
for input in source saved; do
    if test "$input" = source; then
        module=$work/app.zi
        root=$work
    else
        module=$work/ir/app.zir
        root=$work/ir
    fi
    "$ziran" bundle --root "$root" --entry app:Answer \
        -o "$work/$input.zx" "$module"
    test "$("$ziran" run "$work/$input.zx")" = 42
    # The interpreter builds each table once; rebuilding 4096 entries per
    # read would take minutes.
    test "$(timeout 60 "$run_vm" run --vm "$work/$input.zx")" = 42
    "$ziran" bundle --root "$root" --entry app:Outside \
        -o "$work/outside-$input.zx" "$module"
    if "$run_vm" run --vm "$work/outside-$input.zx" > /dev/null 2>&1; then
        echo 'an out-of-range constant table index was accepted' >&2
        exit 1
    fi

    output=$work/c-$input
    "$ziran" build --target=c --root "$root" -o "$output" "$module"
    cat > "$output/main.c" <<'C'
#include <stdlib.h>
#include "app.h"
int main(int argc, char **argv) {
    (void)argv;
    if(argc > 1) return (int)Outside();
    return Answer() == 42 ? 0 : 1;
}
C
    "${CC:-cc}" -std=c99 -pedantic-errors -O2 -Werror=discarded-qualifiers \
        -I"$repo/include" -I"$output" "$output"/*.c -o "$output/app"
    "$output/app"
    if "$output/app" outside 2> /dev/null; then
        echo 'an out-of-range constant table index was accepted' >&2
        exit 1
    fi
    if grep -q '^#define .*\.[[{]' "$output"/*.h; then
        echo 'a header defines a constant table in Ziran spelling:' >&2
        grep -n '^#define .*\.[[{]' "$output"/*.h | cut -c 1-160 >&2
        exit 1
    fi
    # One file-scope table per distinct constant, read without a copy.
    test "$(grep -c '^static const int32_t ZIRAN_APP_H_table[0-9]*\[8\] = {3, 1, 4, 1, 5, 9, 2, 6};' "$output/app.c")" = 1
    grep -q '^static const uint8_t ZIRAN_APP_H_table[0-9]*\[4096\]' "$output/app.c"
    sed -n '/^app_Pick(/,/^}/p' "$output/app.c" > "$work/pick.c"
    grep -q 'ZIRAN_INDEX(ZIRAN_APP_H_table' "$work/pick.c"
    if grep -q 'memmove\|static const' "$work/pick.c"; then
        echo 'a constant table index still copies the table' >&2
        exit 1
    fi
    # .count needs no table, and a returned table is copied once, from
    # the shared table into the result.
    sed -n '/^app_Counted(/,/^}/p' "$output/app.c" > "$work/counted.c"
    if grep -q 'memmove\|static const' "$work/counted.c"; then
        echo 'a constant table count copied the table' >&2
        exit 1
    fi
    sed -n '/^app_Copied(/,/^}/p' "$output/app.c" > "$work/copied.c"
    test "$(grep -c 'memmove' "$work/copied.c")" = 1
    grep -q 'memmove([a-z_0-9]*, ZIRAN_APP_H_table[0-9]*, ' "$work/copied.c"

    output=$work/go-$input
    "$ziran" build --target=go --pkg main --root "$root" -o "$output" "$module"
    cat > "$output/main.go" <<'GO'
package main
func main() { if App_Answer() != 42 { panic("constant tables") } }
GO
    GO111MODULE=off go run "$output"/*.go
    test "$(grep -c '^var App_ziran_table[0-9]* = \[8\]int32{3, 1, 4, 1, 5, 9, 2, 6}' "$output/app.go")" = 1
    sed -n '/^func App_Pick(/,/^}/p' "$output/app.go" > "$work/pick.go"
    grep -q 'return App_ziran_table[0-9]*\[i\]' "$work/pick.go"
    sed -n '/^func App_Answer(/,/^}/p' "$output/app.go" > "$work/answer.go"
    grep -q 'local := App_ziran_table[0-9]*$' "$work/answer.go"
    grep -q 'App_Bump(App_ziran_table[0-9]*)' "$work/answer.go"
done
cmp "$work/source.zx" "$work/saved.zx"
