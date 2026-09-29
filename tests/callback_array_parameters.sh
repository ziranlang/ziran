#!/bin/sh
set -eu

ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
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

#program_export
Answer :: () -> s32 { return 42 }
EOF

"$ziran" ir --root "$work" -o "$work/ir" "$work/readers.zi"
for target in c cpp plan9-c; do
    "$ziran" build --target="$target" --root "$work" \
        -o "$work/$target" "$work/readers.zi"
    "$ziran" build --target="$target" --root "$work/ir" \
        -o "$work/$target-ir" "$work/ir/readers.zir"
    case "$target" in cpp) header=readers.hpp;; *) header=readers.h;; esac
    cmp "$work/$target/$header" "$work/$target-ir/$header"
    rg -Fq 'typedef int32_t (*ReadRow)(int32_t (*)[4], int32_t);' "$work/$target/$header"
    rg -Fq 'typedef int32_t (*ReadGrid)(int32_t (*)[2][4], int32_t);' "$work/$target/$header"
    rg -Fq 'typedef int32_t (*ReadIndirect)(int32_t (**)[4]);' "$work/$target/$header"
done

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
sed 's/"readers.h"/"readers.hpp"/' "$work/driver.c" > "$work/driver.cpp"
"${CXX:-c++}" -std=c++11 -Wall -Werror -I"$work/cpp" \
    "$work/cpp/readers.cpp" "$work/driver.cpp" -o "$work/cpp/run"
"$work/cpp/run"

# Use Plan 9 headers and its host compiler when available. The Taiji Rill
# gate separately exercises these service signatures with native 8c/8l.
plan9=${PLAN9:-"$repo/../../plan9port"}
if test -x "$plan9/bin/9c"; then
    sed '/^int main(void)/,$d' "$work/driver.c" > "$work/plan9-c/driver.c"
    cat >> "$work/plan9-c/driver.c" <<'EOF'
void main(void) {
    int32_t rows[2][4] = {{0, 1, 2, 3}, {4, 5, 6, 42}};
    int32_t grids[2][2][4] = {{{0}}, {{0}, {0, 1, 42, 3}}};
    int32_t (*pointer)[4] = rows;
    Readers readers = {row, grid, indirect};
    exits(Answer() == 42 && readers.row(rows, 1) == 42 &&
        readers.grid(grids, 1) == 42 && readers.indirect(&pointer) == 42 ? nil : "failed");
}
EOF
    (cd "$work/plan9-c" &&
        PLAN9="$plan9" "$plan9/bin/9c" -I. readers.c driver.c &&
        PLAN9="$plan9" "$plan9/bin/9l" -o run readers.o driver.o && ./run)
fi
echo 'callback array parameters: C, C++, Plan 9 C and saved IR passed'
