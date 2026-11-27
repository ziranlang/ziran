#!/bin/sh
set -eu
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
export YUE_DESKTOP_RECOVERY=0
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
bootstrap=${BOOTSTRAP_ROOT:-"$root/bootstrap"}
mkdir -p "$root/build"
work=$(mktemp -d "$root/build/compiler-files-portable.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM

compile_fixture() {
    compiler=$1
    output=$2
    shift 2
    for module in ir context_types parse_types parse_s14_types check_types text statement declaration source scan files; do
        set -- "$@" -I"$bootstrap/compiler_$module"
    done
    "$compiler" -std=c11 -Wall -Wextra -Werror -Wno-unused-label -I"$root/include" "$@" \
        -c "$bootstrap/compiler_files/compiler_files.c" -o "$work/files.o"
    "$compiler" -std=c11 -Wall -Wextra -Werror -I"$root/include" -I"$root/bootstrap/runtime" "$@" \
        "$root/tests/compiler_files_portable.c" "$work/files.o" \
        "$root/bootstrap/runtime/zir_files_host.c" "$root/bootstrap/runtime/zir_memory.c" -o "$output"
}

compile_fixture "${CC:-cc}" "$work/files" -D_GNU_SOURCE
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
compile_fixture "$cc" "$work/files.exe" -static "$@"

echo 'compiler_files_portable: native file operations and Windows compile/link passed'
