#!/bin/sh
set -eu
ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
module=compiler_proof_kernel_test
. "$repo/tests/lib/compiler_module.sh"

# The C boundary: exhaustive interpretation of accepted claims and the trace
# of the C kernel this module replaced.
${CC:-cc} ${VM_CFLAGS:-} -std=c11 -I"$repo/include" \
    -I"$(dirname "${ZIRAN_LIB:-$repo/build/libziran.a}")/compiler-proof-kernel" \
    "$repo/tests/compiler_proof_kernel_test.c" "${ZIRAN_LIB:-$repo/build/libziran.a}" \
    -pthread -lm -o "$work/source-boundary"
"$work/source-boundary"
