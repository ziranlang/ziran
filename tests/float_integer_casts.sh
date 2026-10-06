#!/bin/sh
set -eu
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
python3 "$repo/tests/float_integer_casts.py" "${1:?pass the ziran command}"
