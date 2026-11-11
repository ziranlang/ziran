#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${1:?pass the ziran command}
exec "$ziran" run --target=c --root "$repo/tests" "$repo/tests/compiler_inventory_cases.zi" -- "$ziran" features
