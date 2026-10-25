#!/bin/sh
set -eu
# The wasm execution tier end to end, in its production shape: libziran
# itself compiled to WebAssembly, a bundle lowered by the tier inside it,
# and the platform provider implemented in JavaScript. Skipped unless
# emcc and node are both installed; `make check` covers the tier's seam
# natively through zx-wasm-engine-test.
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build=$(CDPATH= cd -- "$(dirname -- "$ziran")/.." && pwd)
work=$build/tests/zx-wasm-web
mkdir -p "$work"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS

emcc=${ZIRAN_WASM_EMCC:-}
node=${ZIRAN_WASM_NODE:-node}
if command -v "$node" >/dev/null 2>&1; then :; else
    echo "zx-wasm-web: node is unavailable; skipping"
    exit 0
fi
if command -v "$emcc" >/dev/null 2>&1; then :;
elif [ -x "$HOME/emsdk/upstream/emscripten/emcc" ]; then
    emcc=$HOME/emsdk/upstream/emscripten/emcc
else
    echo "zx-wasm-web: emcc is unavailable; skipping"
    exit 0
fi
emar=$(dirname -- "$emcc")/emar

source=$repo/tests/spec/zx_wasm_test.zi
entry=zx_wasm_test:Answer
expected=9007

"$ziran" bundle --root "$repo/tests/spec" --entry "$entry" \
    -o "$work/zx_wasm_test.zx" "$source"
test "$("$ziran" run "$work/zx_wasm_test.zx")" = "$expected"

# The VM-only library is what web hosts embed; the private headers hide
# the compiler's internal symbols from the wasm link.
make -s -C "$repo" -j4 BOOTSTRAP=1 BUILD_DIR="$work/lib" \
    CC="$emcc" AR="$emar" OBJCOPY=true CFLAGS='-O2' \
    FRAMEFLAGS='-Wframe-larger-than=16384' \
    WASM_PRIVATE_HEADERS="$work/private" \
    "$work/lib/libziran-vm.a"

"$emcc" -O2 -std=gnu99 -I"$repo/include" -I"$repo/cmd/zir" \
    "$repo/tests/zx_wasm_web_host.c" "$work/lib/libziran-vm.a" \
    -o "$work/host.js" -s MODULARIZE=1 -s EXPORT_NAME=ZxWasmWebHost \
    -s ENVIRONMENT=node -s NODERAWFS=1 -s ALLOW_MEMORY_GROWTH=1 \
    -s EXIT_RUNTIME=1

run_host() {
    ZIRAN_NATIVE_CACHE=0 "$node" -e "
const host = require('$work/host.js')({arguments: ['$1', '$work/zx_wasm_test.zx', '$expected']});
" 2>&1 | grep -E '^[0-9-]+$' | tail -1
}

# The constructor-installed provider is the production path: no host code
# touches the tier, and the module cache reuses compilation on reopen.
auto=$(run_host auto)
test "$auto" = "$expected" || {
    echo "zx-wasm-web: auto tier result '$auto' != $expected"
    exit 1
}

# An explicitly installed provider still works.
manual=$(run_host manual)
test "$manual" = "$expected" || {
    echo "zx-wasm-web: manual tier result '$manual' != $expected"
    exit 1
}

# ZIRAN_WASM_TIER=0 leaves the provider out and the VM runs the bundle.
plain=$(ZIRAN_WASM_TIER=0 ZIRAN_NATIVE_CACHE=0 "$node" -e "
const host = require('$work/host.js')({arguments: ['auto', '$work/zx_wasm_test.zx', '$expected']});
" 2>&1 | grep -E '^[0-9-]+$' | tail -1)
if test "$plain" = "$expected"; then
    echo "zx-wasm-web: ZIRAN_WASM_TIER=0 still installed the provider" >&2
    exit 1
fi

echo 'The wasm execution tier installs itself in the web build and runs bundles without host changes'
