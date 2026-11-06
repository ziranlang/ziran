#!/bin/sh
# A file-scope variable may start from a constant table of any size, named
# or written out, from its own module or an import. Each target writes the
# whole initializer into the variable's definition, so the program does no
# copying when it starts, and the variable stays separate from the table.
# Results agree on C, Go, native and the portable VM from source and saved
# IR.
set -eu

ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
run_vm="$(dirname "$ziran")/zi2zx-vm"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

# Wrapped lists: 4096 bytes, 6000 wide numbers and 3000 written out.
list() {
    awk -v n="$1" -v m="$2" -v s="$3" 'BEGIN {
        for(i = 0; i < n; i++)
            printf "%s%d", i ? (i % 24 ? ", " : ",\n    ") : "", (i * s) % m
    }'
}
cat > "$work/library.zi" <<ZI
BIG :: #run u8.[$(list 4096 13 7)];
ZI
cat > "$work/app.zi" <<ZI
Lib :: #import "library";
WIDE :: #run s32.[$(list 6000 1000003 7919)];
bytes: [4096]u8 = Lib.BIG;
wide: [6000]s32 = WIDE;
direct: [3000]s64 = s64.[$(list 3000 99991 104729)];
#program_export
Answer :: () -> s64 {
    sum: s64 = 0
    for i: 0..4095 { sum += cast(s64) bytes[i] - cast(s64) ((i * 7) % 13) }
    for i: 0..5999 { sum += cast(s64) wide[i] - (i * 7919) % 1000003 }
    for i: 0..2999 { sum += direct[i] - (i * 104729) % 99991 }
    if sum != 0 { return 1 }
    bytes[1] = 200
    wide[5999] = -1
    if Lib.BIG[1] != 7 || WIDE[5999] != (5999 * 7919) % 1000003 { return 2 }
    if bytes[1] != 200 || wide[5999] != -1 { return 3 }
    return 42
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
    test "$("$run_vm" run "$work/$input.zx")" = 42

    output=$work/c-$input
    "$ziran" build --target=c --root "$root" -o "$output" "$module"
    cat > "$output/main.c" <<'C'
#include "app.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
    "${CC:-cc}" -std=c99 -pedantic-errors -O2 \
        -I"$repo/include" -I"$output" "$output"/*.c -o "$output/app"
    "$output/app"
    # Each variable is defined with its values; nothing fills it at startup.
    grep -q '^uint8_t bytes\[4096\] = {0, 7, 1, 8,' "$output/app.c"
    grep -q '^int32_t wide\[6000\] = {0, 7919, 15838,' "$output/app.c"
    grep -q '^int64_t direct\[3000\] = {0LL, 4738LL,' "$output/app.c"
    if grep -q '__global_init\|memmove(\(bytes\|wide\|direct\)' "$output/app.c"; then
        echo 'a constant global was filled at startup' >&2
        exit 1
    fi

    output=$work/go-$input
    "$ziran" build --target=go --pkg main --root "$root" -o "$output" "$module"
    cat > "$output/main.go" <<'GO'
package main
func main() { if App_Answer() != 42 { panic("large constant globals") } }
GO
    GO111MODULE=off go run "$output"/*.go
    grep -q '^var bytes \[4096\]uint8 = \[4096\]uint8{0, 7, 1, 8,' "$output/app.go"
    grep -q '^var wide \[6000\]int32 = \[6000\]int32{0, 7919,' "$output/app.go"
    grep -q '^var direct \[3000\]int64 = \[3000\]int64{0, 4738,' "$output/app.go"
done
cmp "$work/source.zx" "$work/saved.zx"
