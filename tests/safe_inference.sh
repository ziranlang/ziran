#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${1:-"$repo/build/bin/ziran"}
case "$ziran" in /*) ;; *) ziran="$repo/$ziran";; esac
work="$repo/build/tests/safe-inference"
mkdir -p "$work/tmp" "$work/source"
export TMPDIR="$work/tmp" ZIRAN_DESKTOP_RECOVERY=0 HARMONY_DESKTOP_RECOVERY=0 YUE_DESKTOP_RECOVERY=0
export GOPATH="$work/go" GOTMPDIR="$work/tmp" GOMAXPROCS=2 GOFLAGS=-p=2 GOTOOLCHAIN=local
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS GOWORK
cp "$repo/tests/safe_inference.zi" "$work/source/safe_inference.zi"
"$ziran" ir --root "$work/source" -o "$work/ir" "$work/source/safe_inference.zi"
for state in source saved; do
    input="$work/source/safe_inference.zi"
    root="$work/source"
    if test "$state" = saved; then input="$work/ir/safe_inference.zir"; root="$work/ir"; fi
    "$ziran" bundle --root "$root" --entry safe_inference:main -o "$work/$state.zx" "$input"
    test "$("$(dirname "$ziran")/zi2zx" run --vm "$work/$state.zx")" = 42
    test "$("$ziran" run "$work/$state.zx")" = 42
    for target in c go; do
        out="$work/$state-$target"
        if test "$target" = go; then
            "$ziran" build --target=go --pkg main --exe --entry safe_inference:main --root "$root" -o "$out" "$input"
            GO111MODULE=off go build -o "$out/app" "$out"/*.go
        else
            "$ziran" build --target=c --exe --entry safe_inference:main --root "$root" -o "$out" "$input"
        fi
        executable="$out/app"
        if test "$target" = c; then executable="$out/safe_inference"; fi
        test "$("$executable")" = 42
    done
done
"$ziran" run --target=c --root "$repo/tests" "$repo/tests/native_inline_cases.zi" -- "$ziran" "$work" inference
