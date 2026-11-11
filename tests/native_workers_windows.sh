#!/bin/sh
# Compile and link Win64 source/saved IR using the native worker pool.
set -eu
ziran=${1:?pass the ziran command}
cc=${WINDOWS_CC:-x86_64-w64-mingw32-gcc}
if ! command -v "$cc" >/dev/null 2>&1; then
    echo "SKIP: Windows worker test needs $cc" >&2
    if test "${WINDOWS_REQUIRED:-0}" = 1; then exit 1; fi
    exit 0
fi
case "$($cc -dumpmachine)" in x86_64-*mingw*) ;; *) echo 'Windows worker test requires Win64 MinGW' >&2; exit 1 ;; esac
if test -n "${WINDOWS_WORK:-}"; then
    work=$WINDOWS_WORK
    mkdir "$work"
else
    mkdir -p build
    work=$(mktemp -d "$(pwd)/build/native-workers-windows.XXXXXX")
fi
cleanup() {
    if test -z "${WINDOWS_WORK:-}"; then rm -rf "$work"; fi
}
trap cleanup EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER LD_PRELOAD ZIRAN_PAR_THREADS
export YUE_DESKTOP_RECOVERY=0
set --
if test -n "${MCFGTHREADS:-}"; then
    set -- "-L$MCFGTHREADS/lib"
fi
"$ziran" ir --target=c --define _WIN32 --root tests/spec \
    --module-path std=std --module-path std -o "$work/ir-c" tests/spec/native_parallel_windows_test.zi
for kind in source saved; do
    input_root=tests/spec; input=tests/spec/native_parallel_windows_test.zi
    if test "$kind" = saved; then
        input_root="$work/ir-c"; input="$input_root/native_parallel_windows_test.zir"
    fi
    output="$work/c-$kind"
    "$ziran" build --target=c --define _WIN32 --root "$input_root" \
        --module-path std=std --module-path std --entry native_parallel_windows_test:main \
        --no-main -o "$output" "$input"
    if grep -E 'pthread_(mutex|cond|create|join|self)' "$output"/*.c; then
        echo 'Win64 range workers still depend on pthreads' >&2
        exit 1
    fi
    "$cc" -std=c11 -O2 -static -Iinclude -I"$output" "$output"/*.c "$@" -lm -o "$output.exe"
done
echo 'Win64 C source and saved IR: native worker pool compiled and linked'
