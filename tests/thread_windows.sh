#!/bin/sh
set -eu
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER LD_PRELOAD
export YUE_DESKTOP_RECOVERY=0
ziran=${1:?pass ziran}
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cc=${WIN64_CC:-x86_64-w64-mingw32-gcc}
wine=${WINDOWS_WINE:-wine}
for tool in "$cc" "$wine" Xvfb xvfb-run wineserver; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "SKIP: Windows thread test needs $tool" >&2
        if test "${WINDOWS_REQUIRED:-0}" = 1; then exit 1; fi
        exit 0
    fi
done
work=${ZIRAN_THREAD_WINDOWS_BUILD:-$(mktemp -d)}
mkdir -p "$work"
work=$(CDPATH= cd -- "$work" && pwd)
exec 9>"$work/check.lock"
flock -n 9 || { echo 'Windows thread checks are already running' >&2; exit 1; }
cleanup() {
    if test -d "$work/wine"; then
        WINEPREFIX="$work/wine" wineserver -k || :
        WINEPREFIX="$work/wine" wineserver -w || :
    fi
    if test -z "${ZIRAN_THREAD_WINDOWS_BUILD:-}"; then rm -rf "$work"; fi
}
trap cleanup EXIT HUP INT TERM
set --
if test -n "${MCFGTHREADS:-}"; then
    set -- "-L$MCFGTHREADS/lib"
fi
"$ziran" check --define _WIN32 --root "$root/tests/spec" --module-path "$root/std" \
    "$root/tests/spec/thread_linux_test.zi"
"$ziran" ir --target=c --define _WIN32 --root "$root/tests/spec" \
    --module-path "$root/std" -o "$work/ir-c" "$root/tests/spec/thread_linux_test.zi"
for form in source saved; do
    source=$root/tests/spec/thread_linux_test.zi
    source_root=$root/tests/spec
    modules=$root/std
    if test "$form" = saved; then
        source=$work/ir-c/thread_linux_test.zir
        source_root=$work/ir-c
        modules=$work/ir-c
    fi
    output=$work/$form-c
    "$ziran" build --target=c --define _WIN32 --no-main \
        --root "$source_root" --module-path "$modules" -o "$output" "$source"
    "$cc" -std=c11 -O2 -static -Wall -Wextra -I"$root/include" -I"$output" \
        "$root/tests/thread_windows_completion_test.c" "$output"/*.c "$@" -o "$work/$form-c.exe"
done
cat > "$work/run.sh" <<'SH'
#!/bin/sh
set -eu
work=$1; wine=$2
export WINEPREFIX="$work/wine" WINEARCH=win64 WINEDEBUG=-all
export WINEDLLOVERRIDES='mscoree,mshtml,winemenubuilder.exe=d'
for form in source saved; do
    "$wine" "$work/$form-c.exe"
done
SH
env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS \
    xvfb-run -a sh "$work/run.sh" "$work" "$wine"
