#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
cat > "$work/capability.zi" <<'ZI'
#program_export
main :: () -> s32 {
    #if #defined(POSIX_THREADS) {
        print("POSIX threads\n")
    } else {
        print("portable\n")
    }
    return 0
}
ZI
"$ziran" build --target=c --root "$work" --entry capability:main --exe -o "$work/native" "$work/capability.zi"
test "$("$work/native/capability")" = 'POSIX threads'
for platform in _WIN32 PLAN9 PLATFORM_WEB; do
    "$ziran" build --target=c --define "$platform" --root "$work" --entry capability:main --exe -o "$work/$platform" "$work/capability.zi"
    test "$("$work/$platform/capability")" = 'portable'
done
"$ziran" ir --root "$work" -o "$work/ir" "$work/capability.zi"
"$ziran" build --target=c --root "$work/ir" --entry capability:main --exe -o "$work/saved" "$work/ir/capability.zir"
test "$("$work/saved/capability")" = 'portable'
