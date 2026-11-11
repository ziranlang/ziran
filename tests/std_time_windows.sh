#!/bin/sh
set -eu
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS

ziran=${1:?pass ziran}
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$root/build"
work=$(mktemp -d "$root/build/std-time-windows.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM

"$ziran" ir --root "$root/tests/spec" --module-path "$root/std" \
    --define _WIN32 -o "$work/ir" "$root/tests/spec/time_windows_test.zi"

cc=${WIN64_CC:-x86_64-w64-mingw32-gcc}
set --
if test -n "${MCFGTHREADS:-}"; then
    set -- "-L$MCFGTHREADS/lib"
fi
printf 'int main(void) { return 0; }\n' > "$work/probe.c"
can_link=false
if command -v "$cc" >/dev/null 2>&1 && "$cc" -static "$work/probe.c" "$@" -o "$work/probe.exe"; then
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
    "$cc" -std=c11 -static -Wall -Wextra -Werror -I"$root/include" -I"$out" \
        "$out"/*.c "$@" -o "$work/time-$input.exe"
done
if "$can_link"; then
    echo 'std_time_windows: source and saved IR generated, compiled and linked'
else
    echo "std_time_windows: no working $cc; source and saved IR generation checked"
fi
