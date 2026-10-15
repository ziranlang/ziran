#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
"${CC:-cc}" ${VM_CFLAGS:-} -std=c11 -D_GNU_SOURCE -I"$repo/include" \
    "$repo/tests/host_bindings_test.c" "${ZIRAN_LIB:-$repo/build/libziran.a}" \
    -lm -pthread -o "$work/test"
python3 "$repo/tests/host_bindings_test.py" "$ziran" "$work"
