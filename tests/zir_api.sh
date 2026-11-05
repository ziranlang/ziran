#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${1:-"$repo/build/bin/ziran"}
work="$repo/build/tests/zir-api"
mkdir -p "$work/tmp"
export TMPDIR="$work/tmp" ZIRAN_DESKTOP_RECOVERY=0 HARMONY_DESKTOP_RECOVERY=0 YUE_DESKTOP_RECOVERY=0
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS XDG_RUNTIME_DIR SESSION_MANAGER XDG_SESSION_ID XDG_SESSION_TYPE DESKTOP_SESSION SSH_AUTH_SOCK
cat > "$work/library.zi" <<'ZI'
#import "vec"
TEXT :: "é";
Box :: struct { bytes: Vec(u8); }
Take :: (values: Vec(u8), view: []u8, pointer: *s32) -> Vec(u8) {
    return values
}
Pure :: () -> s32 { return 42 }
#law PureEffect effect Pure == pure;
#if #defined(EXTRA) {
Extra :: () -> s32 { return 9 }
}
#scope_module
Hidden :: () -> s32 { return 0 }
#law HiddenEffect effect Hidden == pure;
ZI
cat > "$work/main.zi" <<'ZI'
#import "library"
Answer :: () -> s32 { return Pure() }
ZI
"$ziran" api --json --root "$work" --module-path "$repo/std" "$work/main.zi" > "$work/source.json"
"$ziran" api --json --root "$work" --module-path "$repo/std" "$work/main.zi" > "$work/repeated.json"
cmp "$work/source.json" "$work/repeated.json"
"$ziran" ir --root "$work" --module-path "$repo/std" -o "$work/ir" "$work/main.zi"
"$ziran" api --json --root "$work/ir" "$work/ir/main.zir" > "$work/saved.json"
"$ziran" api --json --module library --symbol Take --root "$work" --module-path "$repo/std" "$work/main.zi" > "$work/scoped.json"
"$ziran" api --json --imports-only --root "$work" --module-path "$repo/std" "$work/main.zi" > "$work/imports.json"
"$ziran" api --json --max-symbols 2 --root "$work" --module-path "$repo/std" "$work/main.zi" > "$work/limited.json"
"$ziran" api --json --module library --max-bytes 2048 --root "$work" --module-path "$repo/std" "$work/main.zi" > "$work/bytes.json"
"$ziran" api --json --define EXTRA --root "$work" --module-path "$repo/std" "$work/main.zi" > "$work/conditional.json"
python3 - "$work" <<'PY'
import hashlib, json, pathlib, sys
root=pathlib.Path(sys.argv[1])
get=lambda n: json.loads((root/(n+'.json')).read_bytes())
a,b=get('source'),get('saved')
rows=lambda v: [(m,r) for m in v['modules'] for k in ('imports','constants','globals','types','functions','laws') for r in m[k]]
assert a['schema_version']==b['schema_version']==1
assert a['truncation']['complete']
assert {r['id'] for _,r in rows(a)}=={r['id'] for _,r in rows(b)}
assert not any(r['symbol_kind']!='law' and r['name'].startswith(('Hidden','__','zi_default_')) for _,r in rows(a))
assert all(r['visibility']=='module_contract' for m in a['modules'] for r in m['laws'])
assert all(not r['document']['available'] and r['document']['sha256'] is None for _,r in rows(b))
for m,r in rows(a):
    if m['name']=='vec': continue
    doc=r['document']; assert doc['available']
    data=(root/r['span']['path']).read_bytes()
    assert doc['sha256']==hashlib.sha256(data).hexdigest() and doc['bytes']==len(data)
    span=r['span']; assert span['kind']=='declaration_anchor'
    assert 0<=span['start_byte']<=span['end_byte']<=len(data)
