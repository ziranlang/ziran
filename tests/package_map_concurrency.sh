#!/bin/sh
set -eu
ziran=${1:?pass the native Ziran launcher}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$repo"
mkdir -p "$repo/build/tests"
work=$(mktemp -d "$repo/build/tests/package-map-probe.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
entry=package_map_concurrency_probe
"$ziran" ir --root tests --module-path cmd --module-path std -o "$work/ir" "tests/$entry.zi"
for form in source saved; do
    if test "$form" = source; then
        input=$repo/tests/$entry.zi
        set -- --root tests --module-path cmd --module-path std
    else
        input=$work/ir/$entry.zir
        set -- --root "$work/ir" --module-path "$work/ir"
    fi
    output=$work/$form-c
    "$ziran" build "$@" --target=c --no-main --entry "$entry:main" -o "$output" "$input"
    "${CC:-cc}" -O2 -std=c11 -I"$repo/include" "$output"/*.c -o "$output/probe" -lcrypto -lm
    "$ziran" run --target=c --root "$repo/tests" "$repo/tests/package_map_concurrency_cases.zi" -- "$output/probe"
done
