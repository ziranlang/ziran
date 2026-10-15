#!/usr/bin/env python3
"""Compare host-capability enumeration, binding and execution with independent oracles."""
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


def fixture(path, count, loops):
    path.mkdir(parents=True)
    declarations = '\n'.join(f'H{i:03} :: (value: s64) -> s64 #foreign host_api;' for i in range(count))
    calls = '\n'.join(f'value = H{i:03}(value)' for i in range(count))
    source = path / 'api.zi'
    source.write_text(f'host_api :: #system_library "host_api";\n{declarations}\n'
                      '#program_export\nAnswer :: () -> s64 {\nvalue: s64 = 7\n'
                      f'round: s32\nwhile round < {loops} {{\n{calls}\nround += 1\n}}\nreturn value\n}}\n')
    expected = 7
    for _ in range(loops):
        for index in range(count): expected = (expected * 48271 + index + 1) & ((1 << 63) - 1)
    return source, expected


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--before-dir', type=Path, required=True)
    parser.add_argument('--after-dir', type=Path, default=ROOT / 'build/bin')
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--sizes', type=int, nargs='+', default=[1, 16, 64, 256])
    parser.add_argument('--repetitions', type=int, default=9)
    parser.add_argument('--instances', type=int, default=9)
    parser.add_argument('--loops', type=int, default=32)
    args = parser.parse_args()
    if (not args.sizes or len(set(args.sizes)) != len(args.sizes) or
            any(n < 1 or n > 512 for n in args.sizes) or args.repetitions < 1 or
            not 1 <= args.instances <= 10000 or not 1 <= args.loops <= 1024):
        parser.error('unique sizes 1..512, positive repetitions, instances 1..10000, loops 1..1024 required')
    output = args.out.resolve()
    if output.exists(): parser.error('output exists; preserve evidence and remove inactive outputs before reuse')
    cc = shutil.which(os.environ.get('CC', 'cc'))
    if not cc: parser.error('C compiler required')
    output.mkdir(parents=True)
    env = dict(os.environ)
    for key in ['DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'DBUS_SESSION_BUS_ADDRESS',
                'ZIRAN_PROFILE', 'ZIRAN_VM_PROFILE']: env.pop(key, None)
    digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    sources = {str(p.relative_to(ROOT)): digest(p) for folder in ['cmd', 'include', 'std']
               for p in (ROOT / folder).rglob('*') if p.is_file() and p.suffix in ['.c', '.h', '.zi']}
    for name in ['bench/host_capabilities.c', 'bench/host_capabilities.py']:
        sources[name] = digest(ROOT / name)
    metadata = dict(head=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
                    created_utc=time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()), platform=platform.platform(),
                    source_hashes=sources, repetitions=args.repetitions, instances=args.instances, loops=args.loops,
                    compiler=subprocess.check_output([cc, '--version'], text=True), artifacts={}, cases=[],
                    caveats=['One shared host, warm caches, no CPU isolation.',
                             'Each process discards its first instance; one process warmup per variant is discarded.',
                             'Phase medians summarize per-process instance medians. Process time includes startup.',
                             'Allocator in-use byte deltas use glibc mallinfo2 when available; they are not RSS '
                             'or requested live bytes, exclude stacks/mmap, and are affected by free-list/tcache retention.',
                             'Binding enumeration includes caller-side binding/context arrays.',
                             'Each callback checks module, function, argument, context and call order; every run '
                             'must match the independent Python 63-bit recurrence oracle.'])
    variants = {}
    for label, directory in [('before', args.before_dir.resolve()), ('after', args.after_dir.resolve())]:
        library = directory / 'libziran.a'
        if not library.is_file(): library = directory.parent / 'libziran.a'
        runner = output / f'{label}-runner'
        command = list(map(str, [cc, '-O2', '-std=c11', '-D_GNU_SOURCE', '-I', ROOT / 'include',
                                ROOT / 'bench/host_capabilities.c', library, '-lm', '-pthread', '-o', runner]))
        subprocess.run(command, check=True, env=env, capture_output=True)
        variants[label] = dict(runner=runner, bundler=directory / 'zi2zib')
        metadata['artifacts'][label] = dict(library=str(library), library_sha256=digest(library),
                                          runner_sha256=digest(runner), bundler_sha256=digest(directory / 'zi2zib'))
    rows = []
    rng = random.Random(20261015)
    for count in args.sizes:
        source, expected = fixture(output / f'capabilities-{count}', count, args.loops)
        bundles = {}
        for label, variant in variants.items():
            bundle = source.parent / f'{label}.zib'
            subprocess.run(list(map(str, [variant['bundler'], 'bundle', '--root', source.parent,
                           '--entry', 'api:Answer', '-o', bundle, source])), check=True,
                           env=env, capture_output=True)
            bundles[label] = bundle
        if bundles['before'].read_bytes() != bundles['after'].read_bytes():
            raise AssertionError(f'capabilities-{count}: before/after bundle bytes differ')
        metadata['cases'].append(dict(capabilities=count, expected=expected, bundle_sha256=digest(bundles['before']),
                                      source_hashes={p.name: digest(p) for p in source.parent.glob('*.zi')}))
        for repetition in range(args.repetitions + 1):
            order = list(variants); rng.shuffle(order)
            for label in order:
                begin = time.perf_counter_ns()
                result = subprocess.run(list(map(str, [variants[label]['runner'], bundles[label],
                                        args.instances, args.loops, expected])), check=True, env=env,
                                        capture_output=True, text=True, timeout=120)
                row = dict(variant=label, repetition=repetition, warmup=repetition == 0,
                           process_wall_ms=(time.perf_counter_ns() - begin) / 1e6, **json.loads(result.stdout))
                with (output / 'samples.jsonl').open('a') as log: log.write(json.dumps(row) + '\n')
                if repetition: rows.append(row)
        print(f'validated {count} capabilities', flush=True)
    summary = {}
    for count in args.sizes:
        summary[count] = {}
        for label in variants:
            group = [r for r in rows if r['capabilities'] == count and r['variant'] == label]
            summary[count][label] = {k: statistics.median(r[k] for r in group)
                                     for k in group[0] if k.endswith(('_ms', '_bytes'))}
    for relative, expected_hash in sources.items():
        if digest(ROOT / relative) != expected_hash: raise AssertionError(f'source changed: {relative}')
    for name, data in [('metadata', metadata), ('summary', summary)]:
        (output / f'{name}.json').write_text(json.dumps(data, indent=2) + '\n')
    print(json.dumps(summary, indent=2))


if __name__ == '__main__':
    main()
