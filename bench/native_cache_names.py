#!/usr/bin/env python3
"""Measure the native cache's ordered name collection against a Git revision."""
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


def expected_checksum(count):
    value = 14695981039346656037
    for index in range(count):
        key = f'/shared/project/include/long/common/path/component/header-{(index * 1297) % count:08d}.h'
        for byte in key.encode()+b'\0': value = ((value ^ byte) * 1099511628211) & ((1 << 64)-1)
    return value


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--before-ref', required=True)
    parser.add_argument('--repetitions', type=int, default=15)
    args = parser.parse_args()
    if args.repetitions < 1: parser.error('repetitions must be positive')
    out = args.out.resolve()
    if out.exists(): parser.error('archive records and remove inactive outputs before reuse')
    out.mkdir(parents=True)
    env = {k:v for k,v in os.environ.items() if k not in ('DISPLAY','WAYLAND_DISPLAY','XAUTHORITY','DBUS_SESSION_BUS_ADDRESS','ZIRAN_PROFILE')}
    env['YUE_DESKTOP_RECOVERY']='0'
    compiler, timer = shutil.which('cc'), []
    if tool := shutil.which('time'):
        probe = subprocess.run([tool,'--version'],capture_output=True,text=True)
        if probe.returncode == 0 and 'GNU' in probe.stdout+probe.stderr: timer = [tool]
    if not timer and (tool := shutil.which('busybox')):
        probe = subprocess.run([tool,'time','--help'],capture_output=True,text=True)
        if '-f FMT' in probe.stdout+probe.stderr and '-o FILE' in probe.stdout+probe.stderr: timer = [tool,'time']
    if not compiler or not timer: parser.error('requires cc and GNU or BusyBox time')
    ref = subprocess.check_output(['git','rev-parse','--verify',args.before_ref+'^{commit}'],cwd=ROOT,text=True).strip()
    old = subprocess.check_output(['git','show',ref+':cmd/zir-c/zir_c_cache.c'],cwd=ROOT,text=True)
    source = (ROOT/'bench/native_cache_names.c').read_text()
    marker = '#include "../cmd/zir-c/zir_c_cache.c"'
    assert source.count(marker)==1
    common = [compiler,'-O2','-D_GNU_SOURCE','-std=c11','-Iinclude','-Icmd/zir','-Icmd/zir-c']
    subprocess.run([*common,'-x','c','-','-x','none','build/libziran.a','-lm','-pthread','-o',str(out/'before')],input=source.replace(marker,old),text=True,cwd=ROOT,env=env,check=True)
    subprocess.run([*common,'bench/native_cache_names.c','build/libziran.a','-lm','-pthread','-o',str(out/'current')],cwd=ROOT,env=env,check=True)
    sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    meta = dict(schema=1,platform=platform.platform(),before_ref=ref,repetitions=args.repetitions,measurement_launcher=timer,
                cpu_affinity=sorted(os.sched_getaffinity(0)) if hasattr(os,'sched_getaffinity') else None,
                compiler_version=subprocess.check_output([compiler,'--version'],text=True),
                before_cache_sha256=hashlib.sha256(old.encode()).hexdigest(),current_cache_sha256=sha(ROOT/'cmd/zir-c/zir_c_cache.c'),
                benchmark_c_sha256=sha(ROOT/'bench/native_cache_names.c'),benchmark_py_sha256=sha(Path(__file__)),
                binaries={label:sha(out/label) for label in ('before','current')},
                caveats=['Ordered-name insertion/duplicate checking, content verification and cleanup in one native process; not whole compiler time.',
                         'Common path prefixes and large sets stress the previous linear duplicate scans and repeated array growth.',
                         'Process RSS includes the harness and its inputs; it does not isolate container allocations or whole compiler memory.'])
    rows=[]
    for count in (16,256,2048,8192):
        rounds=max(1,8192//count);expected=expected_checksum(count)
        for repetition in range(args.repetitions+1):
            order=['before','current'];random.Random(count+repetition).shuffle(order)
            for label in order:
                rss=out/'rss.txt'
                result=subprocess.run([*timer,'-f','%M','-o',str(rss),str(out/label),str(count),str(rounds)],env=env,capture_output=True,text=True,check=True,timeout=60)
                sample=json.loads(result.stdout)
                assert sample['count']==count and sample['rounds']==rounds and sample['checksum']==expected,sample
                row=dict(count=count,rounds=rounds,variant=label,repetition=repetition,warmup=repetition==0,
                         container_ms=sample['total_ns']/rounds/1e6,max_rss_kib=int(rss.read_text()),checksum=expected)
                rows.append(row)
                with (out/'samples.jsonl').open('a') as f:f.write(json.dumps(row)+'\n')
        print('validated ordered names '+str(count),flush=True)
    summary=[]
    for count in (16,256,2048,8192):
        for label in ('before','current'):
            values=[row for row in rows if row['count']==count and row['variant']==label and not row['warmup']]
            summary.append(dict(count=count,variant=label,samples=len(values),median_container_ms=statistics.median(row['container_ms'] for row in values),median_max_rss_kib=statistics.median(row['max_rss_kib'] for row in values)))
    (out/'metadata.json').write_text(json.dumps(meta,indent=2)+'\n')
    (out/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(f'{sum(not row["warmup"] for row in rows)} ordered-name samples validated against independent checksums.',flush=True)


if __name__=='__main__': main()
