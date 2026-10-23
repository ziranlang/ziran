#!/bin/sh
set -eu
ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
module=compiler_text_test
. "$repo/tests/lib/compiler_module.sh"

${CC:-cc} ${VM_CFLAGS:-} -std=c11 -D_GNU_SOURCE -I"$repo/include" -I"$repo/cmd/zir" \
    "$repo/tests/compiler_text_test.c" "${ZIRAN_LIB:-$repo/build/libziran.a}" \
    -o "$work/text-boundary"
"$work/text-boundary"
