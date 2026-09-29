#!/bin/sh
set -eu

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

"$1" check --root tests/spec --module-path std \
    tests/spec/constant_time_test.zi
"$1" bundle --root tests/spec --module-path std \
    --entry constant_time_test:SelfTest \
    -o "$work/test.zib" tests/spec/constant_time_test.zi
test "$("$1" run "$work/test.zib")" = "42"
