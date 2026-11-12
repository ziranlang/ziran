#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
output="$repo/build/executable-bundle-test"
"$ziran" build --target=c --exe --entry executable_bundle_cases:main \
    --root "$repo/tests" --module-path "$repo/cmd" -o "$output" \
    "$repo/tests/executable_bundle_cases.zi"
exec "$output/executable_bundle_cases"
