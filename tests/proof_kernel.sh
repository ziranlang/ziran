#!/bin/sh
set -eu
unset DISPLAY WAYLAND_DISPLAY
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
${CC:-cc} -std=c11 -I"$repo/cmd/zir" "$repo/tests/proof_kernel_test.c" \
    "$repo/cmd/zir/zir_proof_kernel.c" -o "$work/kernel"
"$work/kernel"
