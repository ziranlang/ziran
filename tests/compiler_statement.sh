#!/bin/sh
set -eu
ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
module=compiler_statement_test
. "$repo/tests/lib/compiler_module.sh"

${CC:-cc} ${VM_CFLAGS:-} -std=c11 -D_GNU_SOURCE -I"$repo/include" -I"$(dirname "${ZIRAN_LIB:-$repo/build/libziran.a}")/compiler-statement" \
    -I"$(dirname "${ZIRAN_LIB:-$repo/build/libziran.a}")/compiler-text" \
    -I"$(dirname "${ZIRAN_LIB:-$repo/build/libziran.a}")/compiler-source" \
    -I"$(dirname "${ZIRAN_LIB:-$repo/build/libziran.a}")/compiler-scan" \
    -I"$repo/cmd/zir" \
    "$repo/tests/compiler_statement_test.c" "${ZIRAN_LIB:-$repo/build/libziran.a}" \
    -lm -o "$work/statement-boundary"
"$work/statement-boundary"
