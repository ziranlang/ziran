#!/bin/sh
set -eu
ziran=${1:?pass the native Ziran launcher}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$repo"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
entry=package_checkout_concurrency_probe
output=$work/c
# PACKAGE_MODULES lets a caller resolve package_lock from another tree, such
# as the previous revision, to show the test catches the race.
"$ziran" build --root tests ${PACKAGE_MODULES:+--module-path "$PACKAGE_MODULES"} \
    --module-path cmd --module-path std --target=c --no-main --entry "$entry:main" \
    -o "$output" "tests/$entry.zi"
"${CC:-cc}" -O2 -std=c11 -I"$repo/include" "$output"/*.c -o "$output/probe" -lcrypto -lm
python3 tests/package_checkout_concurrency.py "$output/probe"
