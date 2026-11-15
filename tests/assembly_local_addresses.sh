#!/bin/sh
# check-cost: 2
set -eu
ziran=${1:?pass the ziran command}
zi2zx=${2:-$(dirname -- "$ziran")/zi2zx}
zx2native=${3:-$(dirname -- "$ziran")/zx2native}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
case "$zi2zx" in /*) ;; *) zi2zx="$repo/$zi2zx" ;; esac
case "$zx2native" in /*) ;; *) zx2native="$repo/$zx2native" ;; esac
test -x "$zi2zx"
test -x "$zx2native"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS GDK_DISPLAY
exec env ZIRAN_ZI2ZX="$zi2zx" ZIRAN_ZX2NATIVE="$zx2native" "$ziran" run --target=c --root "$repo/tests" \
    "$repo/tests/assembly_local_addresses_cases.zi" -- "$ziran"
