#!/bin/sh
set -eu
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
export YUE_DESKTOP_RECOVERY=0
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

"${CC:-cc}" -D_GNU_SOURCE -std=c11 -Wall -Wextra -Werror \
    -I"$root/cmd/zir" "$root/tests/compiler_files_portable.c" \
    "$root/cmd/zir/zir_files.c" -o "$work/files"
mkdir "$work/native"
(cd "$work/native" && "$work/files")

cc=${WIN64_CC:-x86_64-w64-mingw32-gcc}
if ! command -v "$cc" >/dev/null 2>&1; then
    echo "compiler_files_portable: native passed; $cc unavailable"
    exit 0
fi
"$cc" -std=c11 -Wall -Wextra -Werror -I"$root/cmd/zir" \
    "$root/tests/compiler_files_portable.c" "$root/cmd/zir/zir_files.c" \
    -o "$work/files.exe"

wine=${WINE:-wine}
if ! command -v "$wine" >/dev/null 2>&1 || \
    ! command -v wineserver >/dev/null 2>&1 || \
    ! command -v xvfb-run >/dev/null 2>&1; then
    echo 'compiler_files_portable: native and Windows compile/link passed'
    exit 0
fi
mkdir "$work/windows"
wine_prefix_started=1
(cd "$work/windows" && \
    WINEPREFIX="$work/prefix" WINEDEBUG=-all \
    WINEDLLOVERRIDES='mscoree,mshtml=' \
    xvfb-run -a "$wine" "$work/files.exe")
echo 'compiler_files_portable: native and Windows file operations passed'
