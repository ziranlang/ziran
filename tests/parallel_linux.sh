#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
"$ziran" check --root tests/spec --module-path std tests/spec/parallel_linux_test.zi
for target in c cpp; do
    "$ziran" build --target="$target" --no-main --root tests/spec --module-path std \
        -o "$work/$target" tests/spec/parallel_linux_test.zi
    if test "$target" = c; then
        printf '#include "parallel_linux_test.h"\nint main(void) { return SelfTest() == 42 ? 0 : 1; }\n' > "$work/$target/main.c"
        "${CC:-cc}" -std=c11 -Iinclude -I"$work/$target" "$work/$target"/*.c -pthread -o "$work/test"
    else
        printf '#include "parallel_linux_test.hpp"\nint main() { return SelfTest() == 42 ? 0 : 1; }\n' > "$work/$target/main.cpp"
        "${CXX:-c++}" -std=c++17 -Iinclude -I"$work/$target" "$work/$target"/*.cpp -pthread -o "$work/test"
    fi
    env -u ZIRAN_PAR_THREADS "$work/test"
    for count in 1 2 8 64; do ZIRAN_PAR_THREADS=$count "$work/test"; done
done
