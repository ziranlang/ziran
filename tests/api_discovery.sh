#!/bin/sh
set -eu

ziran=${1:?pass the ziran command}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/library.zi" <<'ZI'
Choice :: struct {
    value: s32;
}

Visible :: (value: s32 = 42) -> Choice {
    result: Choice
    result.value = value
    return result
}

#scope_module
Hidden :: () -> s32 { return 9 }
HIDDEN_CONSTANT :: 3;
hidden_global: s32 = 4;

#scope_export
Again :: () -> s32 { return Hidden() }
PUBLIC_CONSTANT :: 5;
public_global: s32 = 6;
host_api :: #system_library "host_api";
CallHost :: (value: s32) -> s32 #foreign host_api;
Handler :: #type (value: s32) -> bool;
FEATURE :: #defined(HAS_EXTRA)
#if FEATURE {
Extra :: () -> s32 { return 1 }
}
ZI
cat > "$work/main.zi" <<'ZI'
#import "library"

Answer :: () -> s32 {
    value: Choice = Visible(42)
    return value.value + Again() - 9
}
ZI

"$ziran" api --json --root "$work" "$work/main.zi" > "$work/source.json"
"$ziran" api --json --define HAS_EXTRA --root "$work" \
    "$work/main.zi" > "$work/defined.json"
"$ziran" api --root "$work" "$work/main.zi" > "$work/source.txt"
"$ziran" ir --root "$work" -o "$work/ir" "$work/main.zi"
"$ziran" api --json --root "$work/ir" "$work/ir/main.zir" > "$work/saved.json"

python3 - "$work/source.json" "$work/saved.json" "$work/source.txt" \
    "$work/defined.json" <<'PY'
import json
from pathlib import Path
import sys

source, saved = (json.loads(Path(path).read_text()) for path in sys.argv[1:3])
assert source['schema_version'] == saved['schema_version'] == 1
for data in (source, saved):
    modules = {module['name']: module for module in data['modules']}
    assert set(modules) == {'main', 'library'}
    library = modules['library']
    functions = {function['name']: function for function in library['functions']}
    assert set(functions) == {'Visible', 'Again'}
    assert functions['Visible']['parameters'] == 'value: s32'
    assert functions['Visible']['defaults']
    assert functions['Visible']['return_type'] == 'Choice'
    assert functions['Visible']['effect'] == 'pure'
    types = {item['name']: item for item in library['types']}
    assert set(types) == {'Choice', 'Handler'}
    assert not types['Choice']['procedure'] and types['Choice']['return_type'] == ''
    assert types['Handler']['procedure'] and types['Handler']['return_type'] == 'bool'
    assert types['Handler']['body'] == 'value: s32'
    assert {item['name'] for item in library['constants']} == {'PUBLIC_CONSTANT'}
    assert {item['name'] for item in library['globals']} == {'public_global'}
    assert library['globals'][0]['type'] == 's32'
    assert {item['kind'] for item in modules['main']['imports']} == {'open'}
    foreign = library['imports']
    assert len(foreign) == 1 and foreign[0]['kind'] == 'foreign'
    assert foreign[0]['name'] == 'CallHost'
    assert foreign[0]['foreign_target'] == 'host'
    assert foreign[0]['parameters'] == 'value: s32'
    assert {item['name'] for item in modules['main']['functions']} == {'Answer'}
text = Path(sys.argv[3]).read_text()
assert 'Visible :: (value: s32 = 42) -> Choice' in text
assert 'Hidden' not in text
assert 'HIDDEN_CONSTANT' not in text
assert 'hidden_global' not in text
assert 'PUBLIC_CONSTANT' in text
assert 'public_global' in text
defined = json.loads(Path(sys.argv[4]).read_text())
library = next(module for module in defined['modules'] if module['name'] == 'library')
assert {fn['name'] for fn in library['functions']} == {'Visible', 'Again', 'Extra'}
PY

if "$ziran" api --root "$work" "$work/missing.zi" > "$work/out" 2> "$work/err"; then
    echo 'api accepted a missing source file' >&2
    exit 1
fi
