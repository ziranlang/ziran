#!/bin/sh
set -eu
# Code linked into a host ahead of time, in its production shape: libziran
# compiled to WebAssembly, a bundle's program written as C by
# `zi2zx native-c`, compiled with emcc under its own symbol prefix
# (scripts/linked-zx.py) and linked into the host, which runs those exact
# bytes as that code and other bundles on the portable VM. Skipped unless
# emcc and node are installed and zi2zx has the native engine.
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build=$(CDPATH= cd -- "$(dirname -- "$ziran")/.." && pwd)
zi2zx=$build/bin/zi2zx
work=$build/tests/zx-linked-web
rm -rf "$work"
mkdir -p "$work"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS

emcc=${ZIRAN_WASM_EMCC:-}
node=${ZIRAN_WASM_NODE:-node}
if command -v "$node" >/dev/null 2>&1; then :; else
    echo "zx-linked-web: node is unavailable; skipping"
    exit 0
fi
if command -v "$emcc" >/dev/null 2>&1; then :;
elif [ -x "$HOME/emsdk/upstream/emscripten/emcc" ]; then
    emcc=$HOME/emsdk/upstream/emscripten/emcc
else
    echo "zx-linked-web: emcc is unavailable; skipping"
    exit 0
fi
if "$zi2zx" 2>&1 | grep -q native-c; then :; else
    echo "zx-linked-web: zi2zx has no native engine here; skipping"
    exit 0
fi
emar=$(dirname -- "$emcc")/emar
emnm=$(dirname -- "$emcc")/emnm

# Globals with a computed initializer, state that grows each run, and a
# host capability.
cat > "$work/linked_api.zi" <<'ZI'
host_api :: #system_library "host_api";
AddHost :: (value: s64) -> s64 #foreign host_api;
#program_export
Add :: (value: s64) -> s64 {
    return AddHost(value)
}
ZI
cat > "$work/linked.zi" <<'ZI'
#import "linked_api"
Seed :: () -> s64 { return 3 + 4 }
base: s64 = Seed();
runs: s64 = 40;
table: [4]s64;
Main :: () -> s64 {
    runs += 1
    table[runs % 4] += runs
    return Add(runs * 100 + base + table[1])
}
ZI
"$ziran" bundle --root "$work" --entry linked:Main -o "$work/linked.zx" "$work/linked.zi"
"$ziran" bundle --root "$repo/tests/spec" --entry zx_wasm_test:Answer \
    -o "$work/plain.zx" "$repo/tests/spec/zx_wasm_test.zi"

"$zi2zx" native-c -o "$work/programs" "$work/linked.zx" > /dev/null
python3 "$repo/scripts/linked-zx.py" "$work/programs" --output "$work/objects" \
    --cc "$emcc" --nm "$emnm" --cflags=-O2 > "$work/objects.txt" 2> /dev/null

make -C "$repo" -j2 BOOTSTRAP=1 BUILD_DIR="$work/lib" \
    CC="$emcc" AR="$emar" OBJCOPY=true CFLAGS='-O2' \
    FRAMEFLAGS='-Wframe-larger-than=16384' \
    WASM_PRIVATE_HEADERS="$work/private" \
    "$work/lib/libziran.a" > "$work/lib.log" 2>&1 || { tail -20 "$work/lib.log"; exit 1; }

# shellcheck disable=SC2046
"$emcc" -O2 -std=gnu99 -I"$repo/include" "$repo/tests/zx_linked_web_host.c" \
    $(cat "$work/objects.txt") "$work/lib/libziran.a" -o "$work/host.js" \
    -s ENVIRONMENT=node -s NODERAWFS=1 -s ALLOW_MEMORY_GROWTH=1 -s EXIT_RUNTIME=1

"$node" "$work/host.js" "$work/linked.zx" "$work/plain.zx" > "$work/out.txt" 2> "$work/err.txt" || {
    cat "$work/err.txt"
    exit 1
}
# Linked: 5148 then 5248 on one instance; beside it the VM from fresh
# globals, 5148; reopened alone, linked code from fresh globals, 5148; the
# other bundle on the VM, 9091.
test "$(cat "$work/out.txt")" = "5148 5248 5148 5148 9091" || {
    echo "zx-linked-web: results '$(cat "$work/out.txt")'"
    cat "$work/err.txt"
    exit 1
}
grep -q 'linked.zx: opened on linked code, unchecked' "$work/err.txt"
test "$(grep -c 'ziran: native Main: linked code' "$work/err.txt")" = 2 || {
    echo "zx-linked-web: expected two linked opens"
    cat "$work/err.txt"
    exit 1
}
echo 'Linked code runs its exact bundle in the WebAssembly host, one instance at a time from fresh globals'
