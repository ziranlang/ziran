#!/bin/sh
# A constant array of arrays is a constant table too. Its rows name their
# own type, as in GRID :: [3]s32.[s32.[1, 2, 3], s32.[4, 5, 6]], and #run
# can build one in a procedure that sets g[a][b][c]. Reading GRID[i][j]
# reads the one shared table in place, with no copy of the row, on C, Go,
# native and the portable VM from source and saved IR. Copies keep value
# semantics, an array type alias still names a type, and an index past a
# row aborts.
set -eu

ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
run_vm="$(dirname "$ziran")/zi2zx"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/library.zi" <<'ZI'
Steps :: () -> [4][2]s64 {
    t: [4][2]s64
    for i: 0..3 {
        t[i][0] = i * i
        t[i][1] = -i
    }
    return t
}
STEPS :: #run Steps();
ZI
cat > "$work/app.zi" <<'ZI'
Lib :: #import "library";
Row :: [3]s32;
GRID :: [3]s32.[s32.[1, 2, 3], s32.[4, 5, 6]];
WIDE :: #run [3]s32.[s32.[-7, 8, 9], s32.[10, 11, 12]];
Cube :: () -> [2][2][3]u8 {
    g: [2][2][3]u8
    for a: 0..1 {
        for b: 0..1 {
            for c: 0..2 { g[a][b][c] = cast(u8)(a * 100 + b * 10 + c) }
        }
    }
    return g
}
CUBE :: #run Cube();
start: [2][3]s32 = GRID;
Pick :: (i: s64, j: s64) -> s32 { return GRID[i][j] }
Corner :: (a: s64, b: s64, c: s64) -> u8 { return CUBE[a][b][c] }
Sum :: () -> s64 {
    total: s64 = 0
    for i: 0..1 { for j: 0..2 { total += cast(s64) GRID[i][j] + cast(s64) WIDE[i][j] } }
    return total
}
#program_export
Answer :: () -> s64 {
    if Pick(1, 2) != 6 || Pick(0, 0) != 1 || WIDE[0][0] != -7 { return 1 }
    if Corner(1, 1, 2) != 112 || Corner(0, 1, 0) != 10 { return 2 }
    if Sum() != 21 + 43 { return 3 }
    if Lib.STEPS[3][0] != 9 || Lib.STEPS[2][1] != -2 { return 4 }
    local: [2][3]s32 = GRID
    local[1][2] = 99
    start[0][1] = -5
    if GRID[1][2] != 6 || local[1][2] != 99 || start[0][1] != -5 || GRID[0][1] != 2 { return 5 }
    r: Row = s32.[7, 8, 9]
    if r[2] != 9 || GRID.count != 2 || GRID[0].count != 3 { return 6 }
    return 42
}
#program_export
Outside :: () -> s64 {
    j: s64 = 3
    return cast(s64) Pick(0, j)
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
    test "$("$run_vm" run --vm "$work/$input.zx")" = 42
    "$ziran" bundle --root "$root" --entry app:Outside \
        -o "$work/outside-$input.zx" "$module"
    if "$run_vm" run --vm "$work/outside-$input.zx" > /dev/null 2>&1; then
        echo 'an index past a constant table row was accepted' >&2
        exit 1
    fi

    output=$work/c-$input
    "$ziran" build --target=c --root "$root" -o "$output" "$module"
    cat > "$output/main.c" <<'C'
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
        echo 'an index past a constant table row was accepted' >&2
        exit 1
    fi
    grep -q '^static const int32_t ZIRAN_APP_H_table[0-9]*\[2\]\[3\] = {{1, 2, 3}, {4, 5, 6}};' "$output/app.c"
    grep -q '^static const uint8_t ZIRAN_APP_H_table[0-9]*\[2\]\[2\]\[3\] = {{{0, 1, 2}, {10, 11, 12}}, {{100, 101, 102}, {110, 111, 112}}};' "$output/app.c"
    grep -q '^int32_t start\[2\]\[3\] = {{1, 2, 3}, {4, 5, 6}};' "$output/app.c"
    # Each read indexes the table itself; no row is copied out first.
    for name in Pick Corner Sum; do
        sed -n "/^app_$name(/,/^}/p" "$output/app.c" > "$work/$name.c"
        grep -q 'ZIRAN_APP_H_table' "$work/$name.c"
        if grep -q 'memmove\|static const' "$work/$name.c"; then
            echo "$name copies a constant table row" >&2
            exit 1
        fi
    done
    grep -q 'ZIRAN_INDEX(ZIRAN_INDEX(ZIRAN_APP_H_table[0-9]*, 2, i), 3, j)' "$work/Pick.c"

    output=$work/go-$input
    "$ziran" build --target=go --pkg main --root "$root" -o "$output" "$module"
    cat > "$output/main.go" <<'GO'
package main
func main() { if App_Answer() != 42 { panic("nested constant tables") } }
GO
    GO111MODULE=off go run "$output"/*.go
    grep -q '^var App_ziran_table[0-9]* = \[2\]\[3\]int32{{1, 2, 3}, {4, 5, 6}}' "$output/app.go"
    sed -n '/^func App_Pick(/,/^}/p' "$output/app.go" > "$work/pick.go"
    grep -q 'return App_ziran_table[0-9]*\[i\]\[j\]' "$work/pick.go"
done
cmp "$work/source.zx" "$work/saved.zx"
