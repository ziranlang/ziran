#!/bin/sh
set -eu

ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$repo/build/test"
work=$(mktemp -d "$repo/build/test/callback_array_parameters.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/readers.zi" <<'EOF'
ReadRow :: #type (rows: *[4]s32, row: s32) -> s32 #c_call;
ReadGrid :: #type (grids: *[2][4]s32, grid: s32) -> s32 #c_call;
ReadIndirect :: #type (rows: **[4]s32) -> s32 #c_call;

Readers :: struct {
    row: ReadRow
    grid: ReadGrid
    indirect: ReadIndirect
}

PublicValue :: 40;
#scope_file
PrivateValue :: 2;
#scope_export
#program_export
Answer :: () -> s32 { return PublicValue + PrivateValue }
EOF

"$ziran" ir --root "$work" -o "$work/ir" "$work/readers.zi"
"$ziran" build --target=c --root "$work" \
    -o "$work/c" "$work/readers.zi"
"$ziran" build --target=c --root "$work/ir" \
    -o "$work/c-ir" "$work/ir/readers.zir"
cmp "$work/c/readers.h" "$work/c-ir/readers.h"
rg -Fq 'typedef int32_t (*ReadRow)(int32_t (*)[4], int32_t);' "$work/c/readers.h"
rg -Fq 'typedef int32_t (*ReadGrid)(int32_t (*)[2][4], int32_t);' "$work/c/readers.h"
rg -Fq 'typedef int32_t (*ReadIndirect)(int32_t (**)[4]);' "$work/c/readers.h"

cat > "$work/driver.c" <<'EOF'
#include "readers.h"
static int32_t row(int32_t (*rows)[4], int32_t index) { return rows[index][3]; }
static int32_t grid(int32_t (*grids)[2][4], int32_t index) { return grids[index][1][2]; }
static int32_t indirect(int32_t (**rows)[4]) { return (*rows)[1][3]; }
int main(void) {
    int32_t rows[2][4] = {{0, 1, 2, 3}, {4, 5, 6, 42}};
    int32_t grids[2][2][4] = {{{0}}, {{0}, {0, 1, 42, 3}}};
    int32_t (*pointer)[4] = rows;
    Readers readers = {row, grid, indirect};
    return Answer() == 42 && readers.row(rows, 1) == 42 &&
        readers.grid(grids, 1) == 42 && readers.indirect(&pointer) == 42 ? 0 : 1;
}
EOF
"${CC:-cc}" -std=c11 -Wall -Werror -I"$work/c" \
    "$work/c/readers.c" "$work/driver.c" -o "$work/c/run"
"$work/c/run"

echo 'callback array parameters: C and saved IR passed'