scope=get('scoped'); assert [m['name'] for m in scope['modules']]==['library']
take=scope['modules'][0]['functions'][0]
assert take['name']=='Take' and take['ownership']=={'result':'move','parameters':[{'name':'values','mode':'move'},{'name':'view','mode':'borrow'},{'name':'pointer','mode':'borrow'}]}
assert take['examples'] is None
(root/'symbol-id').write_text(take['id'])
assert len(rows(get('limited')))==2 and not get('limited')['truncation']['complete']
assert len((root/'bytes.json').read_bytes())<=2048 and get('bytes')['truncation']['byte_limit']
assert all(k=='imports' or not m[k] for m in get('imports')['modules'] for k in ('imports','constants','globals','types','functions','laws'))
assert any(r['name']=='Extra' for _,r in rows(get('conditional')))
assert not any(r['name']=='Extra' for _,r in rows(a))
extra=next(r for _,r in rows(get('conditional')) if r['name']=='Extra')
data=(root/extra['span']['path']).read_bytes(); start=extra['span']['start_byte']
assert data[start:start+5]==b'Extra'
assert next(r for _,r in rows(a) if r['name']=='PureEffect')['status']=='proved'
PY
"$ziran" api --json --symbol "$(cat "$work/symbol-id")" --root "$work" --module-path "$repo/std" "$work/main.zi" > "$work/by-id.json"
python3 - "$work/by-id.json" <<'PY'
import json,sys
v=json.load(open(sys.argv[1])); assert v['truncation']['matched_symbols']==1
PY
# Positions may change without changing semantic identities.
sed -i '1i// leading comment' "$work/library.zi"
"$ziran" api --json --module library --symbol Take --root "$work" --module-path "$repo/std" "$work/main.zi" > "$work/moved.json"
python3 - "$work" <<'PY'
import json,pathlib,sys
p=pathlib.Path(sys.argv[1]); a=json.loads((p/'scoped.json').read_bytes())['modules'][0]['functions'][0]; b=json.loads((p/'moved.json').read_bytes())['modules'][0]['functions'][0]
assert a['id']==b['id'] and a['span']['line']+1==b['span']['line'] and a['document']['sha256']!=b['document']['sha256']
PY
mkdir -p "$work/many"
: > "$work/many/main.zi"
i=0
while test "$i" -lt 12; do
    printf '#import "mod%s"\n' "$i" >> "$work/many/main.zi"
    printf 'VALUE_%s :: %s;\n' "$i" "$i" > "$work/many/mod$i.zi"
    i=$((i + 1))
done
printf 'Answer :: () -> s32 { return 42 }\n' >> "$work/many/main.zi"
if "$ziran" api --json --max-bytes 1024 --root "$work/many" "$work/many/main.zi" > "$work/framing.out" 2> "$work/framing.err"; then exit 1; fi
test ! -s "$work/framing.out"
rg -q 'API framing exceeds' "$work/framing.err"
for invalid in 0 -1 1x 99999999999999999999999999; do
    if "$ziran" api --json --max-bytes "$invalid" --root "$work" "$work/main.zi" > "$work/invalid.out" 2> "$work/invalid.err"; then exit 1; fi
    test ! -s "$work/invalid.out"
done
cat > "$work/rewritten.zi" <<'ZI'
TEXT :: #string RAW
é
RAW; After :: () -> s32 { return 7 }
Answer :: () -> s32 { return 42 }
ZI
"$ziran" api --json --root "$work" "$work/rewritten.zi" > "$work/rewritten.json"
python3 - "$work/rewritten.json" <<'PY2'
import json,sys
v=json.load(open(sys.argv[1])); m=v['modules'][0]
assert m['document']['rewritten']
assert {r['name'] for r in m['functions']}=={'After','Answer'}
for k in ('constants','functions'):
    for r in m[k]:
        assert r['span']['coordinate_space']=='rewritten_source'
        assert r['span']['start_byte'] is None and r['span']['end_byte'] is None
PY2
"${CC:-cc}" ${VM_CFLAGS:-} -std=c11 -D_GNU_SOURCE -I"$repo/include" -I"$repo/cmd/zir" "$repo/tests/zir_api_document.c" "$(dirname "$ziran")/../libziran.a" -lm -pthread -ldl -o "$work/documents"
"$work/documents"
echo 'zir api: scoped checked retrieval, stable identities, captured document hashes/UTF8 spans, saved-IR unknown bytes, ownership/laws, deterministic complete bounded JSON passed'
