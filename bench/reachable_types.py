#!/usr/bin/env python3
"""Compare qualified type-graph lookup, preserving failed baseline answers."""
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
    parser.add_argument('--before-limit', type=int, default=512, help='known baseline module cutoff; 0 expects no cutoff')
    args = parser.parse_args()
    if args.repetitions < 1 or args.before_limit < 0: parser.error('positive repetitions and nonnegative before-limit required')
    out = args.out.resolve()
    if out.exists(): parser.error('archive records and remove inactive outputs before reuse')
    out.mkdir(parents=True)
    env = {k:v for k,v in os.environ.items() if k not in ['DISPLAY','WAYLAND_DISPLAY','XAUTHORITY','DBUS_SESSION_BUS_ADDRESS','ZIRAN_VERIFY_SYMBOL_LOOKUPS','ZIRAN_PROFILE','ZIRAN_VM_PROFILE']}
    env['YUE_DESKTOP_RECOVERY'] = '0'
    timer = []
    if tool := shutil.which('time'):
        result = subprocess.run([tool,'--version'],capture_output=True,text=True)
        if result.returncode == 0 and 'GNU' in result.stdout+result.stderr: timer=[tool]
    if not timer and (tool := shutil.which('busybox')):
        result = subprocess.run([tool,'time','--help'],capture_output=True,text=True)
        if '-f FMT' in result.stdout+result.stderr: timer=[tool,'time']
    if not timer: parser.error('requires GNU or BusyBox time')
    sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    libraries = dict(before=args.before_lib.resolve(),current=args.library.resolve())
    for label,library in libraries.items():
        subprocess.run(['cc','-O2','-D_GNU_SOURCE','-std=c11','-Iinclude','-Icmd/zir','bench/reachable_types.c',library,'-lm','-pthread','-o',out/label],cwd=ROOT,env=env,check=True)
    meta = dict(schema=1,platform=platform.platform(),repetitions=args.repetitions,before_limit=args.before_limit,measurement_launcher=timer,
                compiler_version=subprocess.check_output(['cc','--version'],text=True),cpu_affinity=sorted(os.sched_getaffinity(0)) if hasattr(os,'sched_getaffinity') else None,
                environment_switches={k:env.get(k) for k in ['ZIRAN_VERIFY_SYMBOL_LOOKUPS','ZIRAN_PROFILE','ZIRAN_VM_PROFILE']},
                libraries={label:dict(path=str(p),sha256=sha(p)) for label,p in libraries.items()},
                source_sha256={name:sha(ROOT/name) for name in ['cmd/zir/zir.c','bench/reachable_types.c','bench/reachable_types.py']},
                caveats=['Two qualified lookups per round: the connected final module and an absent module. Input-graph construction and process startup are outside the internal wall/CPU timers.',
                         'Cold samples invalidate entry-scope answers each round. Cached samples reuse answers prepared before timing.',
                         'A supplied nonzero before-limit identifies a known baseline cutoff. Incorrect larger answers are retained and are not equivalent-work speed comparisons.',
                         'Process peak RSS includes graph construction/storage and other compiler metadata; it does not isolate traversal scratch memory.',
                         'One shared warm-cache host; no CPU/frequency isolation. This is a lookup component, not whole compilation.'])
    rows=[]
    for count in [16,128,512,1024,4096]:
        rounds=max(8,8192//count)
        for shape in ['wide','deep','shared']:
            for mode in ['cold','cached']:
                for repetition in range(args.repetitions+1):
                    labels=list(libraries);random.Random(count+repetition+len(shape)+len(mode)).shuffle(labels)
                    for label in labels:
                        rss=out/'rss.txt'
                        result=subprocess.run([*timer,'-f','%M','-o',str(rss),str(out/label),str(count),shape,str(rounds),mode],env=env,capture_output=True,text=True,check=True,timeout=120)
                        value=json.loads(result.stdout)
                        assert value['modules']==count and value['rounds']==rounds and value['missing_correct']
                        comparable=args.before_limit==0 or count<=args.before_limit
                        assert value['found']==(label=='current' or comparable),value
                        row=dict(modules=count,shape=shape,mode=mode,variant=label,repetition=repetition,warmup=repetition==0,correct=value['found'],equivalent_work=comparable,
                                 round_wall_us=value['wall_ns']/rounds/1000,round_cpu_us=value['cpu_ns']/rounds/1000,max_rss_kib=int(rss.read_text()))
                        rows.append(row)
                        with (out/'samples.jsonl').open('a') as f:f.write(json.dumps(row)+'\n')
            print('validated qualified lookup '+str(count)+' '+shape,flush=True)
    summary=[]
    for count in [16,128,512,1024,4096]:
        for shape in ['wide','deep','shared']:
            for mode in ['cold','cached']:
                for label in libraries:
                    selected=[r for r in rows if r['modules']==count and r['shape']==shape and r['mode']==mode and r['variant']==label and not r['warmup']]
                    summary.append(dict(modules=count,shape=shape,mode=mode,variant=label,samples=len(selected),correct=all(r['correct'] for r in selected),equivalent_work=args.before_limit==0 or count<=args.before_limit,
                                        **{'median_'+k:statistics.median(r[k] for r in selected) for k in ['round_wall_us','round_cpu_us','max_rss_kib']}))
    (out/'metadata.json').write_text(json.dumps(meta,indent=2)+'\n')
    (out/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(f'{sum(not r["warmup"] for r in rows)} qualified-lookup samples retained with explicit correctness and equivalent-work labels.',flush=True)


if __name__=='__main__':main()
