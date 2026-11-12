#!/bin/sh
# check-cost: 2
set -eu
ziran=${1:?pass the ziran command}
zi2asm=${2:-$(dirname -- "$ziran")/zi2asm}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
case "$zi2asm" in /*) ;; *) zi2asm="$repo/$zi2asm" ;; esac
test -x "$zi2asm"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS GDK_DISPLAY
exec env ZIRAN_ZI2ASM="$zi2asm" "$ziran" run --target=c --root "$repo/tests" \
    "$repo/tests/assembly_local_addresses_cases.zi" -- "$ziran"
