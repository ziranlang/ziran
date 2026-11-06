#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${1:-"$repo/build/bin/ziran"}
work="$repo/build/tests/semantic-edit"
mkdir -p "$work/tmp"
export TMPDIR="$work/tmp" ZIRAN_DESKTOP_RECOVERY=0 HARMONY_DESKTOP_RECOVERY=0 YUE_DESKTOP_RECOVERY=0
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
python3 - "$ziran" "$work" <<'PY'
import hashlib,json,os,pathlib,subprocess,sys
ziran=str(pathlib.Path(sys.argv[1]).resolve()); work=pathlib.Path(sys.argv[2]); root=work/'source'; root.mkdir(exist_ok=True)
source=root/'main.zi'; request=work/'request.json'
base=b'#program_export\nAnswer :: () -> s32 { return 41 }\n'
def reset(data=base): source.write_bytes(data); source.chmod(0o640)
def span(old='41',new='42',data=None):
 data=source.read_bytes() if data is None else data; at=data.index(old.encode())
 return {'start_byte':at,'end_byte':at+len(old.encode()),'original':old,'replacement':new}
def patch(edits=None,**extra):
 return dict(schema_version=1,path='main.zi',document_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),edits=edits or [span()],**extra)
def run(p,apply=False,ok=True,options=(),edit_root=None):
 before=source.read_bytes()
 request.write_text(json.dumps(p) if not isinstance(p,str) else p)
 r=subprocess.run([ziran,'edit','--root',str(root if edit_root is None else edit_root),'--diagnostics=json',*options,*(['--apply'] if apply else []),str(request)],capture_output=True,timeout=30)
 assert (r.returncode==0)==ok,(p,r.returncode,r.stdout,r.stderr)
 if not ok:
  assert source.read_bytes()==before and not r.stdout
  rows=[json.loads(line) for line in r.stderr.splitlines()]; assert rows and all(x['schema_version']==2 for x in rows),r.stderr
  assert not list(root.glob('.zi-edit-*'))
  return None
 result=json.loads(r.stdout); assert result['schema_version']==1 and result['applied']==apply
 if apply: assert source.read_text()==result['source']
 else: assert source.read_bytes()==before
 return result
reset(); p=patch(); preview=run(p); run(p,True); assert '42' in source.read_text() and source.stat().st_mode&0o777==0o640
run(p,True,False) # stale hash
for bad in (0,2,True,'1'):
 reset(); p=patch(); p['schema_version']=bad; run(p,True,False)
reset(); run('{"schema_version":1,"schema_version":1,"path":"main.zi","document_sha256":"x","edits":[]}',True,False)
for field,value in [('path','../main.zi'),('path','/main.zi'),('path','./main.zi'),('document_sha256','0'*64),('unknown',0)]:
 reset(); p=patch(); p[field]=value; run(p,True,False)
reset(); bad=span(); bad['original']='40'; run(patch([bad]),True,False)
reset(); a=span(); run(patch([a,a]),True,False)
reset(); a=span(); a['end_byte']=a['start_byte']; a['original']=''; run(patch([a,a]),True,False)
reset(); a=span(); a['start_byte']=-1; run(patch([a]),True,False)
reset(); a=span(); a['end_byte']=2**64; run(patch([a]),True,False)
for replacement in ('"wrong"','???','41',' 41 '):
 reset(); run(patch([span(new=replacement)]),True,False)
for import_text in ('#import, file "../outside.zi"\n','#import, file "/outside.zi"\n','#load "../outside.zi"\n'):
 reset(); run(patch([{'start_byte':0,'end_byte':0,'original':'','replacement':import_text},span()]),True,False)
