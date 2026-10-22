#!/bin/sh
set -eu
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS

ziran=${1:?pass ziran}
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
cleanup() {
    if [ -d "$work/prefix" ] && command -v wineserver >/dev/null 2>&1; then
        WINEPREFIX="$work/prefix" wineserver -k
        WINEPREFIX="$work/prefix" wineserver -w
    fi
    rm -rf "$work"
}
trap cleanup EXIT HUP INT TERM

"$ziran" ir --root "$root/tests/spec" --module-path "$root/std" \
    --define _WIN32 -o "$work/ir" "$root/tests/spec/time_windows_test.zi"

cc=${WIN64_CC:-x86_64-w64-mingw32-gcc}
printf 'int main(void) { return 0; }\n' > "$work/probe.c"
can_link=false
if command -v "$cc" >/dev/null 2>&1 && "$cc" "$work/probe.c" -o "$work/probe.exe"; then
    can_link=true
fi

for input in source saved; do
    source=$root/tests/spec/time_windows_test.zi
    source_root=$root/tests/spec
    if [ "$input" = saved ]; then
        source=$work/ir/time_windows_test.zir
        source_root=$work/ir
    fi
    out=$work/$input
    "$ziran" build --target=c --no-main --define _WIN32 \
        --root "$source_root" --module-path "$root/std" -o "$out" "$source"
    if grep -E '__asm__\("(clock_gettime|clock_nanosleep|localtime_r|gmtime_r|setenv|unsetenv)"\)' "$out"/*.c; then
        echo 'Windows clocks still call POSIX functions' >&2
        exit 1
    fi
    if ! "$can_link"; then
        continue
    fi
    cat > "$out/entry.c" <<'C'
#define ZIRAN_DATE_TIME_LINUX_H_PRIVATE 1
#include "time_windows_test.h"
#include <stddef.h>
#include <stdio.h>
#include <windows.h>
_Static_assert(sizeof(FileTime) == sizeof(FILETIME), "FILETIME ABI");
_Static_assert(sizeof(SystemTime) == sizeof(SYSTEMTIME), "SYSTEMTIME ABI");
_Static_assert(sizeof(ZoneInformation) == sizeof(TIME_ZONE_INFORMATION), "timezone ABI");
_Static_assert(offsetof(ZoneInformation, daylight_date) == offsetof(TIME_ZONE_INFORMATION, DaylightDate), "timezone fields ABI");
int main(void) {
    int result = CheckWindowsTime();
    if (result) fprintf(stderr, "Windows clock check failed: %d\n", result);
    return result;
}
C
    "$cc" -std=c11 -Wall -Wextra -Werror -I"$root/include" -I"$out" \
        "$out"/*.c -o "$work/time-$input.exe"
    if command -v "${WINE:-wine}" >/dev/null 2>&1; then
        # The prefix belongs only to this test. Scrubbed display variables
        # prevent Wine's first-run services from touching the owner's desktop.
        WINEPREFIX="$work/prefix" WINEDEBUG=-all WINEDLLOVERRIDES='mscoree,mshtml=' \
            "${WINE:-wine}" "$work/time-$input.exe"
    fi
done
if "$can_link"; then
    echo 'std_time_windows: source and saved IR linked; Wine checks ran when available'
else
    echo "std_time_windows: no working $cc; source and saved IR generation checked"
fi
