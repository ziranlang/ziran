#!/usr/bin/env python3
"""Measure complete native builds under controlled probe-version interleaving."""
import argparse
import ast
import hashlib
import json
import os
from pathlib import Path
import random
import shlex
import shutil
import statistics
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--before-bin-dir', type=Path, required=True)
    parser.add_argument('--bin-dir', type=Path, default=ROOT/'build/bin')
    parser.add_argument('--repetitions', type=int, default=15)
    args = parser.parse_args()
    if args.repetitions < 1: parser.error('repetitions must be positive')
    out = args.out.resolve()
    if out.exists(): parser.error('archive records and remove inactive outputs before reuse')
    out.mkdir(parents=True)
    env = {k:v for k,v in os.environ.items() if k not in ('DISPLAY','WAYLAND_DISPLAY','XAUTHORITY','DBUS_SESSION_BUS_ADDRESS','ZIRAN_PROFILE','ZIRAN_VM_PROFILE','MAKEFLAGS','MFLAGS')}
    env['YUE_DESKTOP_RECOVERY'] = '0'
    compiler, timer = shutil.which('cc'), []
    if tool := shutil.which('time'):
        result = subprocess.run([tool,'--version'],capture_output=True,text=True)
        if result.returncode == 0 and 'GNU' in result.stdout+result.stderr: timer = [tool]
    if not timer and (tool := shutil.which('busybox')):
        result = subprocess.run([tool,'time','--help'],capture_output=True,text=True)
        if '-f FMT' in result.stdout+result.stderr: timer = [tool,'time']
    if not compiler or not timer: parser.error('requires cc and GNU or BusyBox time')
    sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()

    # Reuse the regression's real C compiler shim and five-object FFI fixture.
    # Execute only its setup, before the first build or any test mutations.
    test = ROOT/'tests/native_build_cache_test.py'
    tree = ast.parse(test.read_text(),str(test))
    setup = []
    for node in tree.body:
        if isinstance(node,ast.Expr) and isinstance(node.value,ast.Call) and isinstance(node.value.func,ast.Name) and node.value.func.id == 'build': break
        setup.append(node)
    assert len(setup) < len(tree.body)
    helper = [node.value.args[0].value for node in tree.body
              if isinstance(node,ast.Expr) and isinstance(node.value,ast.Call) and isinstance(node.value.func,ast.Attribute)
              and isinstance(node.value.func.value,ast.Name) and node.value.func.value.id == 'proxy_source' and node.value.func.attr == 'write_text']
    assert len(helper) == 1
    shim = out/'gcc'
    subprocess.run([compiler,'-D_GNU_SOURCE','-DREVISION=0','-DREAL_CC='+json.dumps(compiler),'-x','c','-','-o',shim],input=helper[0],text=True,env=env,check=True)
    cases = {}
    prefix = '\tcompiled by GNU C version 99.0.0, '
    banner = 'GNU assembler version 2.44 (target) using BFD version 2.44\n'
    for label,tools in [('before',args.before_bin_dir.resolve()),('current',args.bin_dir.resolve())]:
        work = out/label; work.mkdir()
        scope = {'__file__':str(test),'__name__':'fixture'}
        original = sys.argv
        try:
            sys.argv = [str(test),str(tools/'ziran'),str(work)]
            exec(compile(ast.Module(body=setup,type_ignores=[]),str(test),'exec'),scope)
        finally: sys.argv = original
        scope['env'].update(env)
        scope['env'].update(CC=str(shim),CFLAGS='-O2 -isystem '+str(scope['headers']),LDFLAGS='-L'+str(work),LDLIBS='-lbase')
        probe = subprocess.run([compiler,*shlex.split(scope['env']['CFLAGS']),'-I',scope['out'],'-pthread','-I',ROOT/'include','-pipe','-v','-x','c','-c','/dev/null','-o','/dev/null'],env=scope['env'],capture_output=True,text=True,check=True).stderr
        saved = work/'probe.txt';scope['env']['CACHE_PROBE_TEXT'] = str(saved)
        cases[label] = dict(scope=scope,probe=probe,saved=saved)

    def write_probe(label,interleaved,version):
        case = cases[label]
        libraries = f'GMP version {version}, MPFR version 4.2.1, MPC version 1.3.1, isl version 0.27\n'
        case['saved'].write_text(case['probe']+prefix+(banner+libraries if interleaved else libraries+banner))

    for label,case in cases.items():
        write_probe(label,False,'6.3.0')
        case['scope']['build'](42)
        case['scope']['build'](42,set())
        assert len(case['scope']['objects']()) == 5
    rows=[]
    for phase in ['stable','alternating','version-change']:
        for repetition in range(args.repetitions+1):
            labels=list(cases);random.Random(repetition+len(phase)).shuffle(labels)
            for label in labels:
                scope=cases[label]['scope']
                interleaved=phase!='stable' and repetition%2==0
                version='6.3.0' if phase!='version-change' else '7.0.'+str(repetition)
                write_probe(label,interleaved,version)
                before=scope['objects']();rss=out/'rss.txt'
                started=time.perf_counter_ns()
                with (out/'stdout.txt').open('wb') as stdout,(out/'stderr.txt').open('wb') as stderr:
                    child=subprocess.Popen([*timer,'-f','%M','-o',str(rss),*scope['command']],cwd=ROOT,env=scope['env'],stdout=stdout,stderr=stderr)
                    _,status,usage=os.wait4(child.pid,0);child.returncode=os.waitstatus_to_exitcode(status)
                elapsed=(time.perf_counter_ns()-started)/1e6
                assert child.returncode==0,(out/'stderr.txt').read_text()
                after=scope['objects']();changed=sum(before.get(name)!=value for name,value in after.items())
                expected=5 if phase=='version-change' or (phase=='alternating' and label=='before') else 0
                assert changed==expected,(label,phase,repetition,changed,expected)
                assert subprocess.run([scope['out']/'app'],env=scope['env']).returncode==42
                row=dict(variant=label,phase=phase,repetition=repetition,warmup=repetition==0,interleaved=interleaved,library_version=version,objects_recompiled=changed,
                         elapsed_ms=elapsed,user_ms=usage.ru_utime*1000,system_ms=usage.ru_stime*1000,max_rss_kib=int(rss.read_text()),oracle_passed=True)
                rows.append(row)
                with (out/'samples.jsonl').open('a') as f:f.write(json.dumps(row)+'\n')
        print('validated complete probe fixture '+phase,flush=True)
    summaries=[]
    for phase in ['stable','alternating','version-change']:
        for label in cases:
            selected=[r for r in rows if r['phase']==phase and r['variant']==label and not r['warmup']]
            summaries.append(dict(phase=phase,variant=label,samples=len(selected),**{'median_'+k:statistics.median(r[k] for r in selected) for k in ['elapsed_ms','user_ms','system_ms','max_rss_kib','objects_recompiled']}))
    generated=lambda label:{p.name:sha(p) for p in cases[label]['scope']['out'].iterdir() if p.suffix in ['.c','.h']}
    identity=dict(generated_c=generated('before')==generated('current'),executable=sha(cases['before']['scope']['out']/'app')==sha(cases['current']['scope']['out']/'app'))
    assert all(identity.values()),identity
    metadata=dict(schema=1,repetitions=args.repetitions,compiler=compiler,compiler_version=subprocess.check_output([compiler,'--version'],text=True),measurement_launcher=timer,
                  cpu_affinity=sorted(os.sched_getaffinity(0)) if hasattr(os,'sched_getaffinity') else None,generator_sha256=sha(Path(__file__)),fixture_sha256=sha(test),shim_sha256=sha(shim),
                  source_hashes={str(p.relative_to(ROOT)):sha(p) for folder in ['cmd','std','include'] for p in (ROOT/folder).rglob('*') if p.is_file() and p.suffix in ['.c','.h','.zi']},
                  tool_hashes={label:{name:sha(tools/name) for name in ['ziran','zi2c']} for label,tools in [('before',args.before_bin_dir.resolve()),('current',args.bin_dir.resolve())]},
                  flags={label:{k:case['scope']['env'][k] for k in ['CC','CFLAGS','LDFLAGS','LDLIBS']} for label,case in cases.items()},output_identity=identity,
                  caveats=['A five-object source/FFI regression fixture, with a native shim serving a saved real probe plus controlled version records.',
                           'Stable messages, alternating within-line assembler interleaving, and genuinely changed GMP versions are separate phases.',
                           'Real object compilation and linking are timed; the shim reads saved diagnostics instead of invoking the ordinary probe subprocesses.',
                           'Alternating messages are forced on every sample, not an estimate of their real-world frequency. Run ordinary native-build controls separately.',
                           'One shared warm-cache host. Peak process/child RSS is not summed concurrent compiler memory.'])
    (out/'metadata.json').write_text(json.dumps(metadata,indent=2)+'\n')
    (out/'summary.json').write_text(json.dumps(summaries,indent=2)+'\n')
    print(f'{sum(not r["warmup"] for r in rows)} complete controlled-probe builds passed their runtime, cache and native-identity checks.',flush=True)


if __name__=='__main__':main()
