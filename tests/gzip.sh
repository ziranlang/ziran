#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
compiler=${1:-"$root/build/bin/ziran"}
"$compiler" run --target=c --root "$root/tests" "$root/tests/gzip_cases.zi" -- "$(dirname "$compiler")/zi2c"
