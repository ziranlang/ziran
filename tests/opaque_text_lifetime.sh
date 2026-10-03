#!/bin/sh
set -eu
ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/opaque.zi" <<'ZI'
Node :: struct { bytes: [2]u8 }
native :: #system_library "libc";
GetNode :: () -> *Node #foreign native "get_node";
Bad :: () -> s32 {
    pointer := GetNode()
    view := TextView(pointer.bytes[:])
    return cast(s32)view[0]
}
ZI
cat > "$work/alias.zi" <<'ZI'
Node :: struct { bytes: [2]u8 }
Bad :: (pointer: *Node) -> s32 {
    alias := pointer
    view := TextView(alias.bytes[:])
    pointer.bytes[0] = cast(u8)98
    return cast(s32)view[0]
}
ZI
for fixture in opaque alias; do
    if "$ziran" check --diagnostics=json --root "$work" "$work/$fixture.zi" \
        > "$work/stdout" 2> "$work/diagnostics.jsonl"; then
        echo "unsafe $fixture text backing was accepted" >&2
        exit 1
    fi
    python3 - "$work/diagnostics.jsonl" <<'PY'
import json, pathlib, sys
items = [json.loads(line) for line in pathlib.Path(sys.argv[1]).read_text().splitlines()]
assert items and all(item['code'] == 'check.slice_lifetime' for item in items), items
PY
done

cat > "$work/safe.zi" <<'ZI'
Node :: struct { bytes: [2]u8; result: s32 }
Read :: (pointer: *Node) -> s32 {
    alias := pointer
    {
        view := TextView(alias.bytes[:])
        if view != "ab" { return 1 }
        pointer.result = 42
    }
    pointer.bytes[0] = cast(u8)99
    return pointer.result
}
#program_export
Answer :: () -> s32 {
    node: Node
    node.bytes[0] = 97
    node.bytes[1] = 98
    return Read(*node)
}
ZI
"$ziran" ir --root "$work" -o "$work/ir" "$work/safe.zi"
for input in "$work/safe.zi" "$work/ir/safe.zir"; do
    "$ziran" check --root "$work" "$input"
    "$ziran" bundle --root "$work" --entry safe:Answer -o "$work/safe.zib" "$input"
    test "$("$ziran" run "$work/safe.zib")" = 42
done
