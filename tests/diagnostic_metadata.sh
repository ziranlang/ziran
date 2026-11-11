#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${1:?pass the ziran command}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
cat > "$work/types.zi" <<'ZI'
Add :: (a: s32, b: s32) -> s32 { return a + b; }
Text :: () -> s32 { return "x"; }
main :: () {
    flag: bool = 3.5;
    total: s32 = Add(1, "two");
    total = "wrong";
}
ZI
if "$ziran" check --diagnostics=json --root "$work" "$work/types.zi" 2> "$work/errors.jsonl"; then
    echo 'type mismatches unexpectedly passed' >&2
    exit 1
fi
"$ziran" run --target=c --root "$repo/tests" "$repo/tests/json_assertions.zi" -- diagnostic-metadata "$work/errors.jsonl"
cat > "$work/names.zi" <<'ZI'
Compute :: (value: s64) -> s64 { return value; }
main :: () {
    count := 3;
    print("cuont: %\n", cuont); // cuont in comment
    Comptue(count);
}
ZI
if "$ziran" check --diagnostics=json --root "$work" "$work/names.zi" 2> "$work/names.jsonl"; then
    echo 'unknown names unexpectedly passed' >&2
    exit 1
fi
"$ziran" run --target=c --root "$repo/tests" "$repo/tests/json_assertions.zi" -- diagnostic-names "$work/names.jsonl" "$work/names.zi"
"$ziran" check --root "$work" "$work/names.zi"
