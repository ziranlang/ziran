#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
compiler=${ZIRAN:-"$repo/build/bin/ziran"}
exec "$compiler" run --target=c --root "$repo/scripts" --module-path "$repo/tests" \
    "$repo/scripts/test_native_package.zi" -- "$@"
