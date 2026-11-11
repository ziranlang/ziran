#!/bin/sh
# check-cost: 110
set -eu
ulimit -c 0
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${1:?pass the ziran command}
"$ziran" run --target=c --root "$repo/tests" "$repo/tests/float_integer_casts_cases.zi" -- "$ziran"
