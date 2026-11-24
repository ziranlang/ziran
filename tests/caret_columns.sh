#!/bin/sh
# Diagnostics inside a declaration's value point at the failing part: the
# argument of `t := TextTrim(42)` is reported at its own column with the caret
# under it, while an error on the statement itself still counts from the
# statement start.
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$repo/build/tests"
work=$(mktemp -d "$repo/build/tests/caret-columns-XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS

fail() { echo "caret_columns: $*" >&2; exit 1; }

cat > "$work/golden.zi" <<'ZI'
#import "std/text_tools"
main :: () -> s32 {
    t := TextTrim(42)
    return 0
}
ZI
if "$ziran" check --root "$work" "$work/golden.zi" >"$work/out" 2>"$work/err"; then
    fail "the invalid argument passed"
fi
grep -Fq "golden.zi:3:19: argument type mismatch" "$work/err" || fail "the argument is not reported at its own column"
# The excerpt drops the source line's own indent and prefixes eight spaces;
# column 19 of `    t := TextTrim(42)` puts the caret at index 23.
awk 'index($0, "^") == 23 && $0 ~ /\^\^\^$/ { found = 1 } END { exit(found ? 0 : 1) }' "$work/err" ||
    fail "the caret excerpt does not underline the argument"

cat > "$work/statement.zi" <<'ZI'
main :: () -> s32 {
    let x = 1
    return 0
}
ZI
if "$ziran" check --root "$work" "$work/statement.zi" >"$work/out2" 2>"$work/err2"; then
    fail "the invalid statement passed"
fi
grep -Fq "statement.zi:2:5: let is not Ziran syntax" "$work/err2" || fail "the statement error is not at its own column"
awk 'index($0, "^") == 9 && $0 ~ /\^$/ { found = 1 } END { exit(found ? 0 : 1) }' "$work/err2" ||
    fail "the statement caret is not at the statement start"
