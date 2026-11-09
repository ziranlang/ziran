from pathlib import Path
import os, shutil, subprocess, sys, tempfile
repo = Path(__file__).resolve().parents[1]
ziran = Path(sys.argv[1]).resolve()
env = {k:v for k,v in os.environ.items() if k not in ('DISPLAY','WAYLAND_DISPLAY')}
def run(*args, ok=True):
    p = subprocess.run(list(map(str,args)), env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    assert (p.returncode == 0) == ok, (args[:3], len(args), p.returncode, p.stdout)
    return p.stdout
with tempfile.TemporaryDirectory(prefix='module-roots-', dir=repo/'build') as temporary:
    work = Path(temporary)
    app = work/'app'; app.mkdir()
    roots = [work/f'root{i:03}' for i in range(273)]
    for i, root in enumerate(roots):
        root.mkdir()
        (root/f'Marker{i:03}.zi').write_text(f'Marker :: {i};\n')
    for i, value in ((64,20),(272,21)):
        (roots[i]/f'R{i}.zi').write_text(f'#program_export\nValue{i} :: () -> s32 {{ return {value} }}\n')
    for root, value in ((roots[0],1),(roots[-1],99)):
        (root/'Winner.zi').write_text(f'#program_export\nFirst :: () -> s32 {{ return {value} }}\n')
    source = app/'main.zi'
    source.write_text('#import "R64"\n#import "R272"\n#import "Winner"\n#program_export\nResult :: () -> s32 { return Value64() + Value272() + First() }\n')
    paths = [p for root in roots for p in ('--module-path',root)]
    run(ziran,'check','--root',app,*paths,source)
    ir = work/'ir'
    run(ziran,'ir','--root',app,*paths,'-o',ir,source)
    saved = [work/f'saved{i:03}' for i in range(273)]
    for root in saved: root.mkdir()
    for name,i in (('R64',64),('R272',272),('Winner',0)):
        shutil.copyfile(ir/f'{name}.zir',saved[i]/f'{name}.zir')
    saved_app = work/'saved-app'; saved_app.mkdir()
    shutil.copyfile(ir/'main.zir',saved_app/'main.zir')
    bundles = []
    for label,root,dirs,entry in (('source',app,roots,source),('saved',saved_app,saved,saved_app/'main.zir')):
        options = [p for directory in dirs for p in ('--module-path',directory)]
        base = ('--root',root,*options)
        bundle = work/f'{label}.zx'
        run(ziran,'bundle',*base,'--entry','main:Result','-o',bundle,entry)
        assert run(ziran,'run',bundle).strip() == '42'
        bundles.append(bundle.read_bytes())
        for target in ('c','go'):
            output = work/f'{label}-{target}'
            run(ziran,'build',f'--target={target}',*(('--pkg','main') if target == 'go' else ()),*base,'--entry','main:Result','-o',output,entry)
            executable = output/'probe'
            if target == 'c':
                (output/'probe.c').write_text('#include "main.h"\nint main(void) { return Result(); }\n')
                run(os.environ.get('CC','cc'),'-I'+str(repo/'include'),*output.glob('*.c'),'-o',executable)
            else:
                (output/'probe.go').write_text('package main\nimport "os"\nfunc main() { os.Exit(int(Main_Result())) }\n')
                run('go','build','-o',executable,*output.glob('*.go'))
            assert subprocess.run([executable],env=env).returncode == 42
    assert bundles[0] == bundles[1]
    error = run(ziran,'check','--root',app,*paths,'--module-path',work/'missing',source,ok=False)
    assert 'module search path is unavailable' in error, error
    print('PASS: 273 roots, last-root resolution, first-root ordering, source/saved IR, identical ZX/portable VM, native C/Go, invalid path')
