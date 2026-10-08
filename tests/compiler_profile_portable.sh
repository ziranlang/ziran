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

check_record() {
    python3 - "$1" <<'PY'
import json
import sys
from pathlib import Path

record, = [json.loads(line) for line in Path(sys.argv[1]).read_text().splitlines()]
assert record['schema_version'] == 1 and record['pid'] > 0, record
assert record['profile_elapsed_ms'] > 0 and record['max_rss_kib'] > 0, record
assert record['workspace_allocation_calls'] == 1, record
assert record['workspace_allocation_bytes'] == 4096, record
assert record['counters'] == {'portable': 1}, record
phase, = record['phases']
assert phase['name'] == 'portable' and phase['calls'] == 1, record
assert 0 <= phase['inclusive_ms'] <= record['profile_elapsed_ms'], record
PY
}

"${CC:-cc}" -D_GNU_SOURCE -std=c11 -Wall -Wextra -Werror \
    -I"$root/cmd/zir" "$root/tests/compiler_profile_portable.c" \
    "$root/cmd/zir/zir_profile.c" -o "$work/profile"
env -u ZIRAN_PROFILE "$work/profile"
ZIRAN_PROFILE="$work/native.jsonl" "$work/profile"
check_record "$work/native.jsonl"

cc=${WIN64_CC:-x86_64-w64-mingw32-gcc}
if ! command -v "$cc" >/dev/null 2>&1; then
    echo "compiler_profile_portable: native passed; $cc unavailable"
    exit 0
fi
"$cc" -std=c11 -Wall -Wextra -Werror -I"$root/cmd/zir" \
    "$root/tests/compiler_profile_portable.c" "$root/cmd/zir/zir_profile.c" \
    -o "$work/profile.exe"

wine=${WINE:-wine}
if ! command -v "$wine" >/dev/null 2>&1 || \
    ! command -v wineserver >/dev/null 2>&1 || \
    ! command -v xvfb-run >/dev/null 2>&1; then
    echo 'compiler_profile_portable: native and Windows compile/link passed'
    exit 0
fi
mkdir "$work/run"
wine_prefix_started=1
(cd "$work/run" && \
    WINEPREFIX="$work/prefix" WINEDEBUG=-all \
    WINEDLLOVERRIDES='mscoree,mshtml=' \
    xvfb-run -a env -u ZIRAN_PROFILE "$wine" "$work/profile.exe")
(cd "$work/run" && \
    WINEPREFIX="$work/prefix" WINEDEBUG=-all \
    WINEDLLOVERRIDES='mscoree,mshtml=' ZIRAN_PROFILE=windows.jsonl \
    xvfb-run -a "$wine" "$work/profile.exe")
check_record "$work/run/windows.jsonl"
echo 'compiler_profile_portable: native and Windows profile records passed'
