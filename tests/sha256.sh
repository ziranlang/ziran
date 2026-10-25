#!/bin/sh
# SHA-256 keys the native code cache and is computed for every bundle a host
# opens. Both the CPU's SHA-256 instructions (where it has them) and the
# portable rounds must give the FIPS 180-4 digests, and the same digest for
# a large input.
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
"${CC:-cc}" ${VM_CFLAGS:-} -O2 -D_GNU_SOURCE -std=c11 -I"$repo/cmd/zir" \
    "$repo/tests/sha256_test.c" "$repo/cmd/zir/zir_sha256.c" -o "$work/sha256"
"$work/sha256"
ZIRAN_SHA256_PORTABLE=1 "$work/sha256"
fast=$("$work/sha256" 3000001 | cut -d' ' -f1)
portable=$(ZIRAN_SHA256_PORTABLE=1 "$work/sha256" 3000001 | cut -d' ' -f1)
test "$fast" = "$portable"
echo "sha256: hardware and portable digests agree"
