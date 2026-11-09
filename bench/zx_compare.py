#!/usr/bin/env python3
"""Prepare on Alpha Personal VM; measure there, or transport built artifacts to USB Moto."""
import sys
import argparse, hashlib, json, math, os, platform, random, re, shutil, statistics, subprocess, time
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]

def oracle(seed, rounds, kind):
    def mix(x, i):
        return ((x*1103515245+12345) if i&1 else (x*1664525+1013904223)) & 2147483647
    total = 0
    cells = [[seed & 255, j+3] for j in range(32)]
    for i in range(rounds):
        if kind == 0:
            seed = mix(seed, i); total += seed
        else:
            x, width = cells[i&31]
            x = (mix(x+seed, i)+width)&65535
            width = (width+(5 if seed&1 else 6)+(i&7))&255
            total += x if x&1 else width
            cells[i&31] = [x,width]
    return total

def invoke(argv, build=False, **kw):
    env = dict(os.environ)
    for key in ('DISPLAY','WAYLAND_DISPLAY','XAUTHORITY','DBUS_SESSION_BUS_ADDRESS'):
        env.pop(key, None)
    env['YUE_DESKTOP_RECOVERY']='0'
    cmd = list(map(str, argv))
    if build and not os.environ.get('BUILD_QUEUE_JOBS'): cmd = ['build-queue', '--', *cmd]
    result=subprocess.run(cmd, env=env, check=False, text=True, capture_output=True, **kw)
    if result.returncode:
        print(result.stdout[-3000:]+result.stderr[-3000:],file=sys.stderr)
        result.check_returncode()
    return result

def sha(p):
    h=hashlib.sha256()
    with open(p,'rb') as f:
        for chunk in iter(lambda:f.read(65536), b''): h.update(chunk)
    return h.hexdigest()

def prepare(a):
    out=a.out.resolve(); out.mkdir(parents=True, exist_ok=True)
    tools=ROOT/'build/bin'
    commands=[]
    def build(cmd):
        commands.append(list(map(str,cmd))); started=time.monotonic()
        result=invoke(cmd,build=True)
        commands[-1]={'argv':commands[-1], 'elapsed_ms':(time.monotonic()-started)*1000, 'stderr':result.stderr[-2000:]}
    build([tools/'ziran','bundle','--root',ROOT/'bench','--entry','zx_compare:Answer','-o',out/'fixture.zx', ROOT/'bench/zx_compare.zi'])
    build([tools/'zi2wasm','--root',ROOT/'bench','--entry','zx_compare:Answer','-o',out/'fixture.wasm',ROOT/'bench/zx_compare.zi'])
    for version,delta in (('old',1),('new',2)):
        directory=out/version; directory.mkdir(exist_ok=True)
        source=directory/'component.zi'
        source.write_text('count: s64 = 7;\n#program_export\nNext :: () -> s64 { count += '+str(delta)+'; return count }\n')
        build([tools/'ziran','bundle','--root',directory,'--entry','component:Next','-o',out/('component-'+version+'.zx'),source])
    cc=a.cc
    links=['-lm','-pthread','-ldl']+(['-llog'] if 'android' in str(cc) else [])
    common=[cc,'-O3','-std=c11','-I'+str(ROOT/'include')]
    build([*common,'-DCONTROL',ROOT/'bench/zx_compare_host.c',ROOT/'bench/zx_compare_control.c','-o',out/'control'])
    build([*common,ROOT/'bench/zx_compare_host.c',a.library,*links,'-o',out/'zx-host'])
    build([*common,ROOT/'bench/zx_compare_component.c',a.library,*links,'-o',out/'component-host'])
    metadata={'commands':commands,'compiler':invoke([cc,'--version']).stdout.splitlines()[0],
      'source_revision':invoke(['git','rev-parse','HEAD']).stdout.strip(),
      'sources':{str(p.relative_to(ROOT)):sha(p) for p in (ROOT/'bench').glob('zx_compare*') if p.is_file()},
      'artifacts':{p.name:sha(p) for p in (out/'fixture.zx',out/'fixture.wasm',out/'control',out/'zx-host',out/'component-host',out/'component-old.zx',out/'component-new.zx')},
      'queue_jobs':os.environ.get('BUILD_QUEUE_JOBS'), 'library':{'path':str(a.library),'sha256':sha(a.library)}}
    (out/'prepare.json').write_text(json.dumps(metadata,indent=2)+'\n')
    print('Prepared checksum fixtures and four real engine artifacts')

