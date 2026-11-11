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
mkdir -p "$root/build"
work=$(mktemp -d "$root/build/compiler-runtime-windows.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
ar=${WIN64_AR:-x86_64-w64-mingw32-ar}
objcopy=${WIN64_OBJCOPY:-x86_64-w64-mingw32-objcopy}
set --
if test -n "${MCFGTHREADS:-}"; then
    set -- "-L$MCFGTHREADS/lib"
fi
make -j4 -C "$root" BOOTSTRAP=1 BUILD_DIR="$work/build" \
    CC="$cc" AR="$ar" OBJCOPY="$objcopy" CFLAGS='-O2 -fPIC' \
    FRAMEFLAGS=-Wframe-larger-than=16384 "$work/build/libziran.a"
"$cc" -D_GNU_SOURCE -std=c11 -static -I"$root/include" -I"$root/cmd/zir" -I"$root/bootstrap/compiler_ir" \
    "$root/tests/bundle_link_test.c" "$work/build/libziran.a" -lm \
    "$@" -o "$work/runtime.exe"
echo 'compiler_runtime_windows: complete runtime compile and link passed'
