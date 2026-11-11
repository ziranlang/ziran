#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
library=${ZIRAN_LIB:-"$(dirname "$ziran")/../libziran.a"}
build=$(CDPATH= cd -- "$(dirname -- "$library")" && pwd)
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
"$ziran" run --target=c --root "$repo/tests" "$repo/tests/go_direct_assembly_cases.zi" -- "$ziran" "$build/tests/go-direct-assembly"
