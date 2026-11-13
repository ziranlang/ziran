#!/bin/sh
# std file_tree_linux built for wasm32 with Emscripten and run under a Node-compatible JS runtime.
# size_t and ssize_t are 32 bits there, so a libc binding declared with u64 or
# s64 for getcwd, readlink or write links with a function signature mismatch
# and traps when called. Skipped without emcc (EMCC names another one).
set -eu
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER
ziran=${1:?pass ziran}
# Select a Node-compatible runtime, such as Bun, without a PATH shim.
js_runtime=${JS_RUNTIME:-node}
emcc=${EMCC:-emcc}
if ! command -v "$emcc" > /dev/null 2>&1 || ! command -v "$js_runtime" > /dev/null 2>&1; then
    echo "std file_tree_linux on wasm32: skipped (needs emcc and JS_RUNTIME=$js_runtime)"
    exit 0
fi
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$repo/build/test"
work=$(mktemp -d "$repo/build/test/std-file-tree-wasm32.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/tree_web.zi" <<'ZI'
#import "file_tree_linux"

#program_export
main :: () -> s32 {
    cwd := WorkingDirectory()
    print("cwd %\n", cwd)
    if !ReplaceFile("/work/source/a.txt", "replaced text") { return 1 }
    if !CopyTree("/work/source", "/work/copy") { return 2 }
    if PathStatus("/work/copy/a.txt").kind != .File { return 3 }
    if PathStatus("/work/copy/link", false).kind != .Link { return 4 }
    if PathStatus("/work/copy/link").kind != .File { return 5 }
    print("tree copied\n")
    return 0
}
ZI
cat > "$work/fixture.js" <<'JS'
Module.preRun = [function () {
    FS.mkdir('/work');
    FS.mkdir('/work/source');
    FS.writeFile('/work/source/a.txt', 'original');
    FS.symlink('a.txt', '/work/source/link');
    FS.chdir('/work');
}];
Module.postRun = [function () {
    process.stdout.write('copy holds ' + FS.readFile('/work/copy/a.txt', {encoding: 'utf8'}) +
        ', link reads ' + FS.readlink('/work/copy/link') + '\n');
}];
JS
"$ziran" build --target=c --target-os web --target-cpu wasm32 --root "$work" \
    --entry tree_web:main -o "$work/c" "$work/tree_web.zi"
# ReplaceFile writes through a 64 KiB stack buffer, the whole default stack.
"$emcc" -I"$repo/include" "$work/c"/*.c --pre-js "$work/fixture.js" \
    -sENVIRONMENT=node -sEXIT_RUNTIME=1 -sSTACK_SIZE=1048576 -o "$work/tree.js"
"$js_runtime" "$work/tree.js" > "$work/out"
cat > "$work/expected" <<'OUT'
cwd /work
tree copied
copy holds replaced text, link reads a.txt
OUT
if ! cmp -s "$work/expected" "$work/out"; then
    diff -u "$work/expected" "$work/out" >&2 || true
    exit 1
fi
echo 'std file_tree_linux on wasm32: passed'
