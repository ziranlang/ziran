#!/bin/sh
# The native ZX engine caches each compiled bundle: a second run links the
# cached object, an unusable object is rebuilt rather than run, and a changed
# bundle gets its own entry. The program passes records around the way that
# once broke TinyCC's ARM calls (a structure split across r3 and the stack,
# then an 8-byte-aligned argument). Builds without the engine skip the cache.
set -eu
ziran=${1:?pass the ziran command}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS

write_program() {
    cat > "$work/answer.zi" <<ZI
buffer: [32]u8;
Joined :: (first: string, second: string, output: []u8) -> string {
    length: s32 = 0
    for byte: first { output[length] = byte; length += 1 }
    for byte: second { output[length] = byte; length += 1 }
    return TextView(output[0:length])
}
#program_export
main :: () -> s32 {
    text := Joined("Ki", "ryon", buffer[:])
    return cast(s32)text.count * 7 + $1
}
ZI
    "$ziran" bundle --root "$work" --entry answer:main -o "$work/answer.zx" "$work/answer.zi"
}

run() {
    ZIRAN_NATIVE_CACHE="$work/cache" ZIRAN_NATIVE_TIMING=1 \
        "$ziran" run "$work/answer.zx" > "$work/out" 2> "$work/err"
    cat "$work/out"
}

write_program 0
test "$(run)" = 42
if ! grep -q '^ziran: native' "$work/err"; then
    echo "portable VM build: no native code cache"
    exit 0
fi
grep -q 'lowering' "$work/err"
test "$(ls "$work/cache" | wc -l)" -eq 1

test "$(run)" = 42
grep -q 'cached, linking' "$work/err"

for object in "$work/cache"/*.o; do printf 'not an object' > "$object"; done
test "$(run)" = 42
grep -q 'lowering' "$work/err"
test "$(run)" = 42
grep -q 'cached, linking' "$work/err"

write_program 1
test "$(run)" = 43
grep -q 'lowering' "$work/err"
test "$(ls "$work/cache" | wc -l)" -eq 2
echo "native code cache: compiled, reused, rebuilt when unusable, keyed by bundle"
