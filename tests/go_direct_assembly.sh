#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
library=${ZIRAN_LIB:-"$(dirname "$ziran")/../libziran.a"}
build=$(CDPATH= cd -- "$(dirname -- "$library")" && pwd)
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
python3 "$repo/tests/go_direct_assembly_test.py" "$ziran" "$build/tests/go-direct-assembly"
