#!/usr/bin/env python3
"""Compare complete native builds with a dense, foreign-visible record ABI."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import random
import shutil
import statistics
import struct
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]
GROUPS, WIDTH, CONSUMERS = 8, 64, 32


def generate(source, groups=GROUPS, width=WIDTH, consumers=CONSUMERS, read_last=False):
    GROUPS, WIDTH, CONSUMERS = groups, width, consumers
    source.mkdir(parents=True)
    for number in range(GROUPS * WIDTH):
        (source/f'leaf{number:04d}.zi').write_text(f'#scope_export\nLeaf{number:04d} :: struct {{ value: u32 }}\n')
    for group in range(GROUPS):
        text = '#scope_export\n'+''.join(f'L{i} :: #import "leaf{group * WIDTH + i:04d}";\n' for i in range(WIDTH))
        text += f'Group{group:02d} :: struct {{\n'+''.join(f'l{i}: L{i}.Leaf{group * WIDTH + i:04d};\n' for i in range(WIDTH))+'}\n'
        (source/f'group{group:02d}.zi').write_text(text)
    text = '#scope_export\n'+''.join(f'G{i} :: #import "group{i:02d}";\n' for i in range(GROUPS))
    text += 'Bundle :: struct {\n'+''.join(f'g{i}: G{i}.Group{i:02d};\n' for i in range(GROUPS))+'}\n'
    (source/'common.zi').write_text(text)
    field = f'g{GROUPS - 1}.l{WIDTH - 1}' if read_last else 'g0.l0'
    for number in range(CONSUMERS):
        (source/f'consumer{number:02d}.zi').write_text('#scope_export\nCommon :: #import "common";\n'
            'Read :: (value: *Common.Bundle) -> s64 {\nif value == null { return 0 }\n'
            f'return cast(s64)(value.{field}.value) * {number + 1}\n}}\n')
    text = '#scope_export\nCommon :: #import "common";\n'+''.join(f'C{i} :: #import "consumer{i:02d}";\n' for i in range(CONSUMERS))
    text += ('lib :: #system_library "c";\nhost :: #system_library "host";\n'
             'Parse :: (text: *u8) -> s32 #foreign lib "atoi";\n'
             'Prepare :: (value: *Common.Bundle, seed: u32) -> u32 #foreign host "prepare";\n'
             '#program_export\nmain :: (argc: s32, argv: **u8) -> s32 {\nif argc != 2 { return 2 }\n'
             'seed := Parse(argv[1])\nvalue: Common.Bundle\nvalue.g0.l0.value = cast(u32)(seed)\n'
             'result: s64 = cast(s64)(Prepare(*value, cast(u32)(seed)))\n')
    text += ''.join(f'result += C{i}.Read(*value)\n' for i in range(CONSUMERS))
    text += 'print("%\\n", result)\nreturn 0\n}\n'
    (source/'app.zi').write_text(text)


def host_source(groups=GROUPS, width=WIDTH):
    GROUPS, WIDTH = groups, width
    size = GROUPS * WIDTH * 4
    return ('#include "common.h"\n'
            f'_Static_assert(sizeof(Bundle) == {size}, "full foreign ABI");\n'
            f'_Static_assert(offsetof(Bundle, g{GROUPS - 1}.l{WIDTH - 1}.value) == {size - 4}, "last ABI field");\n'
            f'uint32_t prepare(Bundle *value, uint32_t seed) {{ value->g{GROUPS - 1}.l{WIDTH - 1}.value = seed + 3; '
            f'return value->g{GROUPS - 1}.l{WIDTH - 1}.value; }}\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--before-bin-dir', type=Path, required=True)
    parser.add_argument('--bin-dir', type=Path, default=ROOT/'build/bin')
    parser.add_argument('--repetitions', type=int, default=15)
    parser.add_argument('--groups', type=int, default=GROUPS)
    parser.add_argument('--width', type=int, default=WIDTH)
    parser.add_argument('--read-last', action='store_true', help='read the final nested field, requiring late-module qualification')
    args = parser.parse_args()
    if args.repetitions < 1 or not 1 <= args.groups <= 8 or not 1 <= args.width <= 64:
        parser.error('positive repetitions, groups 1..8 and width 1..64 required')
    out, before, current = args.out.resolve(), args.before_bin_dir.resolve(), args.bin_dir.resolve()
    if out.exists(): parser.error('archive records and remove inactive outputs before reuse')
    out.mkdir(parents=True)
    env = {k:v for k,v in os.environ.items() if k not in ('DISPLAY','WAYLAND_DISPLAY','XAUTHORITY','DBUS_SESSION_BUS_ADDRESS','MAKEFLAGS','MFLAGS','ZIRAN_PROFILE')}
    env.update(CC=shutil.which('cc'), CFLAGS='-O2 -std=c11', LDFLAGS='', LDLIBS='', YUE_DESKTOP_RECOVERY='0')
    timer = []
    if tool := shutil.which('time'):
        probe = subprocess.run([tool,'--version'],capture_output=True,text=True)
        if probe.returncode == 0 and 'GNU' in probe.stdout+probe.stderr: timer = [tool]
    if not timer and (tool := shutil.which('busybox')):
        probe = subprocess.run([tool,'time','--help'],capture_output=True,text=True)
        if '-f FMT' in probe.stdout+probe.stderr and '-o FILE' in probe.stdout+probe.stderr: timer = [tool,'time']
    if not timer or not env['CC']: parser.error('requires cc and GNU or BusyBox time')
    sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    source = out/'source'; generate(source,args.groups,args.width,CONSUMERS,args.read_last)
    host = host_source(args.groups,args.width)
    def plain(argv): return subprocess.run(list(map(str,argv)),env=env,cwd=ROOT,capture_output=True,text=True,check=True,timeout=180)
    plain([current/'ziran','ir','--root',source,'-o',out/'ir',source/'app.zi'])
    metadata = dict(schema=1,platform=platform.platform(),repetitions=args.repetitions,groups=args.groups,width=args.width,consumers=CONSUMERS,read_last=args.read_last,measurement_launcher=timer,
                    cpu_affinity=sorted(os.sched_getaffinity(0)) if hasattr(os,'sched_getaffinity') else None,
                    flags={k:env[k] for k in ('CC','CFLAGS','LDFLAGS','LDLIBS')},
                    tool_hashes={label:{name:sha(folder/name) for name in ('ziran','zi2c')} for label,folder in [('before',before),('current',current)]},
                    source_hashes={str(p.relative_to(ROOT)):sha(p) for folder in ('cmd','std','include') for p in (ROOT/folder).rglob('*') if p.is_file() and p.suffix in ('.c','.h','.zi')},
                    generator_sha256=sha(Path(__file__)),output_identity=[],dependency_counts=[],
                    fixture_hashes={str(p.relative_to(source)):sha(p) for p in source.iterdir()},host_sha256=hashlib.sha256(host.encode()).hexdigest(),
                    caveats=[f'One generated Ziran/C-interop program with {args.groups * args.width} leaf records; warm shared-host filesystem/toolchain caches.',
                             f'Foreign signature and C static assertions retain the complete {args.groups * args.width * 4}-byte layout and final field offset.',
                             'Fresh samples remove owned generated output; source inputs stay identical, so this is not a cold-machine benchmark.',
                             'Before/after are interleaved with identical compiler flags; this is not a Go or C language ranking.',
                             'Peak RSS is the maximum child/process, not summed concurrent memory.'])
    rows=[]
    for form in ('source','saved'):
        entry=source/'app.zi' if form=='source' else out/'ir/app.zir'
        source_root=source if form=='source' else out/'ir'
        outputs={label:out/form/label for label in ('before','current')}
        def command(label):
            tools=before if label=='before' else current
            return [tools/'ziran','build','--target=c','--exe','--entry','app:main','--root',source_root,'-o',outputs[label],entry]
        def prepare(label):
            outputs[label].mkdir(parents=True,exist_ok=True)
            (outputs[label]/'host.c').write_text(host)
        def validate(label):
            for seed in (7,9):
                result=plain([outputs[label]/'app',seed])
                value = seed + 3 if args.read_last or args.groups * args.width == 1 else seed
                assert result.stdout==f'{value * CONSUMERS * (CONSUMERS + 1) // 2 + seed + 3}\n',result.stdout
        for label in outputs: prepare(label);plain(command(label));validate(label)
        for phase in ('fresh','unchanged'):
            for repetition in range(args.repetitions+1):
                order=list(outputs);random.Random(repetition+len(phase)+(100 if form=='saved' else 0)).shuffle(order)
                for label in order:
                    if phase=='fresh':shutil.rmtree(outputs[label]);prepare(label)
                    rss=out/'rss.txt';argv=list(map(str,command(label)))
                    with (out/'stdout.txt').open('wb') as stdout,(out/'stderr.txt').open('wb') as stderr:
                        started=time.perf_counter_ns()
                        child=subprocess.Popen([*timer,'-f','%M','-o',str(rss),*argv],env=env,cwd=ROOT,stdout=stdout,stderr=stderr)
                        _,status,usage=os.wait4(child.pid,0);child.returncode=os.waitstatus_to_exitcode(status)
                        elapsed=(time.perf_counter_ns()-started)/1e6
                    assert child.returncode==0,(out/'stderr.txt').read_text()
                    validate(label)
                    row=dict(form=form,phase=phase,variant=label,repetition=repetition,warmup=repetition==0,argv=argv,
                             elapsed_ms=elapsed,user_ms=usage.ru_utime*1000,system_ms=usage.ru_stime*1000,max_rss_kib=int(rss.read_text()),oracle_passed=True)
                    rows.append(row)
                    with (out/'samples.jsonl').open('a') as f:f.write(json.dumps(row)+'\n')
            print('validated dense headers '+form+'/'+phase,flush=True)
        for label in outputs:
            stamps=list((outputs[label]/'.objects').glob('consumer*.c.o.inputs'))
            counts=[struct.unpack_from('<I',p.read_bytes(),112)[0] for p in stamps]
            assert len(counts)==CONSUMERS and min(counts)>=args.groups*args.width,counts
            metadata['dependency_counts'].append(dict(form=form,variant=label,minimum=min(counts),maximum=max(counts)))
        generated=lambda folder:{str(p.relative_to(folder)):sha(p) for p in folder.rglob('*') if p.suffix in ('.c','.h')}
        identity=dict(form=form,generated_c=generated(outputs['before'])==generated(outputs['current']),executable=sha(outputs['before']/'app')==sha(outputs['current']/'app'))
        assert identity['generated_c'] and identity['executable'],identity
        metadata['output_identity'].append(identity)
    summary=[]
    for form in ('source','saved'):
        for phase in ('fresh','unchanged'):
            for label in ('before','current'):
                values=[r for r in rows if r['form']==form and r['phase']==phase and r['variant']==label and not r['warmup']]
                summary.append(dict(form=form,phase=phase,variant=label,samples=len(values),**{'median_'+k:statistics.median(r[k] for r in values) for k in ('elapsed_ms','user_ms','system_ms','max_rss_kib')}))
    (out/'metadata.json').write_text(json.dumps(metadata,indent=2)+'\n')
    (out/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(f'{sum(not r["warmup"] for r in rows)} complete dense-header builds measured; source/saved oracles, foreign ABI and native output identity passed.',flush=True)


if __name__=='__main__': main()
