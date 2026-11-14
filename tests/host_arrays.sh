#!/bin/sh
set -eu
ulimit -c 0

ziran=${1:?pass the ziran command}
host_test=${2:?pass the host array test binary}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/arrays.zi" <<'ZI'
host_api :: #system_library "host_api";
Box :: struct {
    empty: [0]s32
    value: s32
}
EchoEmpty :: (values: [0]s32) -> [0]s32 #foreign host_api;
EchoBoxes :: (values: [2]Box) -> [2]Box #foreign host_api;
Grid :: struct {
    cells: [2][2]u8
}
AddGrid :: (grid: Grid, rows: [2][2]u8) -> [2][2]u8 #foreign host_api;

#program_export
Answer :: () -> s32 {
    empty: [0]s32
    returned: [0]s32 = EchoEmpty(empty)
    boxes: [2]Box
    boxes[0].value = 40
    boxes[1].value = 2
    changed: [2]Box = EchoBoxes(boxes)
    grid: Grid
    rows: [2][2]u8
    grid.cells[0][0] = 1
    grid.cells[0][1] = 2
    grid.cells[1][0] = 3
    grid.cells[1][1] = 4
    rows[0][0] = 10
    rows[0][1] = 20
    rows[1][0] = 30
    rows[1][1] = 40
    sums: [2][2]u8 = AddGrid(grid, rows)
    if sums[0][0] != 11 || sums[0][1] != 22 || sums[1][0] != 33 ||
       sums[1][1] != 44 { return 0 }
    if returned.count != 0 || returned.data != null ||
       changed.count != 2 || changed[0].empty.count != 0 ||
       changed[1].empty.count != 0 { return 0 }
    return changed[0].value + changed[1].value
}
ZI

"$ziran" ir --root "$work" -o "$work/ir" "$work/arrays.zi"
"$ziran" bundle --root "$work" --entry arrays:Answer \
    -o "$work/source.zx" "$work/arrays.zi"
"$ziran" bundle --root "$work/ir" --entry arrays:Answer \
    -o "$work/saved.zx" "$work/ir/arrays.zir"
cmp "$work/source.zx" "$work/saved.zx"
"$host_test" "$work/source.zx"
"$host_test" "$work/saved.zx"

for input in source saved; do
    if test "$input" = source; then
        root=$work
        file=$work/arrays.zi
    else
        root=$work/ir
        file=$work/ir/arrays.zir
    fi
    for target in c go; do
        out=$work/$target-$input
        if test "$target" = go; then
            "$ziran" build --target=go --pkg main --root "$root" \
                -o "$out" "$file"
            cat > "$out/main.go" <<'GO'
package main
type arrayHost struct{}
func (arrayHost) EchoEmpty(values [0]int32) [0]int32 { return values }
func (arrayHost) EchoBoxes(values [2]Box) [2]Box { return values }
func (arrayHost) AddGrid(grid Grid, rows [2][2]uint8) [2][2]uint8 {
	for i := range rows { for j := range rows[i] { rows[i][j] += grid.Cells[i][j] } }
	return rows
}
func init() { SetArraysHost(arrayHost{}) }
func main() { if Arrays_Answer() != 42 { panic("array host") } }
GO
            GO111MODULE=off go run "$out"/*.go
        elif test "$target" = c; then
            "$ziran" build --target=c --root "$root" -o "$out" "$file"
            cat > "$out/main.c" <<'C'
#include "arrays.h"
void EchoEmpty(int32_t *result, int32_t *values) {
    (void)result; (void)values;
}
void EchoBoxes(Box result[2], Box values[2]) {
    result[0] = values[0]; result[1] = values[1];
}
void AddGrid(uint8_t result[2][2], Grid grid, uint8_t rows[2][2]) {
    for(int i = 0; i < 2; i++)
        for(int j = 0; j < 2; j++)
            result[i][j] = (uint8_t)(grid.cells[i][j] + rows[i][j]);
}
int main(void) { return Answer() == 42 ? 0 : 1; }
C
            "${CC:-cc}" -std=c99 -Wall -Werror -pedantic-errors \
                -I"$(dirname "$0")/../include" -I"$out" \
                "$out"/*.c -o "$out/app"
            "$out/app"
        fi
    done
done
