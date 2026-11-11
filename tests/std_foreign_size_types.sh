#!/bin/sh
# std's C bindings give size_t, ssize_t and nfds_t values pointer-sized Ziran
# types (usize, isize). A fixed-width u64 or s64 there is the wrong C type on
# wasm32 and other 32-bit targets: wasm-ld reports a function signature
# mismatch and the call traps. Each listed C function names the positions of
# its pointer-sized parameters ("r" is its result, "*" a pointer to one).
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${1:?pass the ziran command}
exec "$ziran" run --target=c --root "$repo/tests" --module-path "$repo/cmd" \
    "$repo/tests/std_foreign_size_types_cases.zi" -- "$ziran"
