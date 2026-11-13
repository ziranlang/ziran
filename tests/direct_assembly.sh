#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
"$ziran" run --target=c --root "$repo/tests" --module-path "$repo/cmd" "$repo/tests/assembly_cases.zi" -- "$ziran"
