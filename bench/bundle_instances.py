#!/usr/bin/env python3
"""Compare public bundle loading and fresh-instance CPU/wall costs."""
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


def fixtures(directory):
    cases = []
    for name in ['scalar', 'enums256', 'modules64']:
        path = directory / name
        path.mkdir(parents=True)
        if name == 'scalar':
            source, expected = '#program_export\nAnswer :: () -> s64 {\nreturn 42\n}\n', 42
        elif name == 'enums256':
            members = '\n'.join(f'V{i} :: {7 + 3*i};' for i in range(256))
            uses = '\n'.join(f'total += cast(s64)(Value.V{i});' for i in range(256))
            source = (f'Value :: enum s64 {{\n{members}\n}}\n#program_export\n'
                      f'Answer :: () -> s64 {{\ntotal: s64 = 0\n{uses}\nreturn total\n}}\n')
            expected = sum(7 + 3*i for i in range(256))
        else:
            for i in reversed(range(64)):
                imported = f'#import "part{i+1}"\n' if i < 63 else ''
                returned = f'return Step{i+1}(result)' if i < 63 else 'return result'
                adds = '\n'.join(f'result += {j}' for j in range(1, 17))
                (path / f'part{i}.zi').write_text(
                    f'{imported}#program_export\nStep{i} :: (value: s64) -> s64 {{\n'
                    f'result := value\n{adds}\n{returned}\n}}\n')
            source = '#import "part0"\n#program_export\nAnswer :: () -> s64 {\nreturn Step0(0)\n}\n'
            expected = 64 * sum(range(1, 17))
        entry = path / 'main.zi'
        entry.write_text(source)
        cases.append((name, entry, expected))
    return cases


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--before-dir', type=Path, required=True)
    parser.add_argument('--after-dir', type=Path, default=ROOT / 'build/bin')
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--repetitions', type=int, default=9)
    parser.add_argument('--instances', type=int, default=49)
    args = parser.parse_args()
    if args.repetitions < 1 or not 1 <= args.instances <= 10000:
        parser.error('positive repetitions and instances 1..10000 required')
    output = args.out.resolve()
    if output.exists():
        parser.error('output exists; preserve evidence and remove inactive outputs before reusing it')
    output.mkdir(parents=True)
    env = dict(os.environ)
    for name in ['DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'DBUS_SESSION_BUS_ADDRESS',
                 'ZIRAN_PROFILE', 'ZIRAN_VM_PROFILE']:
        env.pop(name, None)
    cc = shutil.which(env.get('CC', 'cc'))
    if not cc: parser.error('C compiler required')
    digest = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
    sources = {str(p.relative_to(ROOT)): digest(p) for folder in ['cmd', 'include', 'std', 'bench']
               for p in (ROOT / folder).rglob('*') if p.suffix in ['.c', '.h', '.zi', '.py']}
    metadata = dict(head=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
                    source_hashes=sources, platform=platform.platform(), repetitions=args.repetitions,
                    instances=args.instances, compiler=subprocess.check_output([cc, '--version'], text=True),
                    discarded_process_warmups=1, discarded_instance_warmups=1, artifacts={}, cases=[],
                    caveats=['Synthetic fixtures on one host with warm caches; no CPU isolation.',
                             'Load includes format, canonical, semantic, law and portable verification.',
                             'Each measured instance is new and must match an independent Python integer oracle.',
                             'Phase CPU/wall times come from the runner; medians summarize per-process medians.',
                             'No total RSS claim: small phase timings include clock-call overhead.'])
    variants = {}
    for label, directory in [('before', args.before_dir.resolve()), ('after', args.after_dir.resolve())]:
        library = directory / 'libziran.a'
        if not library.is_file(): library = directory.parent / 'libziran.a'
        runner = output / f'{label}-runner'
        command = [cc, '-O2', '-std=c11', '-D_GNU_SOURCE', '-I', ROOT / 'include',
                   ROOT / 'bench/bundle_instances.c', library, '-lm', '-pthread', '-o', runner]
        subprocess.run(list(map(str, command)), check=True, env=env, capture_output=True)
        variants[label] = dict(runner=runner, bundler=directory / 'zi2zx')
        metadata['artifacts'][label] = dict(library=str(library), library_sha256=digest(library),
                                          runner_sha256=digest(runner), bundler_sha256=digest(directory / 'zi2zx'))
    rows = []
    rng = random.Random(20261015)
    for name, source, expected in fixtures(output / 'fixtures'):
        bundles = {}
        for label, variant in variants.items():
            bundle = source.parent / f'{label}.zx'
            subprocess.run(list(map(str, [variant['bundler'], 'bundle', '--root', source.parent,
                           '--entry', 'main:Answer', '-o', bundle, source])), check=True, env=env,
                           capture_output=True)
            bundles[label] = bundle
        if bundles['before'].read_bytes() != bundles['after'].read_bytes():
            raise AssertionError(f'{name}: before/after bundles differ')
        metadata['cases'].append(dict(name=name, expected=expected, bundle_sha256=digest(bundles['before']),
                                      source_hashes={p.name: digest(p) for p in source.parent.glob('*.zi')}))
        for repetition in range(args.repetitions + 1):
            order = list(variants)
            rng.shuffle(order)
            for label in order:
                begin = time.perf_counter_ns()
                command = [variants[label]['runner'], bundles[label], args.instances, expected]
                result = subprocess.run(list(map(str, command)), env=env, check=True,
                                        capture_output=True, text=True, timeout=120)
                row = dict(case=name, variant=label, repetition=repetition,
                           process_wall_ms=(time.perf_counter_ns() - begin) / 1e6,
                           warmup=repetition == 0, **json.loads(result.stdout))
                with (output / 'samples.jsonl').open('a') as log: log.write(json.dumps(row) + '\n')
                if repetition: rows.append(row)
        print(f'validated {name}', flush=True)
    summary = {}
    for case in metadata['cases']:
        name = case['name']
        summary[name] = {}
        for label in variants:
            group = [r for r in rows if r['case'] == name and r['variant'] == label]
            summary[name][label] = {key: statistics.median(r[key] for r in group)
                                    for key in group[0] if key.endswith('_ms')}
    for relative, expected_hash in sources.items():
        if digest(ROOT / relative) != expected_hash:
            raise AssertionError(f'source changed during benchmark: {relative}')
    (output / 'metadata.json').write_text(json.dumps(metadata, indent=2) + '\n')
    (output / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    print(json.dumps(summary, indent=2))


if __name__ == '__main__':
    main()
