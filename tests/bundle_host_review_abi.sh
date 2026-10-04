#!/bin/sh
# Check the generated standard module against the public C ABI, including
# actual calls through its wrappers on both pointer widths when wasm exists.
set -eu

ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

"$ziran" build --target=c --no-main --root "$repo/std" \
    -o "$work/c" "$repo/std/bundle_host.zi"
"${CC:-cc}" -std=c11 -I"$repo/include" -I"$work/c" \
    "$work/c/bundle_host.c" "$repo/tests/bundle_host_review_abi.c" \
    -o "$work/native"
"$work/native"

abi_emcc=${EMCC:-"$HOME/emsdk/upstream/emscripten/emcc"}
abi_node=${NODE:-}
if [ -z "$abi_node" ]; then
    abi_node=$(command -v node || true)
fi
if [ -z "$abi_node" ]; then
    for candidate in "$HOME"/emsdk/node/*/bin/node; do
        if [ -x "$candidate" ]; then
            abi_node=$candidate
            break
        fi
    done
fi
if [ ! -x "$abi_emcc" ] || [ -z "$abi_node" ]; then
    echo 'Bundle host wasm ABI probe skipped: emcc or node is unavailable'
    exit 0
fi

"$abi_emcc" -std=c11 -I"$repo/include" -I"$work/c" \
    "$work/c/bundle_host.c" "$repo/tests/bundle_host_review_abi.c" \
    -sENVIRONMENT=node -sEXIT_RUNTIME=1 -o "$work/abi.js"
"$abi_node" "$work/abi.js"
