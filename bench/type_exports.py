#!/usr/bin/env python3
"""Measure local/re-exported type CPU and separate allocator traffic."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import random
import shutil
import statistics
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--before-lib', type=Path, required=True)
    parser.add_argument('--library', type=Path, default=ROOT/'build/libziran.a')
    parser.add_argument('--repetitions', type=int, default=15)
    args = parser.parse_args()
    if args.repetitions < 1: parser.error('positive repetitions required')
    out = args.out.resolve()
    if out.exists(): parser.error('archive records and remove inactive outputs before reuse')
    out.mkdir(parents=True)
    env = {k:v for k,v in os.environ.items() if k not in ['DISPLAY','WAYLAND_DISPLAY','XAUTHORITY','DBUS_SESSION_BUS_ADDRESS','ZIRAN_VERIFY_SYMBOL_LOOKUPS','ZIRAN_PROFILE','ZIRAN_VM_PROFILE']}
    env['YUE_DESKTOP_RECOVERY'] = '0'
    timer = []
    if tool := shutil.which('time'):
        r = subprocess.run([tool,'--version'],capture_output=True,text=True)
        if r.returncode == 0 and 'GNU' in r.stdout+r.stderr: timer=[tool]
    if not timer and (tool := shutil.which('busybox')):
        r = subprocess.run([tool,'time','--help'],capture_output=True,text=True)
        if '-f FMT' in r.stdout+r.stderr: timer=[tool,'time']
    if not timer: parser.error('requires GNU or BusyBox time')
    sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    libraries = dict(before=args.before_lib.resolve(),current=args.library.resolve())
    for label,library in libraries.items():
        common=['cc','-O2','-std=c11','-D_GNU_SOURCE','-Iinclude','-Icmd/zir','bench/type_exports.c',library,'-lm','-pthread']
        subprocess.run([*common,'-o',out/label],cwd=ROOT,env=env,check=True)
        subprocess.run([*common,'-DTYPE_ALLOCATIONS','-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc','-o',out/(label+'-alloc')],cwd=ROOT,env=env,check=True)
    meta=dict(schema=1,platform=platform.platform(),repetitions=args.repetitions,measurement_launcher=timer,
              cpu_affinity=sorted(os.sched_getaffinity(0)) if hasattr(os,'sched_getaffinity') else None,
              compiler_version=subprocess.check_output(['cc','--version'],text=True),
              environment_switches={k:env.get(k) for k in ['ZIRAN_VERIFY_SYMBOL_LOOKUPS','ZIRAN_PROFILE','ZIRAN_VM_PROFILE']},
              libraries={label:dict(path=str(p),sha256=sha(p)) for label,p in libraries.items()},
              source_sha256={p:sha(ROOT/p) for p in ['cmd/zir/zir.c','bench/type_exports.c','bench/type_exports.py']},
              caveats=['Two lookups per round: two owned declarations for local, or an exported Value and absent Missing for graph cases.',
                       'Graph construction/startup are outside internal wall/CPU timers. Cold rounds invalidate entry answers; cached rounds retain prepared answers.',
                       'The baseline depth guard can reject connected types. Incorrect baseline answers are retained and excluded from equivalent-work comparisons.',
                       'Allocator rows use separate binaries wrapping malloc/calloc/realloc; report cumulative calls/requested bytes, not peak/live RAM. Their timing is not used.',
                       'Process RSS includes graph construction and compiler storage. One shared warm-cache host with no CPU/frequency isolation.'])
    cases=[(1,'local')]+[(n,s) for n in [16,32,64,256,1024] for s in ['wide','deep','shared']]
    rows=[];allocations=[]
    for count,shape in cases:
        rounds=10000 if shape=='local' else max(8,4096//count)
        valid_before=shape in ['local','wide'] or count<=(34 if shape=='deep' else 67)
        for mode in ['cold','cached']:
            def execute(label,instrumented):
                binary=out/(label+('-alloc' if instrumented else ''));rss=out/'rss.txt'
                r=subprocess.run([*timer,'-f','%M','-o',str(rss),str(binary),str(count),shape,str(rounds),mode],env=env,capture_output=True,text=True,check=True,timeout=120)
                value=json.loads(r.stdout)
                assert value['modules']==count and value['rounds']==rounds and value['secondary_correct']
                assert value['found']==(label=='current' or valid_before),value
                return value,int(rss.read_text())
            for repetition in range(args.repetitions+1):
                order=list(libraries);random.Random(count+len(shape)+len(mode)+repetition).shuffle(order)
                for label in order:
                    value,rss=execute(label,False)
                    row=dict(modules=count,shape=shape,mode=mode,variant=label,repetition=repetition,warmup=repetition==0,correct=value['found'],equivalent_work=valid_before,
                             round_wall_us=value['wall_ns']/rounds/1000,round_cpu_us=value['cpu_ns']/rounds/1000,max_rss_kib=rss)
                    rows.append(row)
                    with (out/'samples.jsonl').open('a') as f:f.write(json.dumps(row)+'\n')
            for label in libraries:
                value,_=execute(label,True)
                allocations.append(dict(modules=count,shape=shape,mode=mode,variant=label,correct=value['found'],equivalent_work=valid_before,
                                        allocation_calls_per_round=value['allocation_calls']/rounds,allocation_bytes_per_round=value['allocation_bytes']/rounds))
        print('validated exported lookup '+shape+' '+str(count),flush=True)
    summary=[]
    for count,shape in cases:
        for mode in ['cold','cached']:
            for label in libraries:
                selected=[r for r in rows if r['modules']==count and r['shape']==shape and r['mode']==mode and r['variant']==label and not r['warmup']]
                summary.append(dict(modules=count,shape=shape,mode=mode,variant=label,samples=len(selected),correct=all(r['correct'] for r in selected),equivalent_work=selected[0]['equivalent_work'],
                                    **{'median_'+k:statistics.median(r[k] for r in selected) for k in ['round_wall_us','round_cpu_us','max_rss_kib']}))
    for name,value in [('metadata',meta),('summary',summary),('allocations',allocations)]:
        (out/(name+'.json')).write_text(json.dumps(value,indent=2)+'\n')
    print(f'{sum(not r["warmup"] for r in rows)} timing samples and {len(allocations)} separate allocation rows retained with correctness labels.',flush=True)


if __name__=='__main__':main()
