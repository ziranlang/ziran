#!/bin/sh
# A #parallel body hands results out by writing its own slot, A[it] with any
# field or index after it, of a fixed array the function owns. Iterations
# write distinct slots, so C builds on several threads, natively run
# bundles, the portable VM and Go all agree. Writes the checker cannot prove
# disjoint stay rejected.
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS

cat > "$work/results.zi" <<'ZI'
Point :: struct {
    x: s64
    y: s64
}
Square :: (n: s64) -> s64 { return n * n }
Fill :: (seed: [4]s64) -> s64 {
    squares: [64]s64
    points: [16]Point
    grid: [8][4]s64
    #parallel for 0..63 {
        squares[it] = Square(it)
        squares[it] = squares[it] + seed[it % 4]
    }
    #parallel for 0..15 {
        points[it].x = it
        points[it].y = Square(it)
    }
    #parallel for row: 0..7 {
        for 0..3 {
            grid[row][it] = row * 10 + it
        }
    }
    total: s64 = 0
    for 0..63 { total += squares[it] }
    for 0..15 { total += points[it].x + points[it].y }
    for row: 0..7 {
        for 0..3 { total += grid[row][it] }
    }
    return total
}
#program_export
Answer :: () -> s64 {
    seed: [4]s64
    seed[0] = 3
    seed[1] = 1
    seed[2] = 4
    seed[3] = 1
    return Fill(seed)
}
ZI

"$ziran" ir --root "$work" -o "$work/ir" "$work/results.zi"
for input in source saved; do
    if test "$input" = source; then
        root=$work
        module=$work/results.zi
    else
        root=$work/ir
        module=$work/ir/results.zir
    fi
    "$ziran" bundle --root "$root" --entry results:Answer -o "$work/$input.zx" "$module"
    for threads in 1 4; do
        test "$(ZIRAN_NATIVE_CACHE=0 ZIRAN_PAR_THREADS=$threads "$ziran" run "$work/$input.zx")" = 88016
    done
    "$ziran" build --target=c --entry results:Answer --root "$root" -o "$work/c-$input" "$module"
    cat > "$work/c-$input/main.c" <<'C'
#include "results.h"
#include <stdio.h>
int main(void) { long long total = Answer(); printf("%lld\n", total); return total == 88016 ? 0 : 1; }
C
    "${CC:-cc}" -std=c11 -pedantic-errors -pthread -I"$repo/include" -I"$work/c-$input" \
        "$work/c-$input"/*.c -o "$work/c-$input/app"
    for threads in 1 2 8; do
        ZIRAN_PAR_THREADS=$threads "$work/c-$input/app" > /dev/null
    done
    "$ziran" build --target=go --pkg main --entry results:Answer --root "$root" \
        -o "$work/go-$input" "$module"
    cat > "$work/go-$input/main.go" <<'GO'
package main
func main() { if Results_Answer() != 88016 { panic("parallel results") } }
GO
    GO111MODULE=off go run "$work/go-$input"/*.go
done
cmp "$work/source.zx" "$work/saved.zx"

reject() {
    name=$1
    pattern=$2
    if "$ziran" check --root "$work" "$work/$name.zi" 2> "$work/$name.err"; then
        echo "$3 was accepted" >&2
        exit 1
    fi
    grep -Fq "$pattern" "$work/$name.err" || { cat "$work/$name.err" >&2; exit 1; }
}
bad() {
    cat > "$work/$1.zi" <<ZI
shared: [8]s64;
Bad :: (view: []s64) -> s64 {
    out: [8]s64
$2
    return out[0]
}
ZI
}
bad neighbour '    #parallel for 0..6 {
        out[it + 1] = it
    }'
reject neighbour "iteration-local binding" "a write to another iteration's slot"
bad fixed_slot '    #parallel for 0..7 {
        out[0] = it
    }'
reject fixed_slot "iteration-local binding" "a write every iteration shares"
bad other_read '    #parallel for 0..7 {
        out[it] = out[7 - it]
    }'
reject other_read "uses out only at out[it]" "a read of another iteration's slot"
bad moved_binding '    #parallel for 0..7 {
        it = 0
        out[it] = 1
    }'
reject moved_binding "loop binding unchanged: it" "a reassigned loop binding"
bad global_slot '    #parallel for 0..7 {
        shared[it] = it
    }'
reject global_slot "iteration-local binding" "a write to a global array"
echo "parallel bodies wrote their own slots on every target"
