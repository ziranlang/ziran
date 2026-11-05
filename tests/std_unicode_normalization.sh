#!/bin/sh
# check-cost: 600
set -eu
ziran=$1
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$repo/build/tests"
work=$(mktemp -d "$repo/build/tests/unicode-normalization.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
export GOCACHE="$work/go-cache"
export TMPDIR="$work" ZIRAN_NATIVE_DIR="$work" ZIRAN_NATIVE_CACHE=0
run_vm() {
    result=$("$@") || { printf '%s\n' "$result"; return 1; }
    test "$result" = 0 || { printf '%s\n' "$result"; return 1; }
}
fixture=unicode_normalization_test
"$ziran" check --root tests/spec --module-path std "tests/spec/$fixture.zi"
"$ziran" ir --root tests/spec --module-path std -o "$work/ir" "tests/spec/$fixture.zi"
for form in source saved; do
    if test "$form" = source; then
        input="$repo/tests/spec/$fixture.zi"
        set -- --root tests/spec --module-path std
    else
        input="$work/ir/$fixture.zir"
        set -- --root "$work/ir" --module-path "$work/ir"
    fi
    "$ziran" bundle "$@" --entry "$fixture:NativeMain" -o "$work/$form.zx" "$input"
    for target in c go; do
        output="$work/$form-$target"
        if test "$target" = go; then
            "$ziran" build "$@" --target=go --pkg main --exe --entry "$fixture:NativeMain" -o "$output" "$input"
            GO111MODULE=off go run "$output"/*.go
        else
            "$ziran" build "$@" --target=c --exe --entry "$fixture:NativeMain" -o "$output" "$input"
            "$output/$fixture"
        fi
    done
done
# The source and saved bundles are the same bytes, so one run of each engine
# covers both; the VM is the slowest part of this check under the sanitizers.
cmp "$work/source.zx" "$work/saved.zx"
run_vm "$ziran" run "$work/source.zx"
run_vm "$(dirname -- "$ziran")/zi2zx-vm" run "$work/source.zx"
if test -n "${ZIRAN_UNICODE_UCD:-}"; then
    "$ziran" build --root scripts --module-path std --target=c --exe --entry unicode_normalization_data:main \
        -o "$work/generator" scripts/unicode_normalization_data.zi
    "$work/generator/unicode_normalization_data" "$ZIRAN_UNICODE_UCD" "$work/data.zi" "$work/corpus.zi"
    cmp std/unicode_normalization_data.zi "$work/data.zi"
    cmp tests/spec/unicode_normalization_cases.zi "$work/corpus.zi"
fi
echo 'Unicode NFC: complete Unicode 17 NormalizationTest + unlisted assigned scalars, strict UTF-8, LowerInto and L/N/White_Space passed on source/saved C/Go/VM'
