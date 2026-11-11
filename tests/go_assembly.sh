#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build=$(CDPATH= cd -- "$(dirname -- "$ziran")/.." && pwd)
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
"$ziran" run --target=c --root "$repo/tests" "$repo/tests/go_assembly_cases.zi" -- "$ziran" "$build/tests/go-assembly"
