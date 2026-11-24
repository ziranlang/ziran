#!/bin/sh
# SHA-256 keys the native code cache and is computed for every bundle a host
# opens. Both the CPU's SHA-256 instructions (where it has them) and the
# portable rounds must give the FIPS 180-4 digests, and the same digest for
# a large input.
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
set --
for module in ir context_types parse_types parse_s14_types check_types text statement declaration source scan sha256; do
    set -- "$@" -I"$repo/bootstrap/compiler_$module"
done
"${CC:-cc}" ${VM_CFLAGS:-} -O2 -D_GNU_SOURCE -std=c11 -I"$repo/include" -I"$repo/bootstrap/runtime" "$@" \
    "$repo/tests/sha256_test.c" "$repo/bootstrap/compiler_sha256/compiler_sha256.c" \
    "$repo/bootstrap/runtime/zir_sha256_host.c" "$repo/bootstrap/runtime/zir_atomics.c" -o "$work/sha256"
"$work/sha256"
ZIRAN_SHA256_PORTABLE=1 "$work/sha256"
fast=$("$work/sha256" 3000001 | cut -d' ' -f1)
portable=$(ZIRAN_SHA256_PORTABLE=1 "$work/sha256" 3000001 | cut -d' ' -f1)
test "$fast" = "$portable"
echo "sha256: hardware and portable digests agree"
