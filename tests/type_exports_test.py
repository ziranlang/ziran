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
    assert result.returncode == 0, (list(map(str,argv[:8])),result.stdout[-2000:],result.stderr[-6000:])
    if expected is not None: assert result.stdout == expected, (result.stdout,expected)
    return result


source = work/'source';source.mkdir()
for index in range(64):
    text = '#scope_export\n'
    if index < 63: text += f'using Next :: #import "hop{index+1}";\n'
    else: text += 'using Cycle :: #import "hop0";\nValue :: struct { number: s64 }\n'
    (source/f'hop{index}.zi').write_text(text)
(source/'app.zi').write_text('#import "hop0";\nSurface :: #import "hop0";\n'
    '#program_export\nAnswer :: (seed: s64) -> s64 {\na: Value\nb: Surface.Value\n'
    'a.number = seed * 3 + 5\nb.number = seed + 1\nsaved := a\na = b\nreturn saved.number + a.number\n}\n'
    '#program_export\nMain :: () -> s64 { return Answer(7) + Answer(9) }\n'
    '#program_export\nCheck :: () -> s32 { if Answer(7) != 34 { return 1 }; if Answer(9) != 42 { return 1 }; return 0 }\n')
run([ziran,'ir','--root',source,'-o',work/'ir',source/'app.zi'])
for form in ['source','saved']:
    root = source if form == 'source' else work/'ir'
    entry = root/('app.zi' if form == 'source' else 'app.zir')
    bundle = work/(form+'.zx')
    run([ziran,'bundle','--root',root,'--entry','app:Main','-o',bundle,entry])
    run([ziran,'run',bundle],'76\n')
    for target in ['c','go','py']:
        output = work/(form+'-'+target)
        command = [ziran,'build','--target='+target,'--root',root,'-o',output,entry]
        if target == 'go': command[2:2] = ['--pkg','main']
        run(command)
        if target == 'c':
            (output/'driver.c').write_text('#include "app.h"\nint main(void) { return Answer(7) == 34 && Answer(9) == 42 ? 0 : 1; }\n')
            run([os.environ.get('CC','cc'),'-O2','-std=c11','-I'+str(ROOT/'include'),'-I'+str(output),*sorted(output.glob('*.c')),'-o',output/'run'])
            run([output/'run'])
        elif target == 'go':
            (output/'main.go').write_text('package main\nfunc main() { if App_Answer(7) != 34 || App_Answer(9) != 42 { panic("deep exported types") } }\n')
            run(['go','build','-o',output/'run',*sorted(output.glob('*.go'))]);run([output/'run'])
        elif target == 'py':
            run(['python3','-c','import runpy; app=runpy.run_path("__init__.py"); assert app["Answer"](7)==34 and app["Answer"](9)==42'],cwd=output)
    print('Deep exported types passed '+form+' VM/bundle and generated-target checks',flush=True)
assert (work/'source.zx').read_bytes() == (work/'saved.zx').read_bytes()

# Private import edges are available inside their file/module, without
# exporting their opened names into a caller's namespace.
for scope in ['module','file']:
    case = work/('private-'+scope);case.mkdir()
    (case/'leaf.zi').write_text('Value :: struct { number: s64 }\n')
    (case/'bridge.zi').write_text('#scope_'+scope+'\nusing Hidden :: #import "leaf";\n'
        '#scope_export\nRead :: () -> s64 { item: Value; item.number = 42; return item.number }\n')
    (case/'good.zi').write_text('Bridge :: #import "bridge";\n#program_export\nAnswer :: () -> s64 { return Bridge.Read() }\n')
    (case/'bad.zi').write_text('#import "bridge";\nBad :: (item: Value) -> s64 { return item.number }\n')
    run([ziran,'bundle','--root',case,'--entry','good:Answer','-o',case/'good.zx',case/'good.zi'])
    run([ziran,'run',case/'good.zx'],'42\n')
    rejected = subprocess.run([str(ziran),'check','--root',str(case),str(case/'bad.zi')],env=env,capture_output=True,text=True,timeout=60)
    assert rejected.returncode != 0, 'private '+scope+' using import leaked its type'

ambiguous = work/'ambiguous';ambiguous.mkdir()
for name in ['left','right']:(ambiguous/(name+'.zi')).write_text('Value :: struct { number: s64 }\n')
(ambiguous/'facade.zi').write_text('using Left :: #import "left";\nusing Right :: #import "right";\n')
(ambiguous/'app.zi').write_text('#import "facade";\n#import "left";\nBad :: (item: Value) -> s64 { return item.number }\n')
rejected = subprocess.run([str(ziran),'check','--root',str(ambiguous),str(ambiguous/'app.zi')],env=env,capture_output=True,text=True,timeout=60)
assert rejected.returncode != 0, 'an ambiguous facade was ignored in favor of a direct import'
print('Public re-exports preserve source/saved runtime values, private boundaries and nested ambiguity',flush=True)
