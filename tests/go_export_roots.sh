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
echo 'Go program_export roots: passed'
