#!/bin/sh
set -eu
ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
module=compiler_expression_test
. "$repo/tests/lib/compiler_module.sh"

${CC:-cc} ${VM_CFLAGS:-} -std=c11 -D_GNU_SOURCE -I"$repo/include" -I"$(dirname "${ZIRAN_LIB:-$repo/build/libziran.a}")/compiler-expression" \
    -I"$(dirname "${ZIRAN_LIB:-$repo/build/libziran.a}")/compiler-text" \
    -I"$(dirname "${ZIRAN_LIB:-$repo/build/libziran.a}")/compiler-source" \
    -I"$(dirname "${ZIRAN_LIB:-$repo/build/libziran.a}")/compiler-scan" \
    -I"$repo/cmd/zir" \
    "$repo/tests/compiler_expression_test.c" "${ZIRAN_LIB:-$repo/build/libziran.a}" \
    -o "$work/expression-boundary"
"$work/expression-boundary"
