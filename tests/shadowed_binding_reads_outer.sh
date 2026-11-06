#!/bin/sh
# A declaration that shadows a binding of the same name reads the outer
# binding in its own initializer, as in `x := x + 5` inside a block or
# `i := i * 10` in a for body, and the loop keeps counting with its own
# binder. C, Go, native and the portable VM agree from source and saved IR.
set -eu

ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
run_vm="$(dirname "$ziran")/zi2zx"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
#program_export
Answer :: () -> s64 {
    r: s64 = 0
    // 5 + 6 + ... + 12
    for i: 0..7 { i := i + 5; r += i }
    if r != 68 { return 1 }
    x: s64 = 1
    {
        x := x + 5
        r += x
    }
    if r != 74 || x != 1 { return 2 }
    // The step before continue still counts the loop's own binder.
    total: s64 = 0
    for i: 0..3 {
        if i == 1 { continue }
        i := i * 10
        total += i
    }
    if total != 50 { return 3 }
    nested: s64 = 0
    for i: 0..4 {
        {
            i := i + 100
            if i == 102 { continue }
            nested += i
        }
    }
    if nested != 408 { return 4 }
    values: [3]s64
    values[0] = 1
    values[1] = 2
    values[2] = 3
    walked: s64 = 0
    for v, k: values {
        v := v * 2
        k := k + 10
        walked += v * k
    }
    // 2*10 + 4*11 + 6*12
    if walked != 136 { return 5 }
    {
        values := values
        values[0] = 50
        if values[0] != 50 { return 6 }
    }
    if values[0] != 1 { return 7 }
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
    test "$(timeout 60 "$ziran" run "$work/$input.zx")" = 42
    test "$(timeout 60 "$run_vm" run --vm "$work/$input.zx")" = 42

    output=$work/c-$input
    "$ziran" build --target=c --root "$root" -o "$output" "$module"
    cat > "$output/main.c" <<'C'
#include "app.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
    # GCC's -Winit-self catches a declaration reading itself.
    "${CC:-cc}" -std=c99 -pedantic-errors -Wall -Winit-self -Werror=uninitialized -O2 \
        -I"$repo/include" -I"$output" "$output"/*.c -o "$output/app"
    timeout 60 "$output/app"
    if grep -Eq 'int64_t (i|x) = \(int64_t\)\(\(uint64_t\)(i|x) ' "$output/app.c"; then
        echo 'a C declaration reads itself' >&2
        exit 1
    fi

    output=$work/go-$input
    "$ziran" build --target=go --pkg main --root "$root" -o "$output" "$module"
    cat > "$output/main.go" <<'GO'
package main
func main() { if App_Answer() != 42 { panic("shadowed binding") } }
GO
    (cd "$output" && GO111MODULE=off timeout 120 go run .)
done
cmp "$work/source.zx" "$work/saved.zx"
