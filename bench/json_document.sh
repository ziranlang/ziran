#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${1:-$repo/build/bin/ziran}
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
cd "$repo"
output="$repo/build/benchmarks/json-document"
"$ziran" build --target=c --root bench --entry json_document:main --exe -o "$output" bench/json_document.zi
nice -n 19 "$output/json_document"
