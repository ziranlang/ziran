#!/bin/sh
set -eu
unset DISPLAY WAYLAND_DISPLAY

# The native file and mapping modules on Windows: generate for _WIN32, link
# with MinGW when it is installed (PE links report every unresolved symbol,
# even in unused code), and run the result under Wine when that is present.
ziran=${1:?pass ziran}
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
compiler=$(dirname -- "$ziran")/zi2c
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

"$compiler" --no-main --root "$root" --module-path "$root/std" \
    --define _WIN32 -o "$work/gen" "$root/tests/std_file_windows.zi"
grep -q '__asm__("CreateFileW")' "$work/gen/file_linux.c"
if grep -q '__asm__("\(open\|pread\|renameat2\|mmap\)")' "$work/gen/file_linux.c" \
    "$work/gen/mapped_file_linux.c"; then
    echo 'the Windows file modules still call POSIX' >&2; exit 1
fi

cc=${WIN64_CC:-x86_64-w64-mingw32-gcc}
printf 'int main(void) { return 0; }\n' > "$work/probe.c"
if ! command -v "$cc" > /dev/null 2>&1 ||
    ! "$cc" "$work/probe.c" -o "$work/probe.exe" > /dev/null 2>&1; then
    echo "std_file_windows: no working $cc; generation checked only"
    exit 0
fi
"$cc" -std=c11 -Wall -Wextra -Werror -I"$root/include" \
    $(find "$work/gen" -name '*.h' -exec dirname {} \; | sort -u | sed 's/^/-iquote/') \
    $(find "$work/gen" -name '*.c') -o "$work/files.exe"

wine=${WINE:-wine}
if ! command -v "$wine" > /dev/null 2>&1; then
    echo "std_file_windows: $wine unavailable; link checked only"
    exit 0
fi
# A new Wine prefix takes over a gigabyte; reuse one when WINEPREFIX names it.
mkdir "$work/run"
(cd "$work/run" && WINEPREFIX="${WINEPREFIX:-$work/prefix}" WINEDEBUG=-all \
    WINEDLLOVERRIDES='mscoree,mshtml=' "$wine" "$work/files.exe")
echo "std_file_windows: passed under Wine"
