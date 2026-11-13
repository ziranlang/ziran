#!/bin/sh
# Revision-28 executable ZX gives the semantic route's output on the
# verified-instruction interpreter; native ELF matches or names its gap.
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
exec "$ziran" run --target=c --root "$repo/tests" "$repo/tests/zx_lowered_parity_cases.zi" -- "$ziran"
