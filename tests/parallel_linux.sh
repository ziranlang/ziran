#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
"${CC:-cc}" -std=c11 -Iinclude tests/parallel_native_lanes_test.c -pthread -o "$work/lanes-c"
for count in 1 2 8 64; do
    ZIRAN_PAR_THREADS=$count "$work/lanes-c"
done
"$ziran" check --root tests/spec --module-path std tests/spec/parallel_linux_test.zi
"$ziran" build --target=c --no-main --root tests/spec --module-path std \
    -o "$work/c" tests/spec/parallel_linux_test.zi
printf '#include "parallel_linux_test.h"\nint main(void) { return SelfTest() == 42 ? 0 : 1; }\n' > "$work/c/main.c"
"${CC:-cc}" -std=c11 -Iinclude -I"$work/c" "$work/c"/*.c -pthread -o "$work/test"
env -u ZIRAN_PAR_THREADS "$work/test"
for count in 1 2 8 64; do ZIRAN_PAR_THREADS=$count "$work/test"; done
