#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
exec "$ziran" run --target=c --root "$repo/tests" \
    "$repo/tests/compound_order_cases.zi" -- "$ziran"