# Byte columns must not divide a multibyte code point.
reset(b'// \xc3\xa9\n'+base); run(patch([dict(start_byte=4,end_byte=4,original='',replacement='x')]),True,False)
# Canonical path aliases (including a filesystem-root source root) must still
# validate the candidate snapshot, never silently fall back to old disk bytes.
reset(); p=patch(); p['path']=str(source.resolve()).lstrip('/')
run(p,True,edit_root='/')
# Multiple independent edits are resolved in original byte coordinates.
reset(base+b'Other :: () -> s32 { return 7 }\n'); run(patch([span('7','8'),span()]),True)
# Collision in the declaration table must be rejected before replacement.
reset(base+b'Other :: () -> s32 { return 7 }\n'); run(patch([span('Other','Answer')]),True,False)
# IDs come from the checked API; header anchors are not treated as full bodies.
reset(); api=subprocess.run([ziran,'api','--json','--root',str(root),'--symbol','Answer',str(source)],capture_output=True,check=True)
identity=json.loads(api.stdout)['modules'][0]['functions'][0]['id']
old='Answer :: () -> s32 { return 41 }'
run(patch([dict(semantic_id=identity,original=old,replacement='Answer :: () -> s32 { return 43 }')]),True)
reset(); run(patch([dict(semantic_id=identity,original='Answer',replacement='Other')]),True,False)
reset(); run(patch([dict(semantic_id='zi:'+'0'*64,original=old,replacement=old.replace('41','42'))]),True,False)
# Comment braces must never masquerade as declaration terminators.
commented='Answer :: () -> s32 {\n    // }\n    /* } */\n    return 41;\n}'
reset(('#program_export\n'+commented+'\n').encode())
api=subprocess.run([ziran,'api','--json','--root',str(root),'--symbol','Answer',str(source)],capture_output=True,check=True)
comment_id=json.loads(api.stdout)['modules'][0]['functions'][0]['id']
run(patch([dict(semantic_id=comment_id,original=commented,replacement='Answer :: () -> s32 { return 44 }')]),True)
# A broken document can be repaired with exact span edits; declaration edits
# require a checked initial graph and are rejected if it is unavailable.
reset(base.replace(b'41',b'"bad"')); run(patch([span('"bad"','42')]),True)
# Imports resolve through the real loader inside an in-memory overlay.
(root/'library.zi').write_text('Value :: () -> s32 { return 6 }\n')
reset(b'#import "library"\n'+base); run(patch([span(new='Value()')]),True)
# A source symlink never lets validation escape the selected root.
outside=work/'outside.zi'; outside.write_text('Value :: () -> s32 { return 9 }\n')
link=root/'escape.zi'
if link.is_symlink(): link.unlink()
link.symlink_to(outside.resolve())
reset(); run(patch([{'start_byte':0,'end_byte':0,'original':'','replacement':'#import "escape"\n'},span(new='Value()')]),True,False)
# The same canonical checker enforces declared laws during edit validation.
reset(base+b'#law AnswerValue custom Answer() == 41;\n')
run(patch(),True,False)
# Explicit dependency roots are read through the same resolver and guard.
dep=work/'dependency'; dep.mkdir(exist_ok=True); (dep/'external.zi').write_text('External :: () -> s32 { return 8 }\n')
reset(b'#import "external"\n'+base)
run(patch([span(new='External()')]),True,False)
run(patch([span(new='External()')]),True,options=('--module-path',str(dep)))
reset(b'#import "std/vec"\n'+base); run(patch(),True)
# Updated source still checks and executes from source and checked saved IR.
source.write_bytes(source.read_bytes()+b'main :: () { print("%\\n", Answer()); }\n')
program=root/'main.zi'; output=work/'ir'
subprocess.run([ziran,'ir','--root',str(root),'-o',str(output),str(program)],check=True,capture_output=True)
for entry in (program,output/'main.zir'):
 out=work/('saved.zx' if entry.suffix=='.zir' else 'source.zx')
 subprocess.run([ziran,'bundle','--root',str(entry.parent),'--entry','main:main','-o',str(out),str(entry)],check=True,capture_output=True)
 vm=pathlib.Path(ziran).with_name('zi2zx')
 result=subprocess.run([str(vm),'run','--vm',str(out)],capture_output=True,timeout=30)
 assert result.returncode==0 and result.stdout==b'42\n',(entry,result.returncode,result.stderr)
print('semantic edit: preview/apply, declaration IDs, rollback, paths, imports, UTF-8, stale versions PASS')
PY
"${CC:-cc}" ${VM_CFLAGS:-} -std=c11 -D_GNU_SOURCE -I"$repo/include" -I"$repo/cmd/zir" "$repo/tests/semantic_edit_runtime.c" "$repo/cmd/zir/zir_edit.c" "$(dirname "$ziran")/../libziran.a" -Wl,--wrap=renameat -Wl,--wrap=fstat -lm -pthread -ldl -o "$work/runtime"
(cd "$work/source" && ../runtime) 2> "$work/runtime.err"
test -z "$(find "$work/source" -maxdepth 1 -name '.zi-edit-*' -print -quit)"
