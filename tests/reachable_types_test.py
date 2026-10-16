import importlib.util
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
ziran, work = (Path(value).resolve() for value in sys.argv[1:])
work.mkdir()
env = {k:v for k,v in os.environ.items() if k not in ['DISPLAY','WAYLAND_DISPLAY','XAUTHORITY','DBUS_SESSION_BUS_ADDRESS']}
env.update(YUE_DESKTOP_RECOVERY='0',GOTOOLCHAIN='local',GOPROXY='off',GOSUMDB='off',GOWORK='off')


def run(argv, expected=None, cwd=ROOT):
    result = subprocess.run(list(map(str,argv)),cwd=cwd,env=env,capture_output=True,text=True,timeout=180)
    assert result.returncode == 0, (list(map(str,argv[:8])),len(argv),result.stdout[-2000:],result.stderr[-6000:])
    if expected is not None: assert result.stdout == expected, (argv,result.stdout,expected)
    return result


spec = importlib.util.spec_from_file_location('header_graphs',ROOT/'bench/native_header_graphs.py')
fixture = importlib.util.module_from_spec(spec);spec.loader.exec_module(fixture)
source = work/'source';fixture.generate(source)
for path in source.glob('consumer*.zi'):
    text = path.read_text();old = 'cast(s64)(value.g0.l0.value)'
    assert text.count(old) == 1
    path.write_text(text.replace(old,'cast(s64)(value.g0.l0.value * 2 + value.g7.l63.value)'))
imports = '#scope_export\nCommon :: #import "common";\n'+''.join(f'C{i} :: #import "consumer{i:02d}";\n' for i in range(fixture.CONSUMERS))
body = ('#program_export\nAnswer :: (seed: s64) -> s64 {\nvalue: Common.Bundle\n'
        'value.g0.l0.value = cast(u32)(seed)\nresult := cast(s64)(Prepare(*value, cast(u32)(seed)))\n')
body += ''.join(f'result += C{i}.Read(*value)\n' for i in range(fixture.CONSUMERS))
body += 'return result\n}\n#program_export\nMain :: () -> s64 { return Answer(7) + Answer(9) }\n'
body += '#program_export\nCheck :: () -> s32 { if Answer(7) != 12682 { return 1 }; if Answer(9) != 15852 { return 1 }; return 0 }\n'
prepare = 'Prepare :: (value: *Common.Bundle, seed: u32) -> u32 { value.g7.l63.value = seed + 3; return value.g7.l63.value }\n'
(source/'app.zi').write_text(imports+prepare+body)
oracle = lambda seed: (3*seed+3)*fixture.CONSUMERS*(fixture.CONSUMERS+1)//2+seed+3
expected = str(oracle(7)+oracle(9))+'\n'
run([ziran,'ir','--root',source,'-o',work/'ir',source/'app.zi'])
for form in ['source','saved']:
    root = source if form == 'source' else work/'ir'
    entry = root/('app.zi' if form == 'source' else 'app.zir')
    bundle = work/(form+'.zib')
    run([ziran,'bundle','--root',root,'--entry','app:Main','-o',bundle,entry])
    run([ziran,'run',bundle],expected)
    for target in ['c','cpp','go','py','rust','plan9-c']:
        output = work/(form+'-'+target)
        command = [ziran,'build','--target='+target,'--root',root,'-o',output,entry]
        if target == 'go': command[2:2] = ['--pkg','main']
        run(command)
        if target in ['c','cpp']:
            suffix,header,compiler,standard = ('c','h',os.environ.get('CC','cc'),'c11') if target == 'c' else ('cpp','hpp',os.environ.get('CXX','c++'),'c++17')
            host = output/('driver.'+suffix)
            host.write_text('#include "app.'+header+'"\nint main(void) { return Answer(7) == '+str(oracle(7))+' && Answer(9) == '+str(oracle(9))+' ? 0 : 1; }\n')
            run([compiler,'-O2','-std='+standard,'-I'+str(ROOT/'include'),'-I'+str(output),*sorted(output.glob('*.'+suffix)),'-o',output/'run'])
            run([output/'run'])
        elif target == 'go':
            (output/'main.go').write_text('package main\nfunc main() { if App_Answer(7) != '+str(oracle(7))+' || App_Answer(9) != '+str(oracle(9))+' { panic("late record fields") } }\n')
            run(['go','build','-o',output/'run',*sorted(output.glob('*.go'))])
            run([output/'run'])
        elif target == 'py':
            run(['python3','-c','import runpy; app=runpy.run_path("__init__.py"); assert app["Answer"](7)=='+str(oracle(7))+' and app["Answer"](9)=='+str(oracle(9))],cwd=output)
        elif target == 'rust' and shutil.which('cargo'):
            (output/'src/main.rs').write_text('fn main() { assert_eq!(ziran_generated::Answer(7), '+str(oracle(7))+'); assert_eq!(ziran_generated::Answer(9), '+str(oracle(9))+'); }\n')
            run(['cargo','run','--quiet','--offline','--manifest-path',output/'Cargo.toml'])
    print('Late record fields passed '+form+' VM/bundle and generated-target checks',flush=True)
assert (work/'source.zib').read_bytes() == (work/'saved.zib').read_bytes()

# A foreign signature keeps every field in the ABI. Check its complete layout
# separately from the portable program's legitimate unused-field pruning.
(source/'app.zi').write_text(imports+'host :: #system_library "host";\nPrepare :: (value: *Common.Bundle, seed: u32) -> u32 #foreign host "prepare";\n'+body)
run([ziran,'ir','--root',source,'-o',work/'foreign-ir',source/'app.zi'])
for form in ['source','saved']:
    root = source if form == 'source' else work/'foreign-ir'
    entry = root/('app.zi' if form == 'source' else 'app.zir')
    output = work/('foreign-'+form);output.mkdir()
    (output/'host.c').write_text(fixture.host_source())
    run([ziran,'build','--target=c','--exe','--entry','app:Check','--root',root,'-o',output,entry])
    run([output/'app'])
print('554-module late fields preserve source/saved execution and the complete 2048-byte foreign ABI',flush=True)
