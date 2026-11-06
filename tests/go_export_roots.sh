#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/m.zi" <<'ZI'
#program_export "Probe"
Probe :: () -> s32 { return 7 }
Dead :: () -> s32 { return 9 }
main :: () -> s32 { return 0 }
ZI
"$ziran" build --target=go --exe --pkg main --root "$work" --module-path std --entry m:main -o "$work/out" "$work/m.zi"
grep -q 'func Probe(' "$work/out"/*.go
grep -q 'func M_Main(' "$work/out"/*.go
if grep -q 'Dead' "$work/out"/*.go; then
    echo 'unreachable non-exported function was emitted' >&2
    exit 1
fi
cat > "$work/out/probe_test.go" <<'GO'
package main
import "testing"
func TestProbe(t *testing.T) {
    if Probe() != 7 || M_Main() != 0 { t.Fatal("export roots") }
}
GO
(cd "$work/out" && GO111MODULE=off GOCACHE=/tmp/ziran-bind-go-cache go test .)

# Exports whose signature needs a #c_call callback, which Go cannot express,
# are left out unless the entry calls them; then the target check rejects them.
cat > "$work/callback.zi" <<'ZI'
Callback :: #type (data: *void) #c_call;
#program_export
Register :: (callback: Callback, data: *void) -> s32 { return 1 }
#program_export
Plain :: () -> s32 { return 7 }
main :: () -> s32 { return Plain() - 7 }
Calls :: (callback: Callback) -> s32 { return Register(callback, null) }
ZI
"$ziran" build --target=go --exe --pkg main --root "$work" --module-path std --entry callback:main -o "$work/callback" "$work/callback.zi"
grep -q 'func Callback_Plain(' "$work/callback"/*.go
if grep -q 'Register' "$work/callback"/*.go; then
    echo 'Go output kept an export that needs a #c_call callback' >&2
    exit 1
fi
if "$ziran" build --target=go --exe --pkg main --root "$work" --module-path std --entry callback:Calls -o "$work/calls" "$work/callback.zi" 2>"$work/calls.err"; then
    echo 'Go output accepted a reached #c_call callback' >&2
    exit 1
fi
grep -q 'does not support #c_call procedure types: Callback' "$work/calls.err"
echo 'Go program_export roots: passed'
