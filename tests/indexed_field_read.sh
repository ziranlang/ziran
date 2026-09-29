#!/bin/sh
# Reading one element of an array reached through an indexed record, as in
# `rows[slot].bytes[i]`, reads it in place: it must not copy the whole array
# into a temporary (a 64 KiB copy per byte overflowed the WebAssembly stack).
set -eu

ziran=${1:?pass the ziran command}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/rows.zi" <<'ZI'
Row :: struct {
    bytes: [4096]u8
    count: s32
}
rows: [4]Row;

#program_export
Sum :: (slot: s32) -> s32 {
    total: s32 = 0
    i: s32 = 0
    while i < rows[slot].count {
        total += cast(s32)rows[slot].bytes[i]
        i += 1
    }
    return total
}

#program_export
Answer :: () -> s32 {
    rows[2].count = 3
    rows[2].bytes[0] = 10
    rows[2].bytes[1] = 20
    rows[2].bytes[2] = 12
    rows[1].bytes[0] = 99
    return Sum(2)
}
ZI

"$ziran" build --target=c --root "$work" -o "$work/c" "$work/rows.zi"
if grep -q "uint8_t value_[0-9]*\[4096\]" "$work/c/rows.c"; then
    echo "an indexed field read copied the whole array" >&2
    exit 1
fi
cat > "$work/c/main.c" <<'C'
#include "rows.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
"${CC:-cc}" -std=c11 -I"$(dirname "$0")/../include" -I"$work/c" "$work/c"/*.c -o "$work/rows"
"$work/rows"
"$ziran" bundle --root "$work" --entry rows:Answer -o "$work/rows.zib" "$work/rows.zi"
test "$("$ziran" run "$work/rows.zib")" = 42
echo "Indexed field reads stay in place"
