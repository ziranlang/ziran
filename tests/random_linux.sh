#!/bin/sh
set -eu

tool_dir=$(dirname "$1")
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

"$1" check --root tests/spec --module-path std \
    tests/spec/random_linux_test.zi
"$tool_dir/zi2c" --no-main --root tests/spec --module-path std \
    -o "$work/c" tests/spec/random_linux_test.zi
"${CC:-cc}" -std=c11 -Iinclude -I"$work/c" \
    "$work/c/random_linux.c" "$work/c/random_linux_test.c" \
    -x c - -o "$work/test" <<'EOF'
#include "random_linux_test.h"
#include <stdint.h>
int main(void) { return SelfTest() == 42 ? 0 : 1; }
EOF
env -u DISPLAY -u WAYLAND_DISPLAY "$work/test"
