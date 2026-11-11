#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

ziran=${1:?pass the ziran command}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/outside_loop.zi" <<'ZI'
Answer :: () -> s32 {
    break
    return 42
}
ZI
cat > "$work/orphan_else.zi" <<'ZI'
Answer :: () -> s32 {
    else
        return 42
    return 0
}
ZI
cat > "$work/deferred_return.zi" <<'ZI'
Answer :: () -> s32 {
    defer return 1
    return 42
}
ZI

for name in outside_loop orphan_else deferred_return; do
    if "$ziran" check --diagnostics=json --root "$work" \
        "$work/$name.zi" > "$work/$name.out" 2> "$work/$name.jsonl"; then
        echo "$name unexpectedly passed" >&2
        exit 1
    fi
    "$ziran" run --target=c --root "$repo/tests" "$repo/tests/json_assertions.zi" -- diagnostic-format "$work/$name.jsonl" "$work/$name.zi"
done

sh "$(dirname "$0")/diagnostic_android.sh" "$ziran"
