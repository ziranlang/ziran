#!/bin/sh
# check-cost: 2
set -eu
ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
exec "$ziran" run --target=c --root "$repo/tests" --module-path "$repo/cmd" \
    "$repo/tests/machine_ir_cases.zi"
