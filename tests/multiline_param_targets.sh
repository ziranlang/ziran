#!/bin/sh
set -eu

ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/params.zi" <<'ZI'
#program_export
Combine :: (
    first: float32,
    second: float32
) -> float32 {
    return first + second
}

#program_export
Answer :: () -> s32 {
    if Combine(20.0, 22.0) == 42.0 { return 42 }
    return 0
}
ZI

"$ziran" check --root "$work" "$work/params.zi"
"$ziran" ir --root "$work" -o "$work/ir" "$work/params.zi"
"$ziran" bundle --root "$work" --entry params:Answer \
    -o "$work/source.zx" "$work/params.zi"
"$ziran" bundle --root "$work" --entry params:Answer \
    -o "$work/ir.zx" "$work/ir/params.zir"
cmp "$work/source.zx" "$work/ir.zx"
test "$("$ziran" run "$work/source.zx")" = 42
for input in "$work/params.zi" "$work/ir/params.zir"; do
    go_output="$work/go-$(basename "$input")"
    "$ziran" build --target=go --pkg main --root "$work" \
        -o "$go_output" "$input"
    cat > "$go_output/main.go" <<'GO'
package main
func main() { if Params_Answer() != 42 { panic("wrong multiline parameter") } }
GO
    GO111MODULE=off go run "$go_output/params.go" "$go_output/main.go"

    c_output="$work/c-$(basename "$input")"
    "$ziran" build --target=c --root "$work" -o "$c_output" "$input"
    cat > "$c_output/main.c" <<'C'
#include "params.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
    ${CC:-cc} -Iinclude -I"$c_output" "$c_output/params.c" \
        "$c_output/main.c" -o "$c_output/test"
    "$c_output/test"

done
