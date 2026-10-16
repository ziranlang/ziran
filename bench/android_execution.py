#!/usr/bin/env python3
"""Prepare cross-built kernels, then measure them on one explicitly selected Android device."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import random
import re
import shlex
import statistics
import subprocess
import time

from cpu_features import ROOT, SEED, oracle, source

TARGETS = {'armeabi-v7a': ('armv7a-linux-androideabi', 'arm', '7'),
           'arm64-v8a': ('aarch64-linux-android', 'arm64', ''),
           'x86': ('i686-linux-android', '386', ''),
           'x86_64': ('x86_64-linux-android', 'amd64', '')}


def environment():
    env = dict(os.environ)
    for key in ('DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'DBUS_SESSION_BUS_ADDRESS',
                'ADB_SERVER_SOCKET', 'ANDROID_ADB_SERVER_PORT', 'ANDROID_SERIAL',
                'ZIRAN_PROFILE', 'ZIRAN_VM_PROFILE', 'GOFLAGS', 'CGO_CFLAGS',
                'CGO_CXXFLAGS', 'CGO_LDFLAGS'):
        env.pop(key, None)
    return env


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def execute(argv, env, **kwargs):
    return subprocess.run(list(map(str, argv)), env=env, check=True, **kwargs)


def prepare(args):
    out = args.out.resolve()
    if out.exists():
        raise ValueError('Preserve required receipts and remove inactive outputs before reusing --out')
    out.mkdir(parents=True)
    env = environment()
    triple, goarch, goarm = TARGETS[args.abi]
    ndk = args.ndk.resolve() / 'toolchains/llvm/prebuilt/linux-x86_64/bin'
    cc = ndk / (triple + '21-clang')
    tools = args.bin_dir.resolve()
    entry = out / 'kernel.zi'
    entry.write_text(source(args.small))
    execute([tools/'ziran', 'ir', '--root', out, '-o', out/'ir', entry], env)
    cases = []
    for form in ('source', 'saved'):
        input_file = entry if form == 'source' else out/'ir/kernel.zir'
        root = input_file.parent
        c = out/(form+'-c')
        execute([tools/'ziran', 'build', '--target=c', '--root', root, '-o', c, input_file], env)
        execute([cc, '-O3', '-std=c11', '-D_GNU_SOURCE', '-I'+str(c),
                 ROOT/'bench/android_native.c', c/'kernel.c', '-lm', '-o', c/'run'], env)
        cases.append(dict(name=form+'-c', binary=str((c/'run').relative_to(out))))
        go = out/(form+'-go')
        execute([tools/'ziran', 'build', '--target=go', '--pkg', 'main', '--root', root,
                 '-o', go, input_file], env)
        (go/'main.go').write_bytes((ROOT/'bench/android_native.go').read_bytes())
        (go/'bridge.go').write_text('package main\nfunc benchKernel(n int64, seed uint64) uint64 { return Kernel_Kernel(n,seed) }\n')
        cases.append(dict(name=form+'-go', binary=str((go/'run').relative_to(out))))
        execute([tools/'ziran', 'bundle', '--root', root, '--entry', 'kernel:Answer',
                 '-o', out/(form+'.zib'), input_file], env)
    assert (out/'source-c/kernel.c').read_bytes() == (out/'saved-c/kernel.c').read_bytes()
    assert (out/'source-go/kernel.go').read_bytes() == (out/'saved-go/kernel.go').read_bytes()
    assert (out/'source.zib').read_bytes() == (out/'saved.zib').read_bytes()
    control = out/'control-c'; control.mkdir()
    (control/'kernel.h').write_text('#include <stdint.h>\nuint64_t Kernel(int64_t rounds, uint64_t seed);\n')
    (control/'kernel.c').write_text('''#include "kernel.h"
static uint64_t Count(uint64_t value) {
    uint64_t bits = value - ((value >> 1) & UINT64_C(0x5555555555555555));
    bits = (bits & UINT64_C(0x3333333333333333)) + ((bits >> 2) & UINT64_C(0x3333333333333333));
    return (((bits + (bits >> 4)) & UINT64_C(0x0f0f0f0f0f0f0f0f)) * UINT64_C(0x0101010101010101)) >> 56;
}
uint64_t Kernel(int64_t rounds, uint64_t seed) {
    uint64_t value=seed,total=0;
    for(int64_t i=0;i<rounds;i++) { value^=value<<13;value^=value>>7;value^=value<<17;total+=Count(value); }
    return total;
}
''')
    execute([cc, '-O3', '-std=c11', '-D_GNU_SOURCE', '-I'+str(control),
             ROOT/'bench/android_native.c', control/'kernel.c', '-o', control/'run'], env)
    cases.append(dict(name='control-c', binary='control-c/run'))
    go = out/'control-go'; go.mkdir()
    (go/'main.go').write_bytes((ROOT/'bench/android_native.go').read_bytes())
    (go/'kernel.go').write_text('''package main
func benchKernel(n int64, seed uint64) uint64 {
    value,total:=seed,uint64(0)
    for i:=int64(0);i<n;i++ { value^=value<<13;value^=value>>7;value^=value<<17
        bits:=value-((value>>1)&0x5555555555555555)
        bits=(bits&0x3333333333333333)+((bits>>2)&0x3333333333333333)
        total+=(((bits+(bits>>4))&0x0f0f0f0f0f0f0f0f)*0x0101010101010101)>>56
    };return total
}
''')
    cases.append(dict(name='control-go', binary='control-go/run'))
    selected = dict(env, GOOS='android', GOARCH=goarch, GO111MODULE='off', CGO_ENABLED='1',
                    CC=str(cc), GOCACHE=str(out/'go-cache'))
    if goarm: selected['GOARM'] = goarm
    for case in cases:
        if case['name'].endswith('-go'):
            directory = (out/case['binary']).parent
            execute(['go', 'build', '-trimpath', '-o', directory/'run', '.'], selected, cwd=directory)
        case['sha256'] = sha(out/case['binary'])
        case['bytes'] = (out/case['binary']).stat().st_size
    library = ROOT/'build/bundle-runtime'/('android-'+args.abi)/'libziran.a'
    execute(['make', '-j4', 'BOOTSTRAP=1', 'BUILD_DIR='+str(library.parent), 'CC='+str(cc),
             'AR='+str(ndk/'llvm-ar'), 'OBJCOPY='+str(ndk/'llvm-objcopy'),
             'CFLAGS=-O2 -fPIC', library], env, cwd=ROOT)
    execute([cc, '-O2', '-std=c11', '-D_GNU_SOURCE', '-I'+str(ROOT/'include'),
             '-I'+str(ROOT/'cmd/zir'), ROOT/'bench/vm_startup.c', library, '-lm', '-pthread',
             '-o', out/'vm-runner'], env)
    data = dict(schema=1, created_utc=time.strftime('%Y-%m-%dT%H:%M:%SZ',time.gmtime()),
                abi=args.abi, small=args.small, large=args.large, seed=SEED,
                expected={str(n):oracle(n) for n in (args.small,args.large)}, cases=cases,
                bundle_sha256=sha(out/'source.zib'), vm_runner_sha256=sha(out/'vm-runner'),
                runtime_sha256=sha(library), source_saved_identical=True,
                compiler_versions=dict(ndk=execute([cc,'--version'],env,capture_output=True,text=True).stdout,
                                       go=execute(['go','version'],env,capture_output=True,text=True).stdout),
                tool_hashes={name:sha(tools/name) for name in ('ziran','zi2c','zi2go','zi2zir','zi2zib')},
                harness_hashes={name:sha(ROOT/'bench'/name) for name in (
                    'android_execution.py','android_native.c','android_native.go','cpu_features.py','vm_startup.c')},
                flags=dict(c=['-O3','-std=c11','-D_GNU_SOURCE'],vm=['-O2','-std=c11'],
                           go=dict(GOOS='android',GOARCH=goarch,GOARM=goarm,CGO_ENABLED='1',GOMAXPROCS='1')),
                source_hashes={str(p.relative_to(out)):sha(p) for p in out.rglob('*')
                               if p.is_file() and p.suffix in ('.zi','.c','.h','.go') and 'go-cache' not in p.parts},
                caveats=['One synthetic workload; device clocks, temperature and background work are not controlled.',
                         'Native intervals exclude ADB and startup; peak RSS includes process startup/runtime.',
                         'Kernel CPU accounting may quantize short samples to zero; use the long input for CPU comparisons.',
                         'The VM runs the small input only and reports checked loading/opening separately.',
                         'VM samples run after the native trials; native/VM time ratios are not paired measurements.',
                         'Assembly eligibility follows the target: ARMv7 uses the checked body for this fixture.',
                         'Go uses GOMAXPROCS=1 for the single-threaded kernel; no device settings are changed.',
                         'C/Go use the same SWAR population-count formula; Python bit_count supplies the independent oracle.'])
    (out/'manifest.json').write_text(json.dumps(data,indent=2)+'\n')
    print('Prepared Android native/Go controls and portable VM fixture')


def run(args):
    out = args.out.resolve(); manifest=json.loads((out/'manifest.json').read_text());env=environment()
    remote=None; samples=[]; result={'manifest':manifest,'samples':samples,'complete':False,
        'started_utc':time.strftime('%Y-%m-%dT%H:%M:%SZ',time.gmtime())}
    output=(out/'results.json').open('x')
    def save():
        output.seek(0);json.dump(result,output,indent=2);output.write('\n');output.truncate();output.flush()
    save()
    def adb(*argv):
        return execute(['adb','-s',args.serial,*argv],env,capture_output=True,text=True,timeout=150).stdout.strip()
    def shell(*argv):return adb('shell',shlex.join(list(map(str,argv))))
    def observe():
        text=shell('dumpsys','battery')
        return {k:v.strip() for k,v in re.findall(r'^\s*(level|temperature|status|USB powered):\s*(.+)$',text,re.M)}
    try:
        assert adb('get-state')=='device'
        assert shell('getprop','ro.product.cpu.abi')==manifest['abi']
        result['device']={key:shell('getprop',key) for key in ('ro.product.model','ro.build.version.release','ro.build.version.sdk','ro.product.cpu.abi')}
        result['serial_sha256']=hashlib.sha256(args.serial.encode()).hexdigest()
        result['before']=observe()
        allocated=shell('mktemp','-d','/data/local/tmp/ziran-bench.XXXXXX')
        assert re.fullmatch(r'/data/local/tmp/ziran-bench\.[A-Za-z0-9]+',allocated)
        remote=allocated
        result['remote']=remote;save()
        files={case['name']:(out/case['binary'],case['sha256']) for case in manifest['cases']}
        files.update({'vm-runner':(out/'vm-runner',manifest['vm_runner_sha256']),
                      'program.zib':(out/'source.zib',manifest['bundle_sha256'])})
        for name,(path,digest) in files.items():
            assert sha(path)==digest
            adb('push',str(path),remote+'/'+name)
            assert shell('sha256sum',remote+'/'+name).split()[0]==digest
            if name!='program.zib':shell('chmod','700',remote+'/'+name)
        trials=[(case['name'],n,r) for r in range(args.repetitions+1) for n in (manifest['small'],manifest['large']) for case in manifest['cases']]
        random.Random(20261016).shuffle(trials)
        # Discarded process warmups finish before measured trials.
        trials.sort(key=lambda row:row[2]!=0)
        for name,n,repetition in trials:
            row=json.loads(shell('timeout','120','env','GOMAXPROCS=1',remote+'/'+name,n,manifest['seed']))
            assert row['result']==manifest['expected'][str(n)],(name,n,row)
            samples.append(dict(case=name,rounds=n,repetition=repetition,warmup=repetition==0,**row))
        for repetition in range(args.repetitions+1):
            row=json.loads(shell('timeout','120',remote+'/vm-runner',remote+'/program.zib',9,manifest['expected'][str(manifest['small'])]))
            assert row['result']==manifest['expected'][str(manifest['small'])]
            samples.append(dict(case='vm',rounds=manifest['small'],repetition=repetition,warmup=repetition==0,**row))
        result['after']=observe()
        groups={}
        for row in samples:
            if not row['warmup']:groups.setdefault((row['case'],row['rounds']),[]).append(row)
        result['summary']=[]
        for (name,n),rows in groups.items():
            metrics=('load_ms','open_ms','steady_run_ms','live_value_bytes') if name=='vm' else (
                'elapsed_ms','user_ms','system_ms','max_rss_kib')
            result['summary'].append(dict(case=name,rounds=n,samples=len(rows),**{
                'median_'+key:statistics.median(r[key] for r in rows) for key in metrics}))
        result['complete']=True
    except Exception as error:
        result['complete']=False;result['error']=repr(error)
        raise
    finally:
        if remote:
            try:shell('rm','-rf',remote);result['cleaned']=True
            except Exception as error:result['cleanup_error']=str(error);result['remote']=remote
        result['finished_utc']=time.strftime('%Y-%m-%dT%H:%M:%SZ',time.gmtime())
        save();output.close()
    if not result.get('cleaned'):raise RuntimeError('Remote benchmark cleanup is unverified; inspect results before another run')
    print(json.dumps({'complete':result['complete'],'cleaned':result.get('cleaned'),
                      'measured_samples':sum(not r['warmup'] for r in samples),'summary':result.get('summary')},indent=2))


def main():
    parser=argparse.ArgumentParser(description=__doc__);sub=parser.add_subparsers(dest='stage',required=True)
    prepare_parser=sub.add_parser('prepare');prepare_parser.add_argument('--out',type=Path,required=True)
    prepare_parser.add_argument('--ndk',type=Path,required=True);prepare_parser.add_argument('--abi',choices=TARGETS,required=True)
    prepare_parser.add_argument('--bin-dir',type=Path,default=ROOT/'build/bin')
    prepare_parser.add_argument('--small',type=int,default=1000);prepare_parser.add_argument('--large',type=int,default=1000000)
    run_parser=sub.add_parser('run');run_parser.add_argument('--out',type=Path,required=True)
    run_parser.add_argument('--serial',required=True);run_parser.add_argument('--repetitions',type=int,default=9)
    args=parser.parse_args()
    if args.stage=='prepare':
        if not 0<args.small<args.large or args.small>10000:parser.error('require 0 < small <= 10000 and small < large')
        prepare(args)
    else:
        if args.repetitions<1:parser.error('positive repetitions required')
        if not re.fullmatch(r'[A-Za-z0-9_.:-]+',args.serial):parser.error('an explicit nonempty serial is required')
        run(args)


if __name__=='__main__':main()
