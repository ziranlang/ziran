#!/bin/sh
set -eu
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
export YUE_DESKTOP_RECOVERY=0
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$root/build"
work=$(mktemp -d "$root/build/compiler-files-portable.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM

"${CC:-cc}" -D_GNU_SOURCE -std=c11 -Wall -Wextra -Werror \
    -I"$root/cmd/zir" -I"$root/bootstrap/compiler_ir" "$root/tests/compiler_files_portable.c" \
    "$root/cmd/zir/zir_files.c" "$root/cmd/zir/zir_memory.c" -o "$work/files"
mkdir "$work/native"
(cd "$work/native" && "$work/files")

cc=${WIN64_CC:-x86_64-w64-mingw32-gcc}
set --
if test -n "${MCFGTHREADS:-}"; then
    set -- "-L$MCFGTHREADS/lib"
fi
if ! command -v "$cc" >/dev/null 2>&1; then
    echo "compiler_files_portable: native passed; $cc unavailable"
    exit 0
fi
"$cc" -std=c11 -static -Wall -Wextra -Werror -I"$root/cmd/zir" -I"$root/bootstrap/compiler_ir" \
    "$root/tests/compiler_files_portable.c" "$root/cmd/zir/zir_files.c" "$root/cmd/zir/zir_memory.c" \
    "$@" -o "$work/files.exe"

echo 'compiler_files_portable: native file operations and Windows compile/link passed'
