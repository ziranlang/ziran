#!/bin/sh
# Actual Win64 source/saved-IR execution, using the existing native pool.
set -eu
ziran=${1:?pass the ziran command}
cc=${WINDOWS_CC:-x86_64-w64-mingw32-gcc}
cxx=${WINDOWS_CXX:-x86_64-w64-mingw32-g++}
wine=${WINDOWS_WINE:-wine}
for tool in "$cc" "$cxx" "$wine" Xvfb wineserver; do
    command -v "$tool" >/dev/null 2>&1 || {
        echo "SKIP: Windows worker test needs $tool" >&2
        if test "${WINDOWS_REQUIRED:-0}" = 1; then exit 1; fi
        exit 0
    }
done
case "$($cc -dumpmachine)" in x86_64-*mingw*) ;; *) echo 'Windows worker test requires Win64 MinGW' >&2; exit 1 ;; esac
if test -n "${WINDOWS_WORK:-}"; then
    work=$WINDOWS_WORK
    mkdir "$work"
else
    work=$(mktemp -d)
fi
cleanup() {
    if test -d "$work/wine"; then
        env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS -u LD_PRELOAD \
            WINEPREFIX="$work/wine" wineserver -k || :
        env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS -u LD_PRELOAD \
            WINEPREFIX="$work/wine" wineserver -w || :
    fi
    if test -z "${WINDOWS_WORK:-}"; then rm -rf "$work"; fi
}
trap cleanup EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS LD_PRELOAD ZIRAN_PAR_THREADS
export YUE_DESKTOP_RECOVERY=0
export XDG_RUNTIME_DIR="$work/runtime"
mkdir -m 700 "$XDG_RUNTIME_DIR"
for target in c cpp; do
    "$ziran" ir --target="$target" --define _WIN32 --root tests/spec \
        --module-path std=std --module-path std -o "$work/ir-$target" tests/spec/native_parallel_windows_test.zi
    for kind in source saved; do
        input_root=tests/spec; input=tests/spec/native_parallel_windows_test.zi
        if test "$kind" = saved; then
            input_root="$work/ir-$target"; input="$input_root/native_parallel_windows_test.zir"
        fi
        output="$work/$target-$kind"
        "$ziran" build --target="$target" --define _WIN32 --root "$input_root" \
            --module-path std=std --module-path std --entry native_parallel_windows_test:main \
            --no-main -o "$output" "$input"
        if test "$target" = c; then
            "$cc" -std=c11 -O2 -static -pthread -Iinclude -I"$output" "$output"/*.c -lm -o "$output.exe"
        else
            "$cxx" -std=c++17 -O2 -static -pthread -Iinclude -I"$output" "$output"/*.cpp -lm -o "$output.exe"
        fi
    done
done
# One private server/prefix for this test; no inherited desktop is reachable.
cat > "$work/run.sh" <<'SH'
#!/bin/sh
set -eu
work=$1; wine=$2
export WINEPREFIX="$work/wine" WINEARCH=win64 WINEDEBUG=-all
export WINEDLLOVERRIDES='mscoree,mshtml,winemenubuilder.exe=d'
for target in c cpp; do
    for kind in source saved; do
        "$wine" "$work/$target-$kind.exe"
        for count in 1 8; do ZIRAN_PAR_THREADS=$count "$wine" "$work/$target-$kind.exe"; done
    done
done
SH
env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS -u LD_PRELOAD \
    xvfb-run -a sh "$work/run.sh" "$work" "$wine"
echo 'Win64 C/C++ source and saved IR: default four workers, publication and close/reopen pass'
