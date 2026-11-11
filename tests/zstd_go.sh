#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
ziran=${1:-"$root/build/bin/ziran"}
"$ziran" run --target=c --root "$root/tests" "$root/tests/zstd_go_cases.zi" -- "$ziran"
