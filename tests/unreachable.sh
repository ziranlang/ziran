#!/bin/sh
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/flow.zi" <<'ZI'
#program_export
Answer :: () -> s32 {
    if false { unreachable }
    return 42
}
#program_export
Trap :: () -> s32 {
    unreachable
}
ZI

"$ziran" check --root "$work" "$work/flow.zi"
"$ziran" ir --root "$work" -o "$work/ir" "$work/flow.zi"
for input in source saved; do
    if test "$input" = source; then
        module=$work/flow.zi
        root=$work
    else
        module=$work/ir/flow.zir
        root=$work/ir
    fi
    "$ziran" bundle --root "$root" --entry flow:Answer \
        -o "$work/$input.zx" "$module"
    test "$("$ziran" run "$work/$input.zx")" = 42
    "$ziran" bundle --root "$root" --entry flow:Trap \
        -o "$work/trap-$input.zx" "$module"
    if "$ziran" run "$work/trap-$input.zx" 2> "$work/trap.err"; then
        echo 'unreachable path unexpectedly returned' >&2
        exit 1
    fi
    grep -Fq 'unreachable code executed' "$work/trap.err"

    for target in c go; do
        output="$work/$target-$input"
        if test "$target" = go; then
            "$ziran" build --target=go --pkg main --root "$root" \
                -o "$output" "$module"
            cat > "$output/main.go" <<'GO'
package main
func main() { if Flow_Answer() != 42 { panic("wrong result") } }
GO
            GO111MODULE=off go run "$output"/*.go
        elif test "$target" = c; then
            "$ziran" build --target=c --root "$root" \
                -o "$output" "$module"
            cat > "$output/main.c" <<'C'
#include "flow.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
            "${CC:-cc}" -I"$repo/include" -I"$output" \
                "$output"/*.c -o "$output/app"
            "$output/app"
        fi
    done
done
cmp "$work/source.zx" "$work/saved.zx"
