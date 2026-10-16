#!/usr/bin/env python3
"""Paired compiler measurements with output identity and runtime oracles."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import random
import resource
import shutil
import statistics
import subprocess
import time

from compiler_scaling import write_workload

ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--before-bin-dir', type=Path, required=True)
    parser.add_argument('--bin-dir', type=Path, default=ROOT/'build/bin')
    parser.add_argument('--sizes', type=int, nargs='+', default=[8, 32, 128])
    parser.add_argument('--fields', type=int, nargs='+', default=[16, 64])
    parser.add_argument('--repetitions', type=int, default=9)
    parser.add_argument('--project', nargs=3, action='append', default=[], metavar=('NAME', 'ROOT', 'ENTRY'))
    args = parser.parse_args()
    if (args.repetitions < 1 or not args.sizes or not args.fields or
            len(set(args.sizes)) != len(args.sizes) or len(set(args.fields)) != len(args.fields) or
            any(not 1 <= n <= 128 for n in args.sizes) or any(not 1 <= n <= 64 for n in args.fields)):
        parser.error('unique sizes 1..128, fields 1..64 and positive repetitions required')
    out = args.out.resolve()
    if out.exists(): parser.error('preserve receipts and remove inactive outputs before reusing --out')
    out.mkdir(parents=True)
    versions = {'before': args.before_bin_dir.resolve(), 'after': args.bin_dir.resolve()}
    tools = ('ziran', 'zi2zir', 'zi2c', 'zi2zib')
    for folder in versions.values():
        for name in tools:
            if not (folder/name).is_file(): parser.error(f'missing {folder/name}')
    timer = [shutil.which('busybox'), 'time'] if shutil.which('busybox') else [shutil.which('time')]
    if not timer[0] or not shutil.which('cc'): parser.error('requires cc and GNU/BusyBox time')
    env = dict(os.environ)
    for key in ('DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'DBUS_SESSION_BUS_ADDRESS', 'ZIRAN_PROFILE'):
        env.pop(key, None)
    commands, samples, profiles, projects, artifacts = [], [], [], [], []

    def invoke(argv, cwd=ROOT, expected=None, measured=False, profile=None):
        selected = dict(env)
        if profile:
            profile.unlink(missing_ok=True)
            selected['ZIRAN_PROFILE'] = str(profile)
        argv = list(map(str, argv)); rss = out/'rss.txt'
        before = resource.getrusage(resource.RUSAGE_CHILDREN); start = time.perf_counter_ns()
        result = subprocess.run(([*timer, '-f', '%M', '-o', str(rss)] if measured else [])+argv,
                                cwd=cwd, env=selected, text=True, capture_output=True, timeout=300)
        elapsed = (time.perf_counter_ns()-start)/1e6
        after = resource.getrusage(resource.RUSAGE_CHILDREN)
        if result.returncode: raise RuntimeError(f'{argv}\n{result.stdout}{result.stderr}')
        if expected is not None: assert result.stdout.strip() == str(expected), (argv, result.stdout, expected)
        return dict(argv=argv, cwd=str(cwd), wall_ms=elapsed,
                    cpu_ms=1000*(after.ru_utime+after.ru_stime-before.ru_utime-before.ru_stime),
                    peak_rss_kib=int(rss.read_text()) if measured else None)

    def sources(directory):
        files = list(directory.rglob('*.zi'))
        files += [directory/n for n in ('ziran.toml', 'ziran.lock', 'ziran.local.toml') if (directory/n).is_file()]
        return {str(p.relative_to(directory)): digest(p) for p in files
                if not {'build', 'backup', 'vendor', '.git', '.ziran'} & set(p.relative_to(directory).parts)}

    def register(case, form, version, phase, argv, cwd=ROOT):
        commands.append(dict(case=case, form=form, version=version, phase=phase,
                             argv=list(map(str, argv)), cwd=str(cwd)))

    def contents(folder):
        return {p.name: p.read_bytes() for p in folder.iterdir() if p.suffix in ('.c', '.h')}

    for size in args.sizes:
        cases = [('modules', fields) for fields in args.fields]+[('generics', 1), ('imported-generics', 1)]
        for kind, fields in cases:
            case = f'{kind}-{size}-{fields}'; folder = out/case
            source, entry, expected = write_workload(folder, kind, size, fields)
            artifacts.append(dict(case=case, source_hashes=sources(source), expected=expected))
            for version, binaries in versions.items():
                ir = folder/('ir-'+version)
                invoke([binaries/'zi2zir', '--root', source, '-o', ir, entry])
                for form, root, input_path in [('source', source, entry), ('saved', ir, ir/'program.zir')]:
                    output = folder/version/form
                    emitted = output/'c'; bundle = output/'program.zib'
                    check = [binaries/'zi2zir', '--check-only', '--root', root, input_path]
                    emit = [binaries/'zi2c', '--root', root, '-o', emitted, input_path]
                    pack = [binaries/'zi2zib', 'bundle', '--root', root, '--entry', 'program:Answer', '-o', bundle, input_path]
                    for phase, command in [('check', check), ('emit:c', emit), ('bundle', pack)]:
                        invoke(command); register(case, form, version, phase, command)
                    invoke([binaries/'zi2zib', 'run', bundle], expected=expected)
            before_ir = folder/'ir-before'; after_ir = folder/'ir-after'
            assert {p.name:p.read_bytes() for p in before_ir.glob('*.zir')} == {p.name:p.read_bytes() for p in after_ir.glob('*.zir')}
            for form in ('source', 'saved'):
                assert contents(folder/'before'/form/'c') == contents(folder/'after'/form/'c'), (case, form)
                assert (folder/'before'/form/'program.zib').read_bytes() == (folder/'after'/form/'program.zib').read_bytes()
                # Compile the identical emitted C once per form and check a
                # closed-form oracle independent of the compiler/runtime.
                output = folder/'after'/form; emitted = output/'c'; host = output/'host.c'
                host.write_text('#include "program.h"\n#include <stdio.h>\nint main(void) { printf("%lld\\n", (long long)Answer()); return 0; }\n')
                invoke(['cc', '-O2', '-std=c11', '-I'+str(ROOT/'include'), '-I'+str(emitted),
                        host, *sorted(emitted.glob('*.c')), '-lm', '-o', output/'run'])
                invoke([output/'run'], expected=expected)
            assert (folder/'after/source/program.zib').read_bytes() == (folder/'after/saved/program.zib').read_bytes()
            print('validated '+case, flush=True)
    for index, (name, directory, entry) in enumerate(args.project):
        directory = Path(directory).resolve()
        if not (directory/'ziran.toml').is_file(): parser.error(f'no package manifest in {directory}')
        project = dict(name=name, root=str(directory), source_hashes=sources(directory))
        projects.append(project)
        flags = [] if (directory/'ziran.local.toml').is_file() else ['--locked']
        for version, binaries in versions.items():
            command = [binaries/'ziran', 'check', '--project', '--offline', *flags, entry]
            invoke(command, cwd=directory)
            register(f'project-{index}-{name}', 'source', version, 'check', command, directory)
    # Separate profiled warmups from timings: counters deliberately add work.
    for index, command in enumerate(commands):
        profile = out/f'profile-{index}.jsonl'
        invoke(command['argv'], cwd=command['cwd'], profile=profile)
        profiles.append(dict(**command, records=[json.loads(line) for line in profile.read_text().splitlines()]))
    trials = commands*args.repetitions; random.Random(59).shuffle(trials)
    for command in trials:
        samples.append(dict(**{k:v for k,v in command.items() if k not in ('argv', 'cwd')},
                            **invoke(command['argv'], cwd=command['cwd'], measured=True)))
    for project in projects:
        assert sources(Path(project['root'])) == project['source_hashes'], project['name']+' sources changed'
    keys = {(r['case'],r['form'],r['version'],r['phase']) for r in samples}
    summary = []
    for key in sorted(keys):
        rows = [r for r in samples if (r['case'],r['form'],r['version'],r['phase']) == key]
        summary.append(dict(zip(('case','form','version','phase'),key),
                            **{field:statistics.median(r[field] for r in rows) for field in ('wall_ms','cpu_ms','peak_rss_kib')}))
    metadata = dict(repetitions=args.repetitions, discarded_warmups=1,
                    binary_sha256={version:{tool:digest(folder/tool) for tool in tools} for version,folder in versions.items()},
                    source_sha256={str(p.relative_to(ROOT)):digest(p) for base in ['cmd','include','std']
                                   for p in (ROOT/base).rglob('*') if p.is_file() and p.suffix in ('.zi','.c','.h')},
                    generator_sha256={p.name:digest(p) for p in [Path(__file__),ROOT/'bench/compiler_scaling.py']},
                    caveats=['One shared host without CPU/frequency isolation; shuffled paired process samples include startup and the time launcher.',
                             'C and bundle bytes and saved IR must match before/after. Identical C is compiled once per form and all VM runs check independent arithmetic oracles.',
                             'Profiles are separate from timing samples; phase times are inclusive and counters cover selected operations only.',
                             'Real projects are checked offline with their existing manifests/overrides. Sources are guarded against changes; project tools must select sibling compiler variants.',
                             'Peak child RSS is process memory, not allocated live bytes or the sum of concurrent process memory.'])
    (out/'results.json').write_text(json.dumps(dict(metadata=metadata,artifacts=artifacts,projects=projects,
                                                 profiles=profiles,summary=summary,samples=samples),indent=2)+'\n')
    print(f'{len(samples)} paired compiler samples validated.', flush=True)


if __name__ == '__main__': main()
