#!/bin/sh
set -eu

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

"$1" check --root tests/spec --module-path std \
    tests/spec/thread_linux_test.zi
for target in c cpp; do
"$1" build --target="$target" --no-main --root tests/spec --module-path std \
    -o "$work/$target" tests/spec/thread_linux_test.zi
if test "$target" = c; then
cat > "$work/$target/main.c" <<'C'
#include "thread_linux_test.h"
int main(void) { return SelfTest() == 42 ? 0 : 1; }
C
"${CC:-cc}" -std=c11 -Iinclude -I"$work/$target" "$work/$target"/*.c -pthread -o "$work/test"
else
cat > "$work/$target/main.cpp" <<'CPP'
#include "thread_linux_test.hpp"
int main() { return SelfTest() == 42 ? 0 : 1; }
CPP
"${CXX:-c++}" -std=c++17 -Iinclude -I"$work/$target" "$work/$target"/*.cpp -pthread -o "$work/test"
fi
env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS "$work/test"
done
