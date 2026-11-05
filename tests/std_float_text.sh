#!/bin/sh
# check-cost: 750
set -eu
ziran=$1
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$repo/build/tests"
work=$(mktemp -d "$repo/build/tests/float-text.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
export TMPDIR="$work" ZIRAN_NATIVE_DIR="$work" ZIRAN_NATIVE_CACHE=0
# Reuse the normal Go cache; the fixture itself stays in this repository.
go run "$repo/tests/float_text_oracle.go" > "$work/float_text_cases.zi"
run_bundle() {
    result=$("$@")
    if test "$result" != 0; then
        printf '%s\n' "$result"
        exit 1
    fi
}
"$ziran" check --root "$work" --module-path "$repo/tests/spec" --module-path "$repo/std" "$work/float_text_cases.zi"
"$ziran" ir --root "$work" --module-path "$repo/tests/spec" --module-path "$repo/std" -o "$work/ir" "$work/float_text_cases.zi"
for form in source saved; do
    if test "$form" = source; then
        input="$work/float_text_cases.zi"
        set -- --root "$work" --module-path "$repo/tests/spec" --module-path "$repo/std"
    else
        input="$work/ir/float_text_cases.zir"
        set -- --root "$work/ir" --module-path "$work/ir"
    fi
    "$ziran" bundle "$@" --entry float_text_cases:NativeMain -o "$work/$form.zx" "$input"
    for target in c go; do
        output="$work/$form-$target"
        if test "$target" = go; then
            "$ziran" build "$@" --target=go --pkg main --exe --entry float_text_cases:NativeMain -o "$output" "$input"
            GO111MODULE=off go run "$output"/*.go
        else
            "$ziran" build "$@" --target=c --exe --entry float_text_cases:NativeMain -o "$output" "$input"
            "$output/float_text_cases"
        fi
    done
    echo "float text: $form C/Go passed"
done
# The source and saved bundles are the same bytes, so one run of each engine
# covers both; the VM is the slowest part of this check under the sanitizers.
cmp "$work/source.zx" "$work/saved.zx"
run_bundle "$ziran" run "$work/source.zx"
run_bundle "$(dirname -- "$ziran")/zi2zx-vm" run "$work/source.zx"
echo 'float text and JSON: edge, every binade boundary, seeded random and long decimal parity passed on C/Go/VM from source and saved IR'
