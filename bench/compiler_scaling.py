#!/usr/bin/env python3
"""Validate and measure module graphs and generic specialization, with phase profiles."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import statistics
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True, help='new result directory')
    parser.add_argument('--bin-dir', type=Path, default=ROOT / 'build/bin')
    parser.add_argument('--sizes', type=int, nargs='+', default=[8, 32, 128])
    parser.add_argument('--fields', type=int, default=16)
    parser.add_argument('--repetitions', type=int, default=3)
    parser.add_argument('--project', type=Path, action='append', default=[],
                        help='also check this real package entry source with --project')
    args = parser.parse_args()
    if any(size < 1 or size > 256 for size in args.sizes) or not 1 <= args.fields <= 64 or args.repetitions < 1:
        parser.error('sizes must be 1..256, fields 1..64, and repetitions positive')
    output = args.out.resolve()
    if output.exists():
        parser.error(f'output directory already exists: {output}')
    output.mkdir(parents=True)
    binaries = args.bin_dir.resolve()
    env = dict(os.environ)
    for key in ['DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'DBUS_SESSION_BUS_ADDRESS', 'ZIRAN_PROFILE']:
        env.pop(key, None)
    cc = shutil.which(os.environ.get('CC', 'cc'))
    if cc is None:
        parser.error('a C compiler is required')
    for name in ['ziran', 'zi2zir', 'zi2c', 'zi2zib']:
        if not (binaries / name).is_file():
            parser.error(f'build the compiler first: missing {binaries / name}')
    hashes = {str(path.relative_to(ROOT)): digest(path)
              for folder in ['cmd', 'std', 'include'] for path in (ROOT / folder).rglob('*')
              if path.is_file() and path.suffix in ['.c', '.h', '.zi']}
    hashes['Makefile'] = digest(ROOT / 'Makefile')
    hashes['bench/compiler_scaling.py'] = digest(Path(__file__))
    def git(*arguments):
        return subprocess.check_output(['git', *arguments], cwd=ROOT, env=env, text=True).strip()
    version = subprocess.run([cc, '--version'], env=env, text=True, capture_output=True, check=True)
    metadata = dict(schema_version=1, created_utc=time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()),
        head=git('rev-parse', 'HEAD'), status=git('status', '--short'),
        platform=platform.platform(), cpu_affinity=sorted(os.sched_getaffinity(0)),
        compiler_version=version.stdout, source_hashes=hashes,
        binary_hashes={name: digest(binaries / name) for name in ['ziran', 'zi2zir', 'zi2c', 'zi2zib']},
        sizes=args.sizes, fields=args.fields, repetitions=args.repetitions, discarded_warmups=1,
        caveats=['Wall times measure whole child processes, including startup and shutdown.',
                 'Compiler phase times are inclusive and overlap; their sum is not build time.',
                 'Allocation counts cover AllocateOrExit workspace requests, not all allocations or live bytes.',
                 'C builds invoke the compiler again; filesystem caches may be warm.',
                 'No CPU frequency isolation; concurrent work may affect samples.'])
    (output / 'metadata.json').write_text(json.dumps(metadata, indent=2) + '\n')
    samples = []
    invocation = 0
    def run(command, case, phase, oracle=None, cwd=ROOT):
        nonlocal invocation
        for repetition in range(args.repetitions + 1):
            invocation += 1
            profile = output / f'profile-{invocation}.jsonl'
            child_env = {**env, 'ZIRAN_PROFILE': str(profile)}
            argv = [str(part) for part in command]
            started = time.perf_counter_ns()
            child = subprocess.run(argv, cwd=cwd, env=child_env, text=True, capture_output=True, timeout=300)
            elapsed = (time.perf_counter_ns() - started) / 1e6
            record = dict(case=case, phase=phase, repetition=repetition, warmup=repetition == 0,
                argv=argv, cwd=str(cwd), elapsed_ms=elapsed, returncode=child.returncode,
                stdout=child.stdout, stderr=child.stderr,
                compiler_profiles=[json.loads(line) for line in profile.read_text().splitlines()] if profile.exists() else [])
            with (output / 'samples.jsonl').open('a') as log:
                log.write(json.dumps(record) + '\n')
            if child.returncode != 0:
                raise RuntimeError(f'{case}/{phase}: {child.stderr}')
            if oracle is not None and child.stdout.strip() != str(oracle):
                raise AssertionError(f'{case}/{phase}: expected {oracle}, got {child.stdout!r}')
            if repetition:
                samples.append(record)
        print(f'{case}: {phase}', flush=True)
    def workload(kind, size):
        directory = output / f'{kind}-{size}'
        source = directory / 'source'
        source.mkdir(parents=True)
        if kind == 'modules':
            for index in range(size):
                fields = '\n'.join(f'field_{field}: s64' for field in range(args.fields))
                writes = '\n'.join(f'value.field_{field} = {index};' for field in range(args.fields))
                result = ' + '.join(f'value.field_{field}' for field in range(args.fields))
                (source / f'm_{index}.zi').write_text(
                    f'Record :: struct {{\n{fields}\n}}\nRead :: () -> s64 {{\nvalue: Record;\n{writes}\nreturn {result};\n}}\n')
            imports = '\n'.join(f'M{index} :: #import "m_{index}";' for index in range(size))
            result = ' + '.join(f'M{index}.Read()' for index in range(size))
            text = f'{imports}\n#program_export\nAnswer :: () -> s64 {{ return {result}; }}\n'
            oracle = size * (size - 1) // 2 * args.fields
        else:
            records = '\n'.join(f'R{index} :: struct {{ item: s64; }}' for index in range(size))
            uses = '\n'.join(f'value_{index}: R{index}; value_{index}.item = {index}; total += Pick(value_{index});'
                             for index in range(size))
            text = f'{records}\nPick :: (value: $T) -> s64 {{ return value.item; }}\n#program_export\nAnswer :: () -> s64 {{\ntotal: s64 = 0;\n{uses}\nreturn total;\n}}\n'
            oracle = size * (size - 1) // 2
        entry = source / 'program.zi'
        entry.write_text(text)
        case = f'{kind}/{size}'
        run([binaries/'zi2zir', '--root', source, '-o', directory/'ir', entry], case, 'save-ir')
        for form, root, entry in [('source', source, entry),
                                 ('saved', directory/'ir', directory/'ir/program.zir')]:
            label = f'{case}/{form}'
            run([binaries/'zi2zir', '--check-only', '--root', root, entry], label, 'check')
            native = directory / form / 'c'
            run([binaries/'zi2c', '--root', root, '-o', native, entry], label, 'emit:c')
            host = directory / form / 'host.c'
            host.write_text('#include "program.h"\n#include <stdio.h>\nint main(void) { printf("%lld\\n", (long long)Answer()); return 0; }\n')
            binary = directory / form / 'program-run'
            run([cc, '-std=c99', '-O2', '-I', native, '-I', ROOT/'include', host,
                 *sorted(native.glob('*.c')), '-lm', '-o', binary], label, 'native-compile:c')
            run([binary], label, 'run:c', oracle)
            bundle = directory / form / 'program.zib'
            run([binaries/'zi2zib', 'bundle', '--root', root, '--entry', 'program:Answer', '-o', bundle, entry], label, 'bundle')
            run([binaries/'zi2zib', 'run', bundle], label, 'run:vm', oracle)
        (directory / 'fixture-hashes.json').write_text(json.dumps(
            {str(path.relative_to(directory)): digest(path) for path in source.glob('*.zi')}, indent=2) + '\n')
    for size in args.sizes:
        for kind in ['modules', 'generics']:
            workload(kind, size)
    for entry in args.project:
        entry = entry.resolve()
        project = next((parent for parent in entry.parents if (parent/'ziran.toml').is_file()), None)
        if project is None:
            parser.error(f'no package manifest above {entry}')
        flags = [] if (project/'ziran.local.toml').is_file() else ['--locked']
        run([binaries/'ziran', 'check', '--project', *flags, entry],
            f'project/{project.name}', 'check', cwd=project)
    groups = {}
    for record in samples:
        groups.setdefault((record['case'], record['phase']), []).append(record['elapsed_ms'])
    summary = [dict(case=case, phase=phase, samples=len(values), median_ms=statistics.median(values),
                    min_ms=min(values), max_ms=max(values)) for (case, phase), values in groups.items()]
    (output / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    metadata.update(head_after=git('rev-parse', 'HEAD'), status_after=git('status', '--short'))
    (output / 'metadata.json').write_text(json.dumps(metadata, indent=2) + '\n')
    print(f'{len(samples)} validated measured samples: {output}', flush=True)


if __name__ == '__main__':
    main()
