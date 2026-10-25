#!/bin/sh
# The native ZX engine caches each compiled bundle: a second run opens the
# bundle without checking it again and links the cached object, an unusable
# object is rebuilt (after checking) rather than run, new asset bytes keep
# the entry, and a changed program gets its own. The program passes records around the way that
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
    "$ziran" bundle --root "$work" --asset-dir notes="$work/notes" --entry answer:main \
        -o "$work/answer.zx" "$work/answer.zi"
}

run() {
    ZIRAN_NATIVE_CACHE="$work/cache" ZIRAN_NATIVE_TIMING=1 \
        "$ziran" run "$work/answer.zx" > "$work/out" 2> "$work/err"
    cat "$work/out"
}

mkdir "$work/notes"
printf one > "$work/notes/note.txt"
write_program 0
test "$(run)" = 42
if ! grep -q '^ziran: native' "$work/err"; then
    echo "portable VM build: no native code cache"
    exit 0
fi
grep -q 'lowering' "$work/err"
if grep -q 'unchecked' "$work/err"; then exit 1; fi
test "$(ls "$work/cache" | wc -l)" -eq 1

test "$(run)" = 42
grep -q 'opened on cached code, unchecked' "$work/err"
grep -q 'cached, linking' "$work/err"

for object in "$work/cache"/*.o; do printf 'not an object' > "$object"; done
test "$(run)" = 42
grep -q 'opened on cached code, unchecked' "$work/err"
grep -q 'lowering' "$work/err"
test "$(run)" = 42
grep -q 'cached, linking' "$work/err"

printf two > "$work/notes/note.txt"
write_program 0
test "$(run)" = 42
grep -q 'opened on cached code, unchecked' "$work/err"
grep -q 'cached, linking' "$work/err"
test "$(ls "$work/cache" | wc -l)" -eq 1

write_program 1
test "$(run)" = 43
grep -q 'lowering' "$work/err"
test "$(ls "$work/cache" | wc -l)" -eq 2
echo "native code cache: compiled, reused unchecked, rebuilt when unusable, keyed by program"
