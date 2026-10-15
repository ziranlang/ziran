#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
if ! command -v cargo >/dev/null 2>&1; then
    echo 'Rust assembly checks skipped: cargo is unavailable'
    exit 0
fi
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
python3 "$repo/tests/rust_assembly_test.py" "$ziran" "$work"
