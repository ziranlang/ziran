#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
compiler=${1:-"$root/build/bin/ziran"}
python3 "$root/tests/gzip_linux_test.py" "$(dirname "$compiler")/zi2c"
