#!/bin/sh
set -eu

ziran=${1:?pass the ziran command}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/literal.zi" <<'ZI'
Box :: struct { text: string; }
Bad :: () -> Box {
    bytes: [1]u8 = .[97]
    return Box.{text = TextView(bytes[:])}
}
ZI
cat > "$work/nested.zi" <<'ZI'
Box :: struct { text: string; }
Outer :: struct { inner: Box; }
Bad :: () -> Outer {
    bytes: [1]u8 = .[97]
    return Outer.{inner = Box.{text = TextView(bytes[:])}}
}
ZI
cat > "$work/array.zi" <<'ZI'
Box :: struct { text: string; }
Bad :: () -> [1]Box {
    bytes: [1]u8 = .[97]
    result: [1]Box
    result[0].text = TextView(bytes[:])
    return result
}
ZI
cat > "$work/field.zi" <<'ZI'
Box :: struct { text: string; }
Bad :: () -> Box {
    value: Box
    {
        bytes: [1]u8 = .[97]
        value.text = TextView(bytes[:])
    }
    return value
}
ZI
cat > "$work/call.zi" <<'ZI'
Box :: struct { text: string; }
Echo :: (value: Box) -> Box { return value }
Bad :: () -> Box {
    bytes: [1]u8 = .[97]
    value: Box = Box.{text = TextView(bytes[:])}
    return Echo(value)
}
ZI
cat > "$work/pointer_escape.zi" <<'ZI'
Box :: struct { text: string; }
Bad :: (source: *Box) -> Box {
    value: Box = Box.{text = source.text}
    return value
}
ZI
cat > "$work/pointer_local.zi" <<'ZI'
Box :: struct { text: string; }
Read :: (source: *Box) -> s32 {
    value: Box = Box.{text = source.text}
    return cast(s32)value.text[0]
}
ZI
cat > "$work/good.zi" <<'ZI'
Box :: struct { text: string; }
Echo :: (value: Box) -> Box { return value }
#program_export
Answer :: () -> s32 {
    value: Box = Box.{text = "stable"}
    copy: Box = Echo(value)
    return cast(s32)copy.text[0]
}
ZI

for name in literal nested array field call pointer_escape; do
    if "$ziran" check --diagnostics=json --root "$work" \
        "$work/$name.zi" > "$work/$name.out" 2> "$work/$name.err"; then
        echo "$name returned a view of local storage" >&2
        exit 1
    fi
    python3 - "$work/$name.err" <<'PY'
import json
from pathlib import Path
import sys

diagnostics = [json.loads(line) for line in Path(sys.argv[1]).read_text().splitlines()]
assert any('backing storage' in item['message'] or
           'borrows local or temporary' in item['message'] for item in diagnostics), diagnostics
PY
done

"$ziran" check --root "$work" "$work/pointer_local.zi"
"$ziran" ir --root "$work" -o "$work/ir" "$work/good.zi"
"$ziran" bundle --root "$work" --entry good:Answer \
    -o "$work/source.zx" "$work/good.zi"
"$ziran" bundle --root "$work/ir" --entry good:Answer \
    -o "$work/saved.zx" "$work/ir/good.zir"
cmp "$work/source.zx" "$work/saved.zx"
test "$("$ziran" run "$work/source.zx")" = 115
