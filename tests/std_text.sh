#!/bin/sh
set -eu

ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

"$ziran" check --root tests/spec --module-path std tests/spec/std_text.zi
"$ziran" ir --root tests/spec --module-path std -o "$work/ir" \
    tests/spec/std_text.zi
"$ziran" bundle --root tests/spec --module-path std \
    --entry std_text:SelfTest -o "$work/source.zx" \
    tests/spec/std_text.zi
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    --entry std_text:SelfTest -o "$work/saved.zx" \
    "$work/ir/std_text.zir"
cmp "$work/source.zx" "$work/saved.zx"
test "$("$ziran" run "$work/source.zx")" = 42
