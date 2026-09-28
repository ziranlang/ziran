#!/bin/sh
set -eu

ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

"$ziran" check --root tests/spec --module-path std tests/spec/std_number_text.zi
"$ziran" ir --root tests/spec --module-path std -o "$work/ir" \
    tests/spec/std_number_text.zi
"$ziran" bundle --root tests/spec --module-path std \
    --entry std_number_text:SelfTest -o "$work/source.zib" \
    tests/spec/std_number_text.zi
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    --entry std_number_text:SelfTest -o "$work/saved.zib" \
    "$work/ir/std_number_text.zir"
cmp "$work/source.zib" "$work/saved.zib"
test "$("$ziran" run "$work/source.zib")" = 42
