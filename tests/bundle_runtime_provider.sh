#!/bin/sh
# Generic runtime discovery must not run an app during provider admission.
set -eu
ziran=$(realpath "${1:?pass the ziran command}")
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
python3 - "$ziran" "$repo" <<'PYTHON'
from pathlib import Path
import json,os,subprocess,sys,tempfile
ziran,repo=Path(sys.argv[1]),Path(sys.argv[2])
with tempfile.TemporaryDirectory(prefix='bundle-provider.',dir=repo/'build') as scratch:
    root=Path(scratch)
    registry=root/'hosts with spaces'; registry.mkdir()
    log=root/'calls.jsonl'
    env={**os.environ,'ZIRAN_HOST_PATH':str(registry),'PROVIDER_LOG':str(log)}
    def run(*args,code=0,environment=None):
        result=subprocess.run([str(ziran),*map(str,args)],cwd=root,env=environment or env,capture_output=True,text=True)
        assert result.returncode==code,(args,result.returncode,result.stdout,result.stderr)
        return result
    def bundle(name,body):
        source=root/(name+'.zi');source.write_text(body)
        output=root/(name+' with spaces.zx')
        run('bundle','--root',root,'--entry',name+':main','-o',output,source)
        return output
    plain=bundle('plain','main :: () -> s32 { return 42 }')
    hosted=bundle('hosted',r'''
host_api :: #system_library "host_api";
CallHost :: () -> s32 #foreign host_api;
Initialize :: () -> s32 { print("initializer executed\n"); return 1 }
initialized: s32 = Initialize();
main :: () -> s32 { print("entry executed\n"); return CallHost() + initialized }
''')
    assert run('run',plain).stdout=='42\n'
    assert not log.exists()
    missing=run('run',hosted,code=2)
    assert missing.stdout=='' and 'no installed runtime provider' in missing.stderr
    (registry/'00-relative.host').write_text('./untrusted-program\n')
    run('run',hosted,code=2)
    assert not log.exists()
    provider=r'''#!/usr/bin/python3
from pathlib import Path
import json,os,sys
with open(os.environ['PROVIDER_LOG'],'a') as output:
    output.write(json.dumps([Path(sys.argv[0]).name,*sys.argv[1:]])+'\n')
if sys.argv[1]=='--check':
    assert len(sys.argv)==3 and Path(sys.argv[2]).is_absolute()
    sys.exit(1 if Path(sys.argv[0]).name.startswith('reject') else 0)
assert Path(sys.argv[-1]).is_file()
print('provider ran')
sys.exit(int(os.environ.get('PROVIDER_STATUS','0')))
'''
    for name in ['reject provider','accepted provider','later provider']:
        path=root/name;path.write_text(provider);path.chmod(0o755)
    (registry/'10-reject.host').write_text(str(root/'reject provider')+'\n')
    (registry/'20-accept.host').write_text(str(root/'accepted provider')+'\n')
    (registry/'30-later.host').write_text(str(root/'later provider')+'\n')
    def calls(): return [json.loads(line) for line in log.read_text().splitlines()]
    assert run('run',hosted).stdout=='provider ran\n'
    assert calls()==[['reject provider','--check',str(hosted)],['accepted provider','--check',str(hosted)],['accepted provider',str(hosted)]]
    log.unlink()
    assert run('run','--vm',hosted).stdout=='provider ran\n'
    assert calls()[-1]==['accepted provider','--vm',str(hosted)]
    log.unlink()
    run('run',hosted,code=23,environment={**env,'PROVIDER_STATUS':'23'})
    assert len(calls())==3 and calls()[-1][0]=='accepted provider'
    log.unlink()
    malformed=root/'bad.zx';malformed.write_bytes(b'ZX broken')
    run('run',malformed,code=1)
    assert not log.exists(), 'invalid bundles must never reach providers'
    assert run('run','--vm',plain).stdout=='42\n'
    assert not log.exists(), 'plain bundles must not consult providers'
    data=root/'user:data with spaces'; installed=data/'ziran/hosts';installed.mkdir(parents=True)
    (installed/'sample.host').write_text(str(root/'accepted provider')+'\n')
    xdg={**env,'ZIRAN_HOST_PATH':'','XDG_DATA_HOME':str(data),'XDG_DATA_DIRS':str(root/'empty-system')}
    assert run('run',hosted,environment=xdg).stdout=='provider ran\n'
    assert calls()[-1]==['accepted provider',str(hosted)]
print('bundle runtime provider tests passed')
PYTHON
