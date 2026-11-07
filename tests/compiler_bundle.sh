#!/bin/sh
set -eu
ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
module=compiler_bundle_test
. "$repo/tests/lib/compiler_module.sh"

${CC:-cc} ${VM_CFLAGS:-} -std=c11 -D_GNU_SOURCE -I"$repo/include" \
    -I"$(dirname "${ZIRAN_LIB:-$repo/build/libziran.a}")/compiler-bundle" -I"$repo/cmd/zir" \
    "$repo/tests/compiler_bundle_test.c" "${ZIRAN_LIB:-$repo/build/libziran.a}" \
    -lm -pthread -o "$work/source-boundary"
"$work/source-boundary"
