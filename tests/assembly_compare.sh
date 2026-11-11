#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS GDK_DISPLAY
exec "$ziran" run --target=c --root "$repo/tests" --module-path "$repo/bench" \
    "$repo/tests/assembly_compare_cases.zi" -- "$ziran"
