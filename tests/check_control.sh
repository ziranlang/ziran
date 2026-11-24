#!/bin/sh
# The documented build modes: --quickest skips only the optional compile-time
# verification, so it writes exactly the default's output; --thorough adds the
# runtime checks of --checks on top of every compile-time verification. The
# allocated program, the lowered IR and the .zx loader keep their checks in
# both modes.
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tools=$(dirname "$ziran")
mkdir -p "$repo/build/tests"
work=$(mktemp -d "$repo/build/tests/check-control-XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS

fail() { echo "check_control: $*" >&2; exit 1; }

cat > "$work/app.zi" <<'ZI'
Answer :: () -> s32 {
    total: s64 = 0
    for i: 0..10 { total += i }
    return cast(s32)total
}
ZI

"$ziran" bundle --root "$work" --entry app:Answer -o "$work/default.zx" "$work/app.zi" || fail "the default bundle failed"
"$ziran" bundle --quickest --root "$work" --entry app:Answer -o "$work/quickest.zx" "$work/app.zi" || fail "--quickest bundle failed"
"$ziran" bundle --thorough --root "$work" --entry app:Answer -o "$work/thorough.zx" "$work/app.zi" || fail "--thorough bundle failed"
cmp "$work/default.zx" "$work/quickest.zx" || fail "--quickest wrote different ZX"
if cmp -s "$work/default.zx" "$work/thorough.zx"; then fail "--thorough did not record runtime checks"; fi
[ "$("$ziran" run "$work/quickest.zx")" = 55 ] || fail "the quickest bundle runs wrong"
[ "$("$ziran" run "$work/thorough.zx")" = 55 ] || fail "the thorough bundle runs wrong"

# The native encoder's own re-check of the allocated program is optional too,
# and skipping it does not change the image or its behavior.
"$tools/zx2native" --target x86_64 --platform linux --emit elf -o "$work/native-default" "$work/default.zx" || fail "the default native image failed"
"$tools/zx2native" --quickest --target x86_64 --platform linux --emit elf -o "$work/native-quickest" "$work/default.zx" || fail "the quickest native image failed"
"$tools/zx2native" --thorough --target x86_64 --platform linux --emit elf -o "$work/native-thorough" "$work/default.zx" || fail "the thorough native image failed"
cmp "$work/native-default" "$work/native-quickest" || fail "--quickest wrote a different native image"
cmp "$work/native-default" "$work/native-thorough" || fail "--thorough changed the native image"
status=0
"$work/native-quickest" || status=$?
[ "$status" = 55 ] || fail "the quickest native program exited with $status"

# The generated C route accepts both modes; the code output is the same.
"$ziran" build --target=c --root "$work" -o "$work/c-default" "$work/app.zi" || fail "the default C build failed"
"$ziran" build --target=c --quickest --root "$work" -o "$work/c-quickest" "$work/app.zi" || fail "the quickest C build failed"
"$ziran" build --target=c --thorough --root "$work" -o "$work/c-thorough" "$work/app.zi" || fail "the thorough C build failed"
diff -r "$work/c-default" "$work/c-quickest" >/dev/null || fail "--quickest wrote different C"
diff -r "$work/c-default" "$work/c-thorough" >/dev/null || fail "--thorough wrote different C"
