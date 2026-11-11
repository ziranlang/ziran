#!/bin/sh
# Run on Alpha: build-queue -- sh bench/compiler_kernel_benchmarks.sh [options]
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
unset SESSION_MANAGER XDG_SESSION_ID XDG_SESSION_TYPE ZIRAN_PROFILE
export YUE_DESKTOP_RECOVERY=0
exec python3 "$repo/bench/codec_kernels.py" "$@"
