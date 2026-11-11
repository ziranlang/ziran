#!/bin/sh
# check-cost: 290
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${1:?pass ziran launcher}
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
shift
exec "$ziran" run --target=c --root "$repo/tests" "$repo/tests/packages_cases.zi" -- "$ziran" "$@"