def measure(a):
    out=a.out.resolve(); meta=json.loads((out/'prepare.json').read_text())
    for name,digest in meta['artifacts'].items():
        if sha(out/name)!=digest: raise ValueError('changed artifact '+name)
    serial=a.serial; remote='/data/local/tmp/ziran-zx-compare'
    def adb(*args): return invoke(['adb','-s',serial,*args]).stdout.strip()
    def shell(text): return adb('shell',text)
    def observe():
        if a.device: return re.sub(r'(?m)^Serial.*$', '', shell('dumpsys battery; cat /proc/cpuinfo'))
        return {'platform':platform.platform(),'cpu':Path('/proc/cpuinfo').read_text()[:3000],'load':os.getloadavg()}
    receipt={'schema':1,'prepared':meta,'target':'USB Moto' if a.device else 'Alpha Personal VM',
        'started_utc':time.strftime('%Y-%m-%dT%H:%M:%SZ',time.gmtime()),'before':observe(),
        'samples':[],'unavailable':[],'complete':False}
    if a.device:
        assert adb('get-state')=='device'
        assert shell('getprop ro.product.cpu.abi')=='armeabi-v7a'
        assert shell('getprop ro.product.model')=='moto e6 play'
        receipt['device']={key:shell('getprop '+key) for key in ('ro.product.model','ro.build.version.release','ro.product.cpu.abi')}
        shell('mkdir '+remote)
        for name in ('fixture.zx','control','zx-host','component-host','component-old.zx','component-new.zx'):
            adb('push',str(out/name),remote+'/'+name)
            assert shell('sha256sum '+remote+'/'+name).split()[0]==meta['artifacts'][name]
        shell('chmod 700 '+remote+'/control '+remote+'/zx-host '+remote+'/component-host')
    else:
        receipt['engines']={engine:shutil.which(engine) for engine in ('node','wasmtime','wasm3','iwasm')}
        if shutil.which('node'): receipt['node_version']=invoke(['node','--version']).stdout.strip()
    receipt['measurement_sources']={p.name:sha(p) for p in (ROOT/'bench').glob('zx_compare*') if p.is_file()}
    variants=['vm','native','c']+([] if a.device or not shutil.which('node') else ['wasm-node'])
    rng=random.Random(20261109)
    a.results.parent.mkdir(parents=True,exist_ok=True)
    def save(): a.results.write_text(json.dumps(receipt,indent=2)+'\n')
    save()
    try:
        for trial in range(a.repetitions):
            order=[(kind,v) for kind in range(2) for v in variants]; rng.shuffle(order)
            for kind,variant in order:
                if a.device:
                    battery=shell('dumpsys battery')
                    temperature=re.search(r'temperature: ([0-9]+)',battery)
                    if temperature and int(temperature[1])>=400: raise RuntimeError('Moto reached 40C thermal stop')
                seed=123456+trial*100
                expected=sum(oracle(seed+j,a.rounds,kind) for j in range(a.batches))
                args=[seed,a.rounds,kind,a.batches,expected]
                if variant=='wasm-node':
                    result=invoke(['node',ROOT/'bench/zx_compare_wasm.cjs',out/'fixture.wasm',*args])
                elif a.device:
                    binary='control' if variant=='c' else 'zx-host'
                    cmd=f'ZIRAN_NATIVE_TIMING=1 ZIRAN_NATIVE_CACHE={remote}/cache {remote}/{binary} {variant} {remote}/fixture.zx '+ ' '.join(map(str,args))
                    result=invoke(['adb','-s',serial,'shell',cmd])
                else:
                    binary=out/('control' if variant=='c' else 'zx-host')
                    result=invoke(['env','ZIRAN_NATIVE_TIMING=1','ZIRAN_NATIVE_CACHE='+str(out/'cache'),binary,variant,out/'fixture.zx',*args])
                if variant=='native' and 'ziran: native' not in result.stderr:
                    receipt['unavailable'].append({'variant':variant,'trial':trial,'kind':kind,'reason':'No TinyCC execution marker; result excluded','stderr':result.stderr[-1000:]}); save(); continue
                row=json.loads(result.stdout.strip())
                warmup=re.search(r'benchmark warmup_count=([0-9]+) warmup_ms=([0-9.]+)',result.stderr)
                if warmup: row.update(warmup_count=int(warmup[1]),warmup_ms=float(warmup[2]))
                assert int(row['checksum'])==expected
                row.update(variant=variant,trial=trial,kind=kind,seed=seed,rounds=a.rounds,
                           per_entry_ms=row['warm_ms']/a.batches,native_evidence=result.stderr[-2000:])
                receipt['samples'].append(row); save()
        if a.device:
            component=invoke(['adb','-s',serial,'shell',remote+'/component-host '+remote+'/component-old.zx '+remote+'/component-new.zx'])
        else:
            component=invoke([out/'component-host',out/'component-old.zx',out/'component-new.zx'])
        receipt['component_samples']=[json.loads(line) for line in component.stdout.splitlines()]
        assert len(receipt['component_samples'])==9 and all(row['state_rollback_stale_verified'] for row in receipt['component_samples'])
        receipt['summary']=[]
        for kind in range(2):
            for variant in variants:
                values=sorted(r['per_entry_ms'] for r in receipt['samples'] if r['kind']==kind and r['variant']==variant)
                if values: receipt['summary'].append({'kind':kind,'variant':variant,'n':len(values),'median_ms':statistics.median(values),'p95_ms':values[math.ceil(.95*len(values))-1]})
        receipt['measurement_loop_finished']=True
        receipt['complete']=False  # Preliminary shell data; embedded comparison has a separate receipt.
    finally:
        receipt['after']=observe(); save()
        if a.device: shell('rm -rf -- '+remote)
    print(json.dumps(receipt.get('summary',[]),indent=2))

def main():
    p=argparse.ArgumentParser(description=__doc__); sub=p.add_subparsers(dest='command',required=True)
    prep=sub.add_parser('prepare'); prep.add_argument('--cc',default='cc'); prep.add_argument('--library',type=Path,default=ROOT/'build/libziran.a')
    run=sub.add_parser('run'); run.add_argument('--serial'); run.add_argument('--device',action='store_true'); run.add_argument('--repetitions',type=int,default=9)
    run.add_argument('--rounds',type=int,default=256); run.add_argument('--batches',type=int,default=16)
    run.add_argument('--results',type=Path,required=True)
    for cmd in (prep,run): cmd.add_argument('--out',type=Path,default=ROOT/'build/benchmarks/zx-compare')
    a=p.parse_args()
    if a.command=='run' and a.device and not a.serial: p.error('--device requires exact --serial')
    (prepare if a.command=='prepare' else measure)(a)
if __name__=='__main__': main()
