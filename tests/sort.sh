#!/bin/sh
set -eu

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

"$1" check --root tests/spec --module-path std tests/spec/sort_test.zi
"$1" bundle --root tests/spec --module-path std \
    --entry sort_test:SelfTest -o "$work/test.zib" tests/spec/sort_test.zi
test "$("$1" run "$work/test.zib")" = "42"

"$1" build --target=c --root tests/spec --module-path std \
    -o "$work/c" tests/spec/sort_test.zi
printf '#include "sort_test.h"\nint main(void) { return SelfTest() == 42 ? 0 : 1; }\n' \
    > "$work/c/run.c"
"${CC:-cc}" -std=c11 -I"$work/c" "$work"/c/*.c -o "$work/c/program"
"$work/c/program"
