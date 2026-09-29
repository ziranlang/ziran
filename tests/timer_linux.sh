#!/bin/sh
set -eu

tool_dir=$(dirname "$1")
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

"$1" check --root tests/spec --module-path std \
    tests/spec/timer_linux_test.zi
"$tool_dir/zi2c" --no-main --root tests/spec --module-path std \
    -o "$work/c" tests/spec/timer_linux_test.zi
cat > "$work/c/main.c" <<'EOF'
#include "timer_linux_test.h"
int main(void) { return SelfTest() == 42 ? 0 : 1; }
EOF
"${CC:-cc}" -std=c11 -Iinclude -I"$work/c" \
    "$work/c/timer_linux.c" "$work/c/timer_linux_test.c" "$work/c/main.c" \
    -o "$work/test"
env -u DISPLAY -u WAYLAND_DISPLAY "$work/test"
