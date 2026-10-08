#!/bin/sh
set -eu
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
export YUE_DESKTOP_RECOVERY=0
cc=${WIN64_CC:-x86_64-w64-mingw32-gcc}
if ! command -v "$cc" >/dev/null 2>&1; then
    echo "compiler_runtime_windows: $cc unavailable"
    exit 0
fi
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
wine_prefix_started=0
cleanup() {
    if [ "$wine_prefix_started" = 1 ]; then
        WINEPREFIX="$work/prefix" wineserver -k
        WINEPREFIX="$work/prefix" wineserver -w
    fi
    rm -rf "$work"
}
trap cleanup EXIT HUP INT TERM
ar=${WIN64_AR:-x86_64-w64-mingw32-ar}
objcopy=${WIN64_OBJCOPY:-x86_64-w64-mingw32-objcopy}
make -j4 -C "$root" BOOTSTRAP=1 BUILD_DIR="$work/build" \
    CC="$cc" AR="$ar" OBJCOPY="$objcopy" CFLAGS='-O2 -fPIC' \
    FRAMEFLAGS=-Wframe-larger-than=16384 "$work/build/libziran.a"
"$cc" -D_GNU_SOURCE -std=c11 -I"$root/include" -I"$root/cmd/zir" \
    "$root/tests/bundle_link_test.c" "$work/build/libziran.a" -lm \
    -o "$work/runtime.exe"
wine=${WINE:-wine}
if ! command -v "$wine" >/dev/null 2>&1 || \
    ! command -v wineserver >/dev/null 2>&1 || \
    ! command -v xvfb-run >/dev/null 2>&1; then
    echo 'compiler_runtime_windows: complete runtime compile and link passed'
    exit 0
fi
wine_prefix_started=1
WINEPREFIX="$work/prefix" WINEDEBUG=-all WINEDLLOVERRIDES='mscoree,mshtml=' \
    xvfb-run -a "$wine" "$work/runtime.exe"
echo 'compiler_runtime_windows: complete runtime compile, link and execution passed'
