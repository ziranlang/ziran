#!/bin/sh
# check-cost: 4
set -eu
ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
"$ziran" run --target=c --root "$repo/tests" --module-path "$repo/cmd" \
    "$repo/tests/machine_ir_cases.zi"
exec "$ziran" run --target=c --root "$repo/tests" --module-path "$repo/cmd" \
    "$repo/tests/x86_encoder_cases.zi"
