#!/usr/bin/env python3
"""Compare Go assembly bridges, pure Go bodies, a preserved emitter and native C."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import random
import re
import shutil
import statistics
import subprocess
import time

from cpu_features import ROOT, SEED, oracle, source


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--bin-dir', type=Path, default=ROOT/'build/bin')
    parser.add_argument('--before-bin-dir', type=Path, required=True)
    parser.add_argument('--small', type=int, default=1000)
    parser.add_argument('--large', type=int, default=20000000)
    parser.add_argument('--repetitions', type=int, default=9)
    args = parser.parse_args()
    if platform.machine() not in ('x86_64','aarch64'):
        parser.error('requires an x86_64 or aarch64 host')
    if not 0 < args.small < args.large or args.repetitions < 1:
        parser.error('require positive repetitions and 0 < small < large')
    out = args.out.resolve()
    if out.exists(): parser.error('preserve required receipts and remove inactive outputs before reuse')
    out.mkdir(parents=True)
    tools, before = args.bin_dir.resolve(), args.before_bin_dir.resolve()
    env = dict(os.environ, GO111MODULE='off', GOMAXPROCS='1', GOCACHE=str(out/'go-cache'))
    for key in ('DISPLAY','WAYLAND_DISPLAY','XAUTHORITY','DBUS_SESSION_BUS_ADDRESS',
                'CGO_CFLAGS','CGO_CPPFLAGS','CGO_CXXFLAGS','GOFLAGS','GOEXPERIMENT',
                'ZIRAN_PROFILE','ZIRAN_VM_PROFILE'):
        env.pop(key,None)
    flags = ['-O2','-g']
    if platform.machine() == 'x86_64':
        env['GOAMD64'] = 'v1'
        flags += ['-march=x86-64','-mno-popcnt','-mno-avx','-mno-bmi2']
    expected = {str(n):oracle(n) for n in (args.small,args.large)}
    metadata = dict(schema=1, platform=platform.platform(), expected=expected,
                    seed=SEED, repetitions=args.repetitions, discarded_warmups=1,
                    go=subprocess.check_output(['go','version'],text=True).strip(),
                    cc=subprocess.check_output(['cc','--version'],text=True).splitlines()[0],
                    c_flags=flags, go_runtime=dict(GOMAXPROCS='1',GOAMD64=env.get('GOAMD64')),
                    artifacts=[], builds=[], source_hashes={},
                    tool_hashes={label:{n:hashlib.sha256((directory/n).read_bytes()).hexdigest()
                                      for n in ('ziran','zi2go','zi2zir')}
                                 for label,directory in [('before',before),('current',tools)]},
                    caveats=['One synthetic workload in three assembly placements on one shared host.',
                             'Kernel timers exclude process startup and parsing; peak RSS includes runtime/startup.',
                             'Short CPU samples can quantize to zero; native mode uses cgo, direct mode uses leaf Go ABI0 assembly where representable.',
                             'Native availability is read after package initialization and recorded per Go sample.',
                             'Build times are individual warm-toolchain observations, not a compilation-speed ranking.',
                             'C controls use the same generated algorithm, not an independently optimized parser or application.'])

    def invoke(argv, selected=env, cwd=ROOT):
        return subprocess.run(list(map(str,argv)),env=selected,cwd=cwd,check=True,
                              capture_output=True,text=True,timeout=180)

    def build(argv, name, selected=env, cwd=ROOT):
        started=time.perf_counter_ns();invoke(argv,selected,cwd)
        metadata['builds'].append(dict(case=name,argv=list(map(str,argv)),
                                      elapsed_ms=(time.perf_counter_ns()-started)/1e6))

    cases=[]
    for form in ('function','block','operation-block'):
        directory=out/form;fixture=directory/'fixture';fixture.mkdir(parents=True)
        entry=fixture/'kernel.zi';entry.write_text(source(args.small,assembly_form=form))
        invoke([tools/'ziran','ir','--root',fixture,'-o',directory/'ir',entry])
        generated={}
        for label,compiler,input_file in [('before',before,entry),('source',tools,entry),
                                          ('saved',tools,directory/'ir/kernel.zir')]:
            target=directory/label
            invoke([compiler/'ziran','build','--target=go','--pkg','main','--root',input_file.parent,
                    '-o',target,input_file])
            generated[label]=target
        names={p.name for p in generated['source'].glob('*') if p.suffix in ('.go','.h','.s')}
        assert names=={p.name for p in generated['saved'].glob('*') if p.suffix in ('.go','.h','.s')}
        assert all((generated['source']/n).read_bytes()==(generated['saved']/n).read_bytes() for n in names)
        for mode,label,cgo in [('before','before','0'),('portable','source','0'),('default','source','1'),('native','source','1'),('direct','source','0')]:
            target=generated[label]
            dispatch='false'
            if label=='source':
                text='\n'.join(p.read_text() for p in target.glob('*.assembly_dispatch.go'))
                names=re.findall(r'^var (\w+) func\(',text,re.M);assert len(names)==1
                dispatch=names[0]+' != nil'
            (target/'bridge.go').write_text('package main\nvar nativeEnabled bool\nfunc init(){ nativeEnabled = '+dispatch+' }\nfunc benchKernel(n int64,seed uint64) uint64 {return Kernel_Kernel(n,seed)}\n')
            host=(ROOT/'bench/android_native.go').read_text().replace('"result": result, "elapsed_ms": elapsed,','"result": result, "elapsed_ms": elapsed, "native": nativeEnabled,')
            (target/'main.go').write_text(host)
            binary=directory/('run-'+mode)
            selected=dict(env,CGO_ENABLED=cgo,CGO_CFLAGS=' '.join(flags))
            build(['go','build','-trimpath',*(['-tags=ziran_asm'] if mode in ('native','direct') else []),'-o',binary,'.'],form+'/'+mode,selected,target)
            cases.append(dict(form=form,mode=mode,binary=binary))
        target=directory/'c'
        invoke([tools/'ziran','build','--target=c','--root',fixture,'-o',target,entry])
        for mode in ('c-native','c-portable'):
            binary=directory/('run-'+mode)
            build(['cc',*flags,'-O3','-std=c11','-D_GNU_SOURCE',
                   *(['-DZIRAN_NO_ASM'] if mode=='c-portable' else []),'-I'+str(target),
                   ROOT/'bench/android_native.c',*sorted(target.glob('*.c')),'-lm','-o',binary],form+'/'+mode)
            cases.append(dict(form=form,mode=mode,binary=binary))
    for case in cases:
        p=case['binary'];metadata['artifacts'].append(dict(form=case['form'],mode=case['mode'],
                  path=str(p.relative_to(out)),bytes=p.stat().st_size,sha256=hashlib.sha256(p.read_bytes()).hexdigest()))
    for p in out.rglob('*'):
        if p.is_file() and p.suffix in ('.zi','.zir','.go','.c','.h','.s') and 'go-cache' not in p.parts:
            metadata['source_hashes'][str(p.relative_to(out))]=hashlib.sha256(p.read_bytes()).hexdigest()
    metadata['source_saved_identical']=True
    timer=[]
    if executable:=shutil.which('time'):
        probe=subprocess.run([executable,'--version'],capture_output=True,text=True)
        if probe.returncode==0 and 'GNU' in probe.stdout+probe.stderr: timer=[executable]
    if not timer and (executable:=shutil.which('busybox')):
        probe=subprocess.run([executable,'time','--help'],capture_output=True,text=True)
        if '-f FMT' in probe.stdout+probe.stderr and '-o FILE' in probe.stdout+probe.stderr:
            timer=[executable,'time']
    if not timer: raise RuntimeError('GNU or BusyBox time is required for independent child peak RSS')
    metadata['rss_launcher']=timer
    (out/'metadata.json').write_text(json.dumps(metadata,indent=2)+'\n')
    trials=[(case,n,r) for r in range(args.repetitions+1) for n in (args.small,args.large) for case in cases]
    random.Random(20261016).shuffle(trials);trials.sort(key=lambda row:row[2]!=0)
    samples=[]
    with (out/'samples.jsonl').open('x') as log:
        for case,n,repetition in trials:
            rss=out/'rss.txt'
            row=json.loads(invoke([*timer,'-f','%M','-o',rss,case['binary'],n,SEED]).stdout)
            row['self_reported_max_rss_kib']=row['max_rss_kib']
            row['max_rss_kib']=int(rss.read_text())
            assert row['result']==expected[str(n)],(case,n,row)
            row.update(form=case['form'],mode=case['mode'],rounds=n,repetition=repetition,warmup=repetition==0)
            samples.append(row);log.write(json.dumps(row)+'\n');log.flush()
    summary=[]
    for case in cases:
        for n in (args.small,args.large):
            rows=[r for r in samples if not r['warmup'] and r['form']==case['form'] and r['mode']==case['mode'] and r['rounds']==n]
            summary.append(dict(form=case['form'],mode=case['mode'],rounds=n,samples=len(rows),
                native=all(r.get('native',False) for r in rows),**{
                'median_'+key:statistics.median(r[key] for r in rows)
                for key in ('elapsed_ms','user_ms','system_ms','max_rss_kib')}))
    (out/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(f'{sum(not r["warmup"] for r in samples)} independently checked measured samples; source/saved Go companions match')


if __name__=='__main__':main()
