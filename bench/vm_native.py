#!/usr/bin/env python3
"""Calibrate VM workloads against matched generated C and Go execution."""
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

from vm_execution import MASK, ROOT, workloads


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--vm-results', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--bin-dir', type=Path, default=ROOT/'build/bin')
    parser.add_argument('--runs', type=int, default=128)
    parser.add_argument('--repetitions', type=int, default=9)
    args = parser.parse_args()
    if not 1 <= args.runs <= 10000 or args.repetitions < 1:
        parser.error('runs must be 1..10000 and repetitions must be positive')
    out, vm, tools = args.out.resolve(), args.vm_results.resolve(), args.bin_dir.resolve()
    if out.exists():
        parser.error('preserve receipts and remove inactive outputs before reuse')
    out.mkdir(parents=True)
    env = dict(os.environ, GO111MODULE='off', GOMAXPROCS='1', CGO_ENABLED='0',
               GOCACHE=str(out/'go-cache'))
    for key in ('DISPLAY','WAYLAND_DISPLAY','XAUTHORITY','DBUS_SESSION_BUS_ADDRESS',
                'ZIRAN_PROFILE','ZIRAN_VM_PROFILE','GOFLAGS','GOEXPERIMENT'):
        env.pop(key, None)
    if platform.machine() == 'x86_64': env['GOAMD64'] = 'v1'
    cc = shutil.which(os.environ.get('CC','cc'))
    if not cc or not shutil.which('go'):
        parser.error('C and Go compilers are required')
    launcher = []
    if executable := shutil.which('time'):
        probe = subprocess.run([executable,'--version'],capture_output=True,text=True)
        if probe.returncode == 0 and 'GNU' in probe.stdout+probe.stderr:
            launcher = [executable]
    if not launcher and (executable := shutil.which('busybox')):
        probe = subprocess.run([executable,'time','--help'],capture_output=True,text=True)
        if '-f FMT' in probe.stdout+probe.stderr and '-o FILE' in probe.stdout+probe.stderr:
            launcher = [executable,'time']
    if not launcher: parser.error('GNU or BusyBox time is required for child peak RSS')
    digest = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
    manifest = json.loads((vm/'fixtures.json').read_text())
    vm_metadata = json.loads((vm/'comparison/metadata.json').read_text())
    vm_cases = {case['name']:case for case in vm_metadata['cases']}
    expected = {name:(source,answer) for name,source,answer in workloads(manifest['iterations'])}
    assert {case['name'] for case in manifest['cases']} == set(expected)
    assert set(vm_cases) == set(expected)
    metadata = dict(schema=1,created_utc=time.strftime('%Y-%m-%dT%H:%M:%SZ',time.gmtime()),
                    platform=platform.platform(), runs=args.runs, repetitions=args.repetitions,
                    discarded_warmups=1,commands=[],artifacts=[],cases=[],
                    vm_results=str(vm),vm_metadata_sha256=digest(vm/'comparison/metadata.json'),
                    cc=subprocess.check_output([cc,'--version'],text=True).splitlines()[0],
                    go=subprocess.check_output(['go','version'],text=True).strip(),
                    go_runtime=dict(GOMAXPROCS='1',CGO_ENABLED='0',GOAMD64=env.get('GOAMD64')),
                    tool_hashes={n:digest(tools/n) for n in ('ziran','zi2zir','zi2c','zi2go')},
                    source_hashes={n:digest(ROOT/n) for n in ('bench/vm_native.py','bench/vm_execution.py',
                                  'bench/android_native.c','bench/android_native.go')},
                    caveats=['Matched generated C/Go controls, not independent handwritten implementations.',
                             'Both native controls use checked bodies; C defines ZIRAN_NO_ASM and Go disables cgo.',
                             'Native timers cover a batch of Answer calls; normalized timings include the call and checksum loop.',
                             'CPU usage for very short batches can quantize to zero; increase --runs for CPU comparisons.',
                             'VM steady medians come from separate repeated-instance measurements; native batch sizes differ.',
                             'Peak RSS includes runtime/process startup and is measured by a separate child launcher.',
                             'Go keeps ordinary GC and runs with GOMAXPROCS=1; GC and allocation are part of the workload.',
                             'Shared host and warm caches, without CPU isolation; no whole-application or language-ranking claim.'])
    def invoke(argv, cwd=ROOT):
        started = time.perf_counter_ns()
        result = subprocess.run(list(map(str,argv)),cwd=cwd,env=env,check=True,
                                capture_output=True,text=True,timeout=180)
        metadata['commands'].append(dict(argv=list(map(str,argv)),cwd=str(cwd),
                                         elapsed_ms=(time.perf_counter_ns()-started)/1e6))
        return result.stdout
    binaries=[]
    for case in manifest['cases']:
        name=case['name'];module=name.replace('-','_');source,answer=expected[name]
        assert (vm/'fixtures'/(module+'.zi')).read_text()==source
        assert answer==case['expected']
        measured = vm_cases[name]
        assert measured['expected']==answer
        assert measured['source_sha256']==digest(vm/'fixtures'/(module+'.zi'))
        assert measured['bundle_sha256']==digest(vm/'fixtures'/(module+'.zx'))
        checked=(0x12345678+args.runs*(answer & MASK)) & MASK
        metadata['cases'].append(dict(name=name,answer=answer,batch_result=checked,
                                      source_sha256=digest(vm/'fixtures'/(module+'.zi'))))
        for target in ('c','go'):
            original=None
            for form in ('source','saved'):
                directory=out/name/(target+'-'+form)
                base=vm/'fixtures' if form=='source' else vm/'ir'/module
                entry=base/(module+('.zi' if form=='source' else '.zir'))
                invoke([tools/'ziran','build','--target='+target,*(['--pkg','main'] if target=='go' else []),
                        '--root',base,'--module-path',ROOT/'std','-o',directory,entry])
                generated={p.name:digest(p) for p in directory.iterdir() if p.suffix in ('.c','.h','.go')}
                if original is None: original=generated
                else: assert original==generated,(name,target,'source/saved output differs')
                binary=directory/'run'
                if target=='c':
                    driver=(ROOT/'bench/android_native.c').read_text().replace(
                        '#include "kernel.h"','#include "'+module+'.h"\nuint64_t benchKernel(int64_t, uint64_t);').replace(
                        ' = Kernel(', ' = benchKernel(')
                    driver+='\nuint64_t benchKernel(int64_t rounds, uint64_t seed) {\n    uint64_t result = seed;\n    for(int64_t i=0; i<rounds; i++) result += (uint64_t)Answer();\n    return result;\n}\n'
                    (directory/'bench.c').write_text(driver)
                    invoke([cc,'-O2','-std=c11','-D_GNU_SOURCE','-DZIRAN_NO_ASM','-I'+str(directory),
                            *sorted(directory.glob('*.c')),'-lm','-o',binary])
                else:
                    code=(directory/(module+'.go')).read_text()
                    function=re.search(r'^func (\w*Answer)\(\) int64',code,re.M)
                    assert function,(name,'Go Answer entry missing')
                    (directory/'main.go').write_text((ROOT/'bench/android_native.go').read_text()+
                        '\nfunc benchKernel(rounds int64, seed uint64) uint64 {\n    result := seed\n    for i:=int64(0); i<rounds; i++ { result += uint64('+function[1]+'()) }\n    return result\n}\n')
                    invoke(['go','build','-trimpath','-o',binary,'.'],directory)
                row=json.loads(invoke([binary,args.runs,0x12345678]))
                assert row['result']==checked,(name,target,form,row)
                metadata['artifacts'].append(dict(case=name,target=target,form=form,sha256=digest(binary),
                                                  bytes=binary.stat().st_size,source_hashes=generated,
                                                  checked_result=row['result']))
                if form=='source':binaries.append(dict(case=name,target=target,binary=binary,expected=checked))
    metadata['source_saved_native_output_identical']=True
    (out/'metadata.json').write_text(json.dumps(metadata,indent=2)+'\n')
    trials=[(binary,repetition) for repetition in range(args.repetitions+1) for binary in binaries]
    random.Random(20261016).shuffle(trials);trials.sort(key=lambda row:row[1]!=0)
    samples=[]
    with (out/'samples.jsonl').open('x') as log:
        for binary,repetition in trials:
            rss=out/'rss.txt'
            row=json.loads(invoke([*launcher,'-f','%M','-o',rss,binary['binary'],args.runs,0x12345678]))
            assert row['result']==binary['expected'],(binary,row)
            row['self_reported_max_rss_kib']=row['max_rss_kib'];row['max_rss_kib']=int(rss.read_text())
            row.update(case=binary['case'],target=binary['target'],runs=args.runs,
                       repetition=repetition,warmup=repetition==0)
            row['per_answer_ms']=row['elapsed_ms']/args.runs
            samples.append(row);log.write(json.dumps(row)+'\n');log.flush()
    vm_rows=json.loads((vm/'comparison/summary.json').read_text())
    summary=[]
    for binary in binaries:
        rows=[r for r in samples if not r['warmup'] and r['case']==binary['case'] and r['target']==binary['target']]
        steady=next(r['median_steady_run_ms'] for r in vm_rows if r['case']==binary['case'] and r['variant']=='after' and r['phase']=='vm')
        per_answer=statistics.median(r['per_answer_ms'] for r in rows)
        summary.append(dict(case=binary['case'],target=binary['target'],samples=len(rows),
                            median_per_answer_ms=per_answer,vm_median_steady_ms=steady,
                            vm_to_native_elapsed_ratio=steady/per_answer,
                            median_max_rss_kib=statistics.median(r['max_rss_kib'] for r in rows)))
    (out/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    (out/'metadata.json').write_text(json.dumps(metadata,indent=2)+'\n')
    print(f'{len(metadata["artifacts"])} source/saved native controls checked; {len(binaries)*args.repetitions} measured samples verified.')


if __name__=='__main__':main()
