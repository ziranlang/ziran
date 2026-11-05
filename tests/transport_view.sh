#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${1:-"$repo/build/bin/ziran"}
work="$repo/build/tests/transport-view"
mkdir -p "$work/tmp"
export TMPDIR="$work/tmp" ZIRAN_DESKTOP_RECOVERY=0 HARMONY_DESKTOP_RECOVERY=0 YUE_DESKTOP_RECOVERY=0
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
python3 - "$ziran" "$work" "$repo" <<'PY'
import hashlib,json,pathlib,subprocess,sys
ziran=str(pathlib.Path(sys.argv[1]).resolve()); work=pathlib.Path(sys.argv[2]); repo=pathlib.Path(sys.argv[3])
def run(action,path,*flags,ok=True):
 r=subprocess.run([ziran,action,'--diagnostics=json',*map(str,flags),str(path)],capture_output=True,timeout=30)
 assert (r.returncode==0)==ok,(action,path,r.returncode,r.stderr)
 if not ok:
  assert not r.stdout
  assert all(x['schema_version']==2 for x in map(json.loads,r.stderr.splitlines()))
 return r.stdout
fixture=work/'canonical.zi'
fixture.write_text('''// é 🙂 ~ repeat transport preserves all payloads
#program_export
Answer :: () -> s32 {
    if true {
        text := "        ~~🙂"
        return 42
    }
    return 0
}
''')
subprocess.run([ziran,'fmt',str(fixture)],check=True,capture_output=True)
original=fixture.read_bytes(); map_path=work/'map.json'; encoded=run('compact',fixture,'--map',map_path)
compact=work/'source.zic'; compact.write_bytes(encoded)
assert run('expand',compact,'--map',work/'expanded-map.json')==original
assert json.loads(map_path.read_bytes())==json.loads((work/'expanded-map.json').read_bytes())
assert run('compact',fixture)==encoded and fixture.read_bytes()==original
mapping=json.loads(map_path.read_bytes()); assert mapping['schema_version']==1
assert mapping['source_sha256']==hashlib.sha256(original).hexdigest() and mapping['transport_sha256']==hashlib.sha256(encoded).hexdigest()
assert mapping['source_bytes']==len(original) and mapping['transport_bytes']==len(encoded)
a=0;b=mapping['header_bytes']; segments=mapping['segments']
for part in segments:
 assert (part['source_start'],part['transport_start'])==(a,b)
 a=part['source_end']; b=part['transport_end']
 raw=original[part['source_start']:a]; packed=encoded[part['transport_start']:b]
 if part['kind']=='literal': assert raw==packed
 elif part['kind']=='escape': assert raw==b'~' and packed==b'~~'
 else:
  byte={ord('s'):32,ord('t'):9,ord('n'):10,ord('r'):13}[packed[1]]
  assert raw==bytes([byte])*int(packed[2:-1])
assert a==len(original) and b==len(encoded)
# Every declaration, statement, literal and trivia byte is covered, including
# a diagnostic span after the expanded source is checked.
assert all(any(p['source_start']<=offset<p['source_end'] for p in segments) for offset in range(len(original)))
for bad in (b'',b'ZI-COMPACT-0\n',b'ZI-COMPACT-1\n~',b'ZI-COMPACT-1\n~x9;',b'ZI-COMPACT-1\n~s0;',b'ZI-COMPACT-1\n~s08;',b'ZI-COMPACT-1\n~s7;',b'ZI-COMPACT-1\n~s99999999999999999999999999;',b'ZI-COMPACT-1\n~s16777217;',b'ZI-COMPACT-1\na~s8;~s8;\n'):
 compact.write_bytes(bad); run('expand',compact,ok=False)
# Source formatting policy remains the ordinary formatter's policy.
unformatted=work/'unformatted.zi'; unformatted.write_text('Answer::()->s32{return 42}\n')
run('compact',unformatted,ok=False)
# Empty, CRLF, multiline raw strings and byte-heavy real modules round-trip.
for index,data in enumerate((b'',b'Answer :: () -> s32 { return 42 }\r\n',b'TEXT :: #string RAW\n        ~~~~ payload\nRAW;\n', (repo/'std/vec.zi').read_bytes(),(repo/'cmd/compiler_expression.zi').read_bytes())):
 path=work/f'case{index}.zi'; path.write_bytes(data)
 subprocess.run([ziran,'fmt',str(path)],check=True,capture_output=True)
 expected=path.read_bytes(); compact.write_bytes(run('compact',path)); assert run('expand',compact)==expected
# Mapping output cannot overwrite canonical source through a symlink/hardlink.
linked=work/'linked.json'
if linked.is_symlink() or linked.exists(): linked.unlink()
linked.symlink_to(fixture.resolve()); run('compact',fixture,'--map',linked,ok=False); assert fixture.read_bytes()==original
linked.unlink(); linked.hardlink_to(fixture); run('compact',fixture,'--map',linked,ok=False); assert fixture.read_bytes()==original; linked.unlink()
# Diagnostics still address canonical source bytes after exact expansion.
broken=work/'broken.zi'; broken.write_bytes(original.replace(b'return 42',b'return "bad"'))
compact.write_bytes(run('compact',broken,'--map',map_path)); restored=run('expand',compact); broken.write_bytes(restored)
r=subprocess.run([ziran,'check','--root',str(work),'--diagnostics=json',str(broken)],capture_output=True)
assert r.returncode and json.loads(r.stderr.splitlines()[0])['document']['bytes']==len(restored)
print('transport: exact roundtrip, idempotence, malformed/overflow rejection, complete UTF-8 maps and canonical formatter policy PASS')
PY
