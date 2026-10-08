#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS LD_PRELOAD
build_program() {
    backend=$1; input_root=$2; output=$3; input=$4; shift 4
    if test "$backend" = c; then
        "$ziran" build --target=c "$@" --root "$input_root" --entry capability:main --exe -o "$output" "$input"
    else
        "$ziran" build --target=cpp "$@" --root "$input_root" --entry capability:main --no-main -o "$output" "$input"
        "${CXX:-c++}" -std=c++17 -O2 -pthread "$output"/*.cpp -o "$output/capability"
    fi
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
for target in c cpp; do
    build_program "$target" "$work" "$work/native-$target" "$work/capability.zi"
    test "$("$work/native-$target/capability")" = 'POSIX threads'
    "$ziran" ir --target="$target" --root "$work" -o "$work/native-ir-$target" "$work/capability.zi"
    build_program "$target" "$work/native-ir-$target" "$work/native-saved-$target" "$work/native-ir-$target/capability.zir"
    test "$("$work/native-saved-$target/capability")" = 'POSIX threads'
    for platform in _WIN32 PLAN9 PLATFORM_WEB; do
        expected=portable
        if test "$platform" = _WIN32; then expected='POSIX threads'; fi
        build_program "$target" "$work" "$work/$target-$platform" "$work/capability.zi" --define "$platform"
        test "$("$work/$target-$platform/capability")" = "$expected"
        "$ziran" ir --target="$target" --define "$platform" --root "$work" -o "$work/ir-$target-$platform" "$work/capability.zi"
        build_program "$target" "$work/ir-$target-$platform" "$work/saved-$target-$platform" "$work/ir-$target-$platform/capability.zir"
        test "$("$work/saved-$target-$platform/capability")" = "$expected"
    done
done
"$ziran" ir --root "$work" -o "$work/ir" "$work/capability.zi"
"$ziran" build --target=c --root "$work/ir" --entry capability:main --exe -o "$work/saved" "$work/ir/capability.zir"
test "$("$work/saved/capability")" = 'portable'

# Automatic native capability insertion must never overrun the define table.
set --
index=0
while test "$index" -lt 64; do set -- "$@" --define FULL; index=$((index + 1)); done
for target in c cpp; do
    for command in build ir; do
        if "$ziran" "$command" --target="$target" "$@" --root "$work" -o "$work/full-$command-$target" "$work/capability.zi" 2> "$work/full.err"; then
            echo 'Full define table unexpectedly accepted native capability insertion' >&2
            exit 1
        fi
        grep -Fq 'no define slot left for native thread capability' "$work/full.err"
    done
done
echo 'Native C/C++ source and explicit target IR agree; portable selectors and bounded defines pass'
