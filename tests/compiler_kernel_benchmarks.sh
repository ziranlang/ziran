#!/bin/sh
# check-cost: 235
# Run with: build-queue -- sh tests/compiler_kernel_benchmarks.sh [options]
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
unset SESSION_MANAGER XDG_SESSION_ID XDG_SESSION_TYPE ZIRAN_PROFILE
export YUE_DESKTOP_RECOVERY=0
# tests/run_check.py passes the ziran launcher first: use its build/bin and
# a fresh scratch output that is removed afterwards.
if [ $# -gt 0 ] && [ -f "$1" ] && [ -x "$1" ]; then
    bin=$(dirname -- "$1"); shift
    out="$repo/build/scratch/codec-kernels-check.$$"
    trap 'rm -rf "$out"' EXIT HUP INT TERM
    python3 "$repo/bench/codec_kernels.py" --bin-dir "$bin" --out "$out" "$@"
    exit
fi
exec python3 "$repo/bench/codec_kernels.py" "$@"
