#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${1:-"$repo/build/bin/ziran"}
work="$repo/build/tests/transport-view"
mkdir -p "$work/tmp"
export TMPDIR="$work/tmp" ZIRAN_DESKTOP_RECOVERY=0 HARMONY_DESKTOP_RECOVERY=0 YUE_DESKTOP_RECOVERY=0
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
exec "$ziran" run --target=c --root "$repo/tests" "$repo/tests/transport_view_cases.zi" -- "$ziran" "$work"
