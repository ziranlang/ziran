#!/bin/sh
set -eu
ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
module=compiler_source_test
. "$repo/tests/lib/compiler_module.sh"

${CC:-cc} ${VM_CFLAGS:-} -std=c11 -D_GNU_SOURCE -I"$repo/include" -I"$(dirname "${ZIRAN_LIB:-$repo/build/libziran.a}")/compiler-source" \
    "$repo/tests/compiler_source_test.c" "${ZIRAN_LIB:-$repo/build/libziran.a}" \
    -o "$work/source-boundary"
"$work/source-boundary"
