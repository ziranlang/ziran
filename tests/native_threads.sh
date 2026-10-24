#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS LD_PRELOAD
build_program() {
    input_root=$1; output=$2; input=$3; shift 3
    "$ziran" build --target=c "$@" --root "$input_root" --entry capability:main --exe -o "$output" "$input"
}
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
build_program "$work" "$work/native-c" "$work/capability.zi"
test "$("$work/native-c/capability")" = 'POSIX threads'
"$ziran" ir --target=c --root "$work" -o "$work/native-ir-c" "$work/capability.zi"
build_program "$work/native-ir-c" "$work/native-saved-c" "$work/native-ir-c/capability.zir"
test "$("$work/native-saved-c/capability")" = 'POSIX threads'
for platform in _WIN32 PLATFORM_WEB; do
    expected=portable
    if test "$platform" = _WIN32; then expected='POSIX threads'; fi
    build_program "$work" "$work/c-$platform" "$work/capability.zi" --define "$platform"
    test "$("$work/c-$platform/capability")" = "$expected"
    "$ziran" ir --target=c --define "$platform" --root "$work" -o "$work/ir-c-$platform" "$work/capability.zi"
    build_program "$work/ir-c-$platform" "$work/saved-c-$platform" "$work/ir-c-$platform/capability.zir"
    test "$("$work/saved-c-$platform/capability")" = "$expected"
done
"$ziran" ir --root "$work" -o "$work/ir" "$work/capability.zi"
"$ziran" build --target=c --root "$work/ir" --entry capability:main --exe -o "$work/saved" "$work/ir/capability.zir"
test "$("$work/saved/capability")" = 'portable'

# Automatic native capability insertion must never overrun the define table.
set --
index=0
while test "$index" -lt 64; do set -- "$@" --define FULL; index=$((index + 1)); done
for command in build ir; do
    if "$ziran" "$command" --target=c "$@" --root "$work" -o "$work/full-$command-c" "$work/capability.zi" 2> "$work/full.err"; then
        echo 'Full define table unexpectedly accepted native capability insertion' >&2
        exit 1
    fi
    grep -Fq 'no define slot left for native thread capability' "$work/full.err"
done
echo 'Native C source and explicit target IR agree; portable selectors and bounded defines pass'
