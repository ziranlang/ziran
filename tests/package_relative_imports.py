"""Explicit package paths retain canonical ownership across source and saved IR."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

repo=Path(__file__).resolve().parent.parent
tools=Path(sys.argv[1]).resolve().parent
env=os.environ.copy()
for key in ('DISPLAY','WAYLAND_DISPLAY','XAUTHORITY','DBUS_SESSION_BUS_ADDRESS'):
 env.pop(key,None)
def call(*args,contains=None):
 result=subprocess.run(list(map(str,args)),env=env,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
 if contains is None:
  assert result.returncode==0,(args,result.returncode,result.stdout)
 else:
  assert result.returncode!=0 and contains in result.stdout,(args,result.returncode,result.stdout)
 return result.stdout.strip()
def write(path,text):
 path.parent.mkdir(parents=True,exist_ok=True)
 path.write_text(text)
with tempfile.TemporaryDirectory(prefix='package-relative-imports.') as tmp:
 root=Path(tmp);app=root/'app';library=root/'library'
 main=app/'src/entry/app.zi'
 local=app/'src/one/value.zi';other=app/'src/two/value.zi'
 api=library/'src/api/module.zi';secret=library/'src/internal/value.zi';group=library/'src/Group/module.zi'
 write(local,'Get :: () -> s32 { return 19 }\n')
 write(other,'Get :: () -> s32 { return 3 }\n')
 write(secret,'Token :: struct { value: s32 }\nRead :: (token: Token) -> s32 { return token.value }\n')
 write(group,'Get :: () -> s32 { return 1 }\n')
 api_text='''Private :: #import, file "../internal/value.zi";
Canonical :: #import "secret";
Group :: #import, dir "../Group";
Get :: () -> s32 { return Private.Read(Canonical.Token.{value = 19}) + Group.Get() }
'''
 write(api,api_text)
 source='''Local :: #import, file "../one/value.zi";
Other :: #import, file "../two/value.zi";
Canonical :: #import "Other";
Library :: #import "library/Api";
#program_export
Answer :: () -> s32 { return Local.Get() + Other.Get() + Library.Get() + Canonical.Get() - 3 }
'''
 write(main,source)
 mapping=root/'map.tsv'
 rows=[['P','root',app],['P','p0000000000000001',library],['M','root','app',main],['M','root','Other',other],['M','p0000000000000001','api',api],['M','p0000000000000001','secret',secret],['M','p0000000000000001','Group',group],['D','root','library/Api','p0000000000000001','api']]
 mapping.write_text(''.join('\t'.join(map(str,row))+'\n' for row in rows))
 env['ZIRAN_PACKAGE_MAP']=str(mapping)
 checker=tools/'zi2zir'
 call(checker,'--check-only','--root',app,main)
 ir=root/'ir'
 call(checker,'--root',app,'-o',ir,main)
 for form,native_root,file in [('source',app,main),('saved',ir,ir/'app.zir')]:
  bundle=root/(form+'.zx')
  call(tools/'zi2zx','bundle','--root',native_root,'--entry','app:Answer','-o',bundle,file)
  assert call(tools/'zi2zx','run',bundle)=='42'
  assert call(tools/'zi2zx','run','--vm',bundle)=='42'
  out=root/(form+'-c')
  call(tools/'zi2c','--root',native_root,'-o',out,file)
  write(out/'driver.c','#include "app.h"\nint main(void) { return Answer() == 42 ? 0 : 1; }\n')
  call(env.get('CC','cc'),'-std=c11','-I'+str(repo/'include'),'-I'+str(out),*sorted(out.glob('*.c')),'-o',out/'program')
  call(out/'program')
 # Canonical containment is checked before reading another package's file.
 link=app/'src/linked.zi';link.symlink_to(secret)
 escapes=[('file','../../../library/src/internal/value.zi'),('file','../linked.zi'),('dir','../../../library/src/Group')]
 for kind,path in escapes:
  write(main,f'Leak :: #import, {kind} "{path}";\n')
  call(checker,'--check-only','--root',app,main,contains='explicit import must stay inside its owning package')
 # A dependency still cannot load private files absent from its declared modules.
 write(main,source)
 write(library/'src/internal/undeclared.zi','Get :: () -> s32 { return 0 }\n')
 write(api,api_text+'Hidden :: #import, file "../internal/undeclared.zi";\n')
 call(checker,'--check-only','--root',app,main,contains='source file is not a declared package module')
print('Package relative file/directory imports preserve canonical identities, source/saved C/native/VM semantics and package boundaries')
