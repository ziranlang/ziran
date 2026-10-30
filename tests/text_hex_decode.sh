#!/bin/sh
set -eu

ziran=$1
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$root/work"
work=$(mktemp -d "$root/work/text_hex_decode.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM

"$ziran" check --root tests/spec --module-path std tests/spec/text_hex_decode.zi
"$ziran" ir --root tests/spec --module-path std -o "$work/ir" \
    tests/spec/text_hex_decode.zi
"$ziran" bundle --root tests/spec --module-path std \
    --entry text_hex_decode:SelfTest -o "$work/source.zx" \
    tests/spec/text_hex_decode.zi
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    --entry text_hex_decode:SelfTest -o "$work/saved.zx" \
    "$work/ir/text_hex_decode.zir"
cmp "$work/source.zx" "$work/saved.zx"
test "$("$ziran" run "$work/source.zx")" = 42
test "$("$ziran" run "$work/saved.zx")" = 42

for input in source saved; do
    if test "$input" = source; then
        module=$root/tests/spec/text_hex_decode.zi
        build_root=$root/tests/spec
    else
        module=$work/ir/text_hex_decode.zir
        build_root=$work/ir
    fi
    "$ziran" build --target=c --root "$build_root" --module-path "$root/std" \
        --entry text_hex_decode:SelfTest -o "$work/c-$input" "$module"
    cat > "$work/c-$input/entry.c" <<'C'
#include <stdio.h>
#include "text_hex_decode.h"
int main(void) {
    if (SelfTest() != 42) return 1;
    return 0;
}
C
    "${CC:-cc}" -std=c11 -I"$root/include" -I"$work/c-$input" \
        "$work/c-$input"/*.c -lm -o "$work/c-$input/app"
    "$work/c-$input/app"
done

echo "TextHexDecodeInto: bounded append decoding is atomic on failure"
