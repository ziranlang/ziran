#!/bin/sh
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/recursive.zi" <<'ZI'
Node :: struct {
    next: *#this
    value: s32
}

Chain :: struct($T: Type) {
    next: *#this
    value: T
}

Factorial :: (value: s32) -> s32 {
    if value <= 1 { return 1 }
    return value * #this(value - 1)
}

Repeat :: (value: $T, count: s32) -> T {
    if count == 0 { return value }
    return #this(value, count - 1)
}

Shadow :: (Shadow: s32) -> s32 {
    if Shadow == 0 { return 0 }
    return 1 + #this(Shadow - 1)
}

Callback :: #type (value: s32) -> s32;
ViaValue :: (value: s32) -> s32 {
    if value == 0 { return 0 }
    recurse: Callback = #this
    return 1 + recurse(value - 1)
}

SlotShadow :: (SlotShadow: s32) -> s32 {
    if SlotShadow == 0 { return 0 }
    recurse: Callback = #this
    return 1 + recurse(SlotShadow - 1)
}

#program_export
NativeSelf :: () -> s32 {
    node: Node
    node.next = null
    node.value = 40
    chain: Chain(s32)
    chain.next = null
    chain.value = 2
    return node.value + chain.value
}

#program_export
Answer :: () -> s32 {
    return Factorial(4) + Factorial(3) + Repeat(cast(s32)12, 2) + Shadow(2) + ViaValue(1) + SlotShadow(1)
}
ZI

"$ziran" ir --root "$work" -o "$work/ir" "$work/recursive.zi"
for input in source saved; do
    if test "$input" = source; then
        module=$work/recursive.zi
        module_dir=$work
    else
        module=$work/ir/recursive.zir
        module_dir=$work/ir
    fi
    "$ziran" bundle --root "$module_dir" --entry recursive:Answer \
        -o "$work/$input.zx" "$module"
    test "$("$ziran" run "$work/$input.zx")" = 46

    for target in c go; do
        out="$work/$target-$input"
        if test "$target" = go; then
            "$ziran" build --target=go --pkg main --root "$module_dir" \
                -o "$out" "$module"
            cat > "$out/main.go" <<'GO'
package main
func main() {
    if Recursive_Answer() != 46 { panic("#this recursion") }
    if Recursive_NativeSelf() != 42 { panic("#this type") }
}
GO
            GO111MODULE=off go run "$out"/*.go
        elif test "$target" = c; then
            "$ziran" build --target=c --root "$module_dir" \
                -o "$out" "$module"
            cat > "$out/main.c" <<'C'
#include "recursive.h"
int main(void) { return Answer() == 46 && NativeSelf() == 42 ? 0 : 1; }
C
            "${CC:-cc}" -std=c11 -I"$repo/include" -I"$out" \
                "$out"/*.c -o "$out/app"
            "$out/app"
        fi
    done
done
cmp "$work/source.zx" "$work/saved.zx"

cat > "$work/outside.zi" <<'ZI'
Outside :: #this;
ZI
if "$ziran" check --root "$work" "$work/outside.zi" \
    2> "$work/outside.err"; then
    echo '#this was accepted outside a procedure or type scope' >&2
    exit 1
fi
grep -Fq '#this requires a procedure or type scope' "$work/outside.err"
