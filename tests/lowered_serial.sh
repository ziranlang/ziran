#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
driver="$repo/build/lowered-serial-test/driver"
"$ziran" build --target=c --exe --entry lowered_serial_cases:main \
    --define LOWERED_SERIAL_NATIVE_TEST \
    --root "$repo/tests" --module-path "$repo/cmd" -o "$driver" \
    "$repo/tests/lowered_serial_cases.zi"
export ZIRAN_SCRIPT_FILE="$repo/tests/lowered_serial_cases.zi"
exec "$driver/lowered_serial_cases" "$ziran"
