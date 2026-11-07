#!/bin/sh
# `build --target=c --exe` links with the configured compiler: a language
# forced through CC is reset for the object files, and Emscripten's emcc,
# which reads -x none as a source language and writes NAME.wasm beside its
# NAME launcher, links a program that runs under Node.
set -eu
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER
ziran=${1:?pass ziran}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/hello.zi" <<'ZI'
#program_export
main :: () -> s32 {
    print("hello %\n", 42)
    return 0
}
ZI
CC="${CC:-cc} -x c" "$ziran" build --target=c --exe --entry hello:main \
    --root "$work" -o "$work/host" "$work/hello.zi"
test "$("$work/host/hello")" = 'hello 42'
# The link leaves nothing of its own behind.
if ls -A "$work/host" | grep -q '^\.hello\.'; then
    ls -A "$work/host" >&2
    exit 1
fi

emcc=${EMCC:-emcc}
if ! command -v "$emcc" > /dev/null 2>&1 || ! command -v node > /dev/null 2>&1; then
    echo 'C executable link: host passed; emcc skipped (needs emcc and node)'
    exit 0
fi
CC=$emcc "$ziran" build --target=c --define PLATFORM_WEB --exe --entry hello:main \
    --root "$work" -o "$work/web" "$work/hello.zi"
test -f "$work/web/hello.wasm"
test "$(node "$work/web/hello")" = 'hello 42'
echo 'C executable link with the host compiler and emcc: passed'
