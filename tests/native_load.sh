#!/bin/sh
# check-cost: 2
set -eu
ziran=${1:?pass the ziran command}
native_load=${2:?pass the native load test}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
zi2zx=$(dirname -- "$ziran")/zi2zx
mkdir -p "$repo/build/tests"
work=$(mktemp -d "$repo/build/tests/native-load-XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER
export HARMONY_DESKTOP_RECOVERY=0

"$zi2zx" --root "$repo/tests/spec" --entry native_bundle:Answer \
    -o "$work/native.zx" "$repo/tests/spec/native_bundle.zi"
"$native_load" "$work/native.zx"
