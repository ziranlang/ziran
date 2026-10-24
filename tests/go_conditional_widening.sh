#!/bin/sh
# A conditional whose arms have different numeric types, such as a float64
# literal and a float32 binding, takes the wider type. Go does not convert
# an arm by itself, so the arm is converted explicitly.
set -eu
ziran=${1:?pass the ziran command}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/pick.zi" <<'ZI'
Pick :: (shared: bool, fade: float32) -> float64 {
    alpha := ifx shared then 1.0 else fade
    return alpha
}
#program_export
main :: () -> s32 {
    if Pick(true, 0.25) != 1.0 || Pick(false, 0.25) != 0.25 { return 1 }
    return 42
}
ZI
"$ziran" build --target=go --pkg main --exe --root "$work" --entry pick:main \
    -o "$work/go" "$work/pick.zi"
GO111MODULE=off go build -o "$work/go/app" "$work/go"/*.go
status=0
"$work/go/app" || status=$?
test "$status" = 42
"$ziran" bundle --root "$work" --entry pick:main -o "$work/pick.zx" "$work/pick.zi"
test "$("$ziran" run "$work/pick.zx")" = 42
echo "conditional arms of different numeric types run on Go and in bundles"
