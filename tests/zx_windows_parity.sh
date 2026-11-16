#!/bin/sh
# check-cost: 4
# The parity programs as Windows x86-64 executables under Wine print and exit
# as their Linux native ELFs.
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
exec "$ziran" run --target=c --root "$repo/tests" "$repo/tests/zx_windows_parity_cases.zi" -- "$ziran"
