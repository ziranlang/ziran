#!/usr/bin/env python3
"""Compare complete source/saved-IR builds of Ziran's formatter application."""
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
import time

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--before-bin-dir', type=Path, required=True)
    parser.add_argument('--bin-dir', type=Path, default=ROOT/'build/bin')
    parser.add_argument('--repetitions', type=int, default=9)
    parser.add_argument('--cflags', default='-O2 -std=c11 -D_GNU_SOURCE')
    args = parser.parse_args()
    if args.repetitions < 1: parser.error('repetitions must be positive')
    out, before, current = args.out.resolve(), args.before_bin_dir.resolve(), args.bin_dir.resolve()
    if out.exists(): parser.error('preserve receipts and remove inactive outputs before reuse')
    out.mkdir(parents=True)
    env = dict(os.environ,CC=shutil.which('cc'),CFLAGS=args.cflags,LDFLAGS='',LDLIBS='')
    for key in ('DISPLAY','WAYLAND_DISPLAY','XAUTHORITY','DBUS_SESSION_BUS_ADDRESS',
                'ZIRAN_PROFILE','ZIRAN_VM_PROFILE','MAKEFLAGS','MFLAGS'):
        env.pop(key,None)
    digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    launcher=[]
    if tool:=shutil.which('time'):
        probe=subprocess.run([tool,'--version'],capture_output=True,text=True)
        if probe.returncode==0 and 'GNU' in probe.stdout+probe.stderr:launcher=[tool]
    if not launcher and (tool:=shutil.which('busybox')):
        probe=subprocess.run([tool,'time','--help'],capture_output=True,text=True)
        if '-f FMT' in probe.stdout+probe.stderr and '-o FILE' in probe.stdout+probe.stderr:launcher=[tool,'time']
    if not launcher: parser.error('GNU or BusyBox time is required for child RSS')
    if not env['CC'] or not hasattr(os,'wait4'):parser.error('requires cc and POSIX wait4')
    metadata=dict(schema=1,platform=platform.platform(),repetitions=args.repetitions,discarded_warmups=1,
                  cpu_affinity=sorted(os.sched_getaffinity(0)) if hasattr(os, 'sched_getaffinity') else None,
                  flags={k:env[k] for k in ('CC','CFLAGS','LDFLAGS','LDLIBS')},
                  versions=dict(cc=subprocess.check_output([env['CC'],'--version'],text=True)),
                  tool_hashes={label:{name:digest(directory/name) for name in ('ziran','zi2c')}
                               for label,directory in [('before',before),('current',current)]},
                  checker_sha256=digest(current/'zi2zir'),reference_formatter_sha256=digest(current/'zi-fmt'),
                  source_hashes={str(p.relative_to(ROOT)):digest(p) for folder in ('cmd','std','include')
                                 for p in (ROOT/folder).rglob('*') if p.is_file() and p.suffix in ('.zi','.c','.h')},
                  generator_sha256=digest(Path(__file__)),profiles=[],output_identity=[],
                  caveats=['One Linux formatter application on one shared host, warm toolchain/filesystem caches.',
                           'Fresh samples remove this harness\'s generated outputs; unchanged samples retain them.',
                           'Source/saved builds share the same current checker; native compiler before/after differ.',
                           'Profiles run separately; their environment changes the native-cache identity.',
                           'Peak RSS is the maximum child/process value, not total concurrent compiler memory.',
                           'Formatting results and native output identity are checked; this is not a formatter-runtime benchmark.'])
    def plain(argv,selected=env):
        return subprocess.run(list(map(str,argv)),cwd=ROOT,env=selected,capture_output=True,text=True,check=True,timeout=180)
    plain([current/'ziran','ir','--root',ROOT/'cmd','--module-path',ROOT/'std','-o',out/'ir',ROOT/'cmd/format.zi'])
    probe=out/'probe.zi'
    unformatted='// café 日本語\nPoint::struct { x:s32; y:s32 }\nAnswer::()->s32 {\nvalue:=Point.{17,25}\nreturn value.x+value.y\n}\n'
    probe.write_text(unformatted);plain([current/'zi-fmt',probe]);expected=probe.read_bytes()
    assert expected!=unformatted.encode()
    metadata['probe_expected_sha256']=digest(probe)
    rows=[]
    for form in ('source','saved'):
        entry=ROOT/'cmd/format.zi' if form=='source' else out/'ir/format.zir'
        source_root=ROOT/'cmd' if form=='source' else out/'ir'
        outputs={label:out/form/label for label in ('before','current')}
        def command(label):
            tools=before if label=='before' else current
            return [tools/'ziran','build','--target=c','--exe','--entry','format:main','--root',source_root,
                    '--module-path',ROOT/'std','-o',outputs[label],entry]
        def validate(label):
            probe.write_text(unformatted)
            plain([outputs[label]/'format',probe]);assert probe.read_bytes()==expected
            plain([outputs[label]/'format','--check',probe])
        # Warm each toolchain, then profile a separate build after warming.
        for label in ('before','current'):
            plain(command(label));validate(label)
            profile=out/(form+'-'+label+'-profile.jsonl')
            plain(command(label),dict(env,ZIRAN_PROFILE=str(profile)))
            metadata['profiles'].append(dict(form=form,variant=label,
                records=[json.loads(line) for line in profile.read_text().splitlines()]))
        for phase in ('fresh','unchanged'):
            for label in outputs:plain(command(label))
            for repetition in range(args.repetitions+1):
                order=list(outputs);random.Random((1 if form=='saved' else 0)*1000+repetition+len(phase)).shuffle(order)
                for label in order:
                    if phase=='fresh':shutil.rmtree(outputs[label])
                    rss=out/'rss.txt';argv=list(map(str,command(label)))
                    with (out/'stdout.txt').open('wb') as stdout,(out/'stderr.txt').open('wb') as stderr:
                        started=time.perf_counter_ns()
                        child=subprocess.Popen([*launcher,'-f','%M','-o',str(rss),*argv],env=env,cwd=ROOT,stdout=stdout,stderr=stderr)
                        _,status,usage=os.wait4(child.pid,0);child.returncode=os.waitstatus_to_exitcode(status)
                        elapsed=(time.perf_counter_ns()-started)/1e6
                    assert child.returncode==0,(out/'stderr.txt').read_text()
                    row=dict(form=form,variant=label,phase=phase,repetition=repetition,warmup=repetition==0,
                             argv=argv,elapsed_ms=elapsed,user_ms=usage.ru_utime*1000,system_ms=usage.ru_stime*1000,
                             max_rss_kib=int(rss.read_text()),returncode=child.returncode)
                    validate(label);row['formatting_verified']=True
                    rows.append(row)
                    with (out/'samples.jsonl').open('a') as f:f.write(json.dumps(row)+'\n')
        generated=lambda directory:{str(p.relative_to(directory)):digest(p) for p in directory.rglob('*') if p.suffix in ('.c','.h')}
        equality=dict(form=form,generated_c=generated(outputs['before'])==generated(outputs['current']),
                      executable=digest(outputs['before']/'format')==digest(outputs['current']/'format'))
        assert equality['generated_c'] and equality['executable'],equality
        metadata['output_identity'].append(equality)
    summary=[]
    for form in ('source','saved'):
        for phase in ('fresh','unchanged'):
            for label in ('before','current'):
                selected=[r for r in rows if not r['warmup'] and r['form']==form and r['phase']==phase and r['variant']==label]
                summary.append(dict(form=form,phase=phase,variant=label,samples=len(selected),**{
                    'median_'+key:statistics.median(r[key] for r in selected)
                    for key in ('elapsed_ms','user_ms','system_ms','max_rss_kib')}))
    (out/'metadata.json').write_text(json.dumps(metadata,indent=2)+'\n')
    (out/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(f'{sum(not r["warmup"] for r in rows)} complete formatter builds measured; source/saved formatting and native output identity passed.')


if __name__=='__main__':main()
