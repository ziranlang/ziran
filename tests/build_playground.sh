#!/bin/sh
# Native fixture processes exercise build orchestration without Emscripten.
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
exec "$ziran" run --target=c --root "$repo/tests" "$repo/tests/build_playground_cases.zi" -- "$ziran"
