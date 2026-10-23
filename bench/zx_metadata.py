#!/usr/bin/env python3
"""Measure portable bundle size, packing and fresh-host loading of native metadata."""
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

ROOT = Path(__file__).resolve().parents[1]


def fixture(functions, padding, form):
    lines = ['state: s64;']
    for index in range(functions):
        template = '# NATIVE_CANARY_' + str(index) + ' ' + 'x' * padding + '\nadd $1, %[out]'
        if form == 'function':
            modifier = '#asm(' + ', '.join(json.dumps(s) for s in ['x86_64', template, '=r', '0', 'cc', '']) + ')'
            lines += [f'F{index} :: (value: s64) -> s64 {modifier} {{', '    return value + 1', '}']
        elif form == 'block':
            modifier = '#asm(' + ', '.join(json.dumps(s) for s in ['x86_64', template, '=r:out=result', '0:old=value', 'cc', '']) + ')'
            lines += [f'F{index} :: (value: s64) -> s64 {{', '    result: s64', '    ' + modifier + ' {', '        result = value + 1', '    }', '    return result', '}']
        else:
            lines += [f'F{index} :: (value: s64) -> s64 {{', '    return value + 1', '}']
    lines += ['#program_export', 'Answer :: () -> s64 {', '    value: s64 = 7']
    lines += [f'    value = F{index}(value)' for index in range(functions)]
    lines += ['    state += 1', '    return value + state', '}']
    return '\n'.join(lines) + '\n'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--before-bin-dir', type=Path, required=True)
    parser.add_argument('--bin-dir', type=Path, default=ROOT / 'build/bin')
    parser.add_argument('--repetitions', type=int, default=9)
    parser.add_argument('--runs', type=int, default=20)
    args = parser.parse_args()
    if min(args.repetitions, args.runs) < 1: parser.error('positive repetitions and runs required')
    out = args.out.resolve()
    if out.exists(): parser.error('output exists; preserve receipts before reusing inactive outputs')
    out.mkdir(parents=True)
    env = dict(os.environ)
    for key in ('DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'DBUS_SESSION_BUS_ADDRESS', 'ZIRAN_PROFILE'):
        env.pop(key, None)
    versions = {'before': args.before_bin_dir.resolve(), 'after': args.bin_dir.resolve()}
    timer = [shutil.which('busybox'), 'time'] if shutil.which('busybox') else [shutil.which('time')]
    if not timer[0]: parser.error('GNU or BusyBox time required for child peak RSS')
    samples, commands, ir_commands, artifacts = [], [], [], []

    def run(command, *, measured=False, expected=None):
        argv = list(map(str, command)); rss = out / 'rss.txt'
        before = resource.getrusage(resource.RUSAGE_CHILDREN); start = time.perf_counter_ns()
        result = subprocess.run(([*timer, '-f', '%M', '-o', str(rss)] if measured else []) + argv,
                                cwd=ROOT, env=env, capture_output=True, text=True, timeout=180)
        elapsed = (time.perf_counter_ns() - start) / 1e6
        after = resource.getrusage(resource.RUSAGE_CHILDREN)
        if result.returncode: raise RuntimeError(f'{argv}\n{result.stdout}{result.stderr}')
        if expected is not None: assert result.stdout.strip() == str(expected), (argv, result.stdout, expected)
        return dict(argv=argv, wall_ms=elapsed, cpu_ms=1000*(after.ru_utime+after.ru_stime-before.ru_utime-before.ru_stime),
                    peak_rss_kib=int(rss.read_text()) if measured else None), result.stdout

    for version, folder in versions.items():
        host = out / ('host-' + version)
        run(['cc', '-O2', '-D_GNU_SOURCE', '-std=c11', '-I' + str(ROOT / 'include'),
             ROOT / 'bench/zx_metadata.c', folder / 'libziran.a' if (folder / 'libziran.a').exists() else folder.parent / 'libziran.a', '-lm', '-pthread', '-o', host])
    for form in ('plain', 'function', 'block'):
        for functions, padding in ([(128, 0)] if form == 'plain' else [(1, 0), (64, 1024), (128, 2048)]):
            name = f'{form}-{functions}-{padding}'; work = out / name; work.mkdir()
            entry = work / 'program.zi'; entry.write_text(fixture(functions, padding, form))
            for version, folder in versions.items():
                ir = work / ('ir-' + version)
                run([folder / 'ziran', 'ir', '--root', work, '-o', ir, entry])
                ir_commands.append((name, version, [folder / 'ziran', 'ir', '--root', work, '-o', ir, entry]))
                bundle = work / (version + '.zx')
                command = [folder / 'ziran', 'bundle', '--root', work, '--entry', 'program:Answer', '-o', bundle, entry]
                run(command); run([folder / 'ziran', 'run', bundle], expected=8 + functions)
                saved = work / (version + '-saved.zx')
                run([folder / 'ziran', 'bundle', '--root', ir, '--entry', 'program:Answer', '-o', saved, ir / 'program.zir'])
                assert bundle.read_bytes() == saved.read_bytes()
                if version == 'after':
                    assert b'NATIVE_CANARY_' not in bundle.read_bytes()
                    assert (form == 'plain') or b'NATIVE_CANARY_' in (ir / 'program.zir').read_bytes()
                commands.append((name, version, command))
                artifacts.append(dict(case=name, version=version, bytes=bundle.stat().st_size,
                                      sha256=hashlib.sha256(bundle.read_bytes()).hexdigest()))
            assert (work / 'ir-before/program.zir').read_bytes() == (work / 'ir-after/program.zir').read_bytes()
            if form == 'plain': assert (work / 'before.zx').read_bytes() == (work / 'after.zx').read_bytes()
    # Warmups were the validation builds above. Shuffle paired packing samples.
    trials = [(phase, *command) for phase, items in [('bundle', commands), ('ir', ir_commands)] for command in items] * args.repetitions
    random.Random(41).shuffle(trials)
    for phase, case, version, command in trials:
        record, _ = run(command, measured=True)
        samples.append(dict(phase=phase, case=case, writer=version, **record))
    loads = []
    for artifact in artifacts:
        for reader in versions:
            command = [out / ('host-' + reader), out / artifact['case'] / (artifact['version'] + '.zx'),
                       artifact['case'].split('-')[1], args.runs]
            run(command)
            loads.append((artifact['case'], artifact['version'], reader, command))
    trials = loads * args.repetitions; random.Random(73).shuffle(trials)
    for case, writer, reader, command in trials:
        record, output = run(command, measured=True)
        samples.append(dict(phase='host', case=case, writer=writer, reader=reader, **record, **json.loads(output)))
    summary = []
    keys = {(r['phase'], r['case'], r['writer'], r.get('reader')) for r in samples}
    for phase, case, writer, reader in sorted(keys, key=str):
        selected = [r for r in samples if (r['phase'],r['case'],r['writer'],r.get('reader')) == (phase,case,writer,reader)]
        fields = ('wall_ms','cpu_ms','peak_rss_kib') + (('open_ms','instantiate_ms','run_ms','close_ms','live_heap_delta_bytes','retained_heap_delta_bytes') if phase=='host' else ())
        summary.append(dict(phase=phase,case=case,writer=writer,reader=reader,
                            **{key:statistics.median(r[key] for r in selected)
                               if not key.endswith('heap_delta_bytes') or all(r['heap_supported'] for r in selected) else None
                               for key in fields}))
    metadata = dict(repetitions=args.repetitions,runs=args.runs,discarded_warmups=1,
                    source_sha256={str(p.resolve().relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [Path(__file__),ROOT/'bench/zx_metadata.c']},
                    binary_sha256={version:{tool:hashlib.sha256((folder/tool).read_bytes()).hexdigest() for tool in ('ziran','zi2zx','zi2zir')} for version,folder in versions.items()},
                    caveats=['Synthetic annotations include deliberately large comments; this is a metadata stress test, not a typical application size claim.',
                             'Fresh host processes include startup; internal open timing excludes input-file reading.',
                             'Host reader/writer crossover separates smaller payloads from runtime changes. Every run validates an independent arithmetic result and persistent state.',
                             'Heap deltas use glibc malloc accounting (uordblks plus hblkhd), include retained interned text/caches, and are not RSS or a portable heap measure.',
                             'No CPU isolation; paired trials are shuffled. Packing includes source checking and linking, not only serialization.'])
    (out / 'results.json').write_text(json.dumps(dict(metadata=metadata,artifacts=artifacts,summary=summary,samples=samples),indent=2)+'\n')
    print('Portable metadata benchmark completed; native IR bytes and every portable result were validated.')


if __name__ == '__main__': main()
