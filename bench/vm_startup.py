#!/usr/bin/env python3
"""Compare checked loading, VM preparation/execution, and compiler checks."""
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
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--before-dir', type=Path, required=True,
                        help='directory containing preserved zi2zir and libziran.a')
    parser.add_argument('--after-dir', type=Path, default=ROOT/'build/bin',
                        help='tools directory; libziran.a is also sought in its parent')
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--case', nargs=4, action='append', default=[],
                        metavar=('NAME', 'SOURCE', 'BUNDLE', 'EXPECTED'))
    parser.add_argument('--project', nargs=3, action='append', default=[],
                        metavar=('NAME', 'ROOT', 'ENTRY'),
                        help='also compare offline package checks using sibling ziran launchers')
    parser.add_argument('--repetitions', type=int, default=9)
    parser.add_argument('--runs', type=int, default=9, help='steady runs in each loaded VM instance')
    args = parser.parse_args()
    if not args.case and not args.project: parser.error('supply at least one case or project')
    if args.repetitions < 1 or not 1 <= args.runs <= 10000:
        parser.error('positive repetitions and runs from 1 to 10000 required')
    if len({item[0] for item in args.case + args.project}) != len(args.case + args.project):
        parser.error('case names must be unique')
    output = args.out.resolve()
    if output.exists():
        parser.error('output exists; preserve required receipts and remove inactive outputs before reuse')
    cc = shutil.which(os.environ.get('CC', 'cc'))
    if not cc or not hasattr(os, 'wait4'):
        parser.error('requires a C compiler and POSIX wait4')
    output.mkdir(parents=True)
    env = dict(os.environ)
    for key in ['DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'DBUS_SESSION_BUS_ADDRESS',
                'ZIRAN_PROFILE', 'ZIRAN_VM_PROFILE']:
        env.pop(key, None)
    launcher = []
    if tool := shutil.which('time'):
        version = subprocess.run([tool, '--version'], capture_output=True, text=True)
        if version.returncode == 0 and 'GNU' in version.stdout + version.stderr:
            launcher = [tool]
    if not launcher and (tool := shutil.which('busybox')):
        help_result = subprocess.run([tool, 'time', '--help'], capture_output=True, text=True)
        if '-f FMT' in help_result.stdout + help_result.stderr:
            launcher = [tool, 'time']
    digest = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
    git = lambda *argv: subprocess.check_output(['git', *argv], cwd=ROOT, text=True).strip()
    metadata = dict(schema_version=1, created_utc=time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()),
                    head=git('rev-parse', 'HEAD'), status=git('status', '--short'),
                    platform=platform.platform(), compiler=subprocess.check_output([cc, '--version'], text=True),
                    repetitions=args.repetitions, steady_runs=args.runs, discarded_process_warmups=1,
                    measurement_launcher=launcher, artifacts={}, cases=[], projects=[],
                    source_hashes={str(p.relative_to(ROOT)): digest(p)
                                   for directory in ['cmd', 'include', 'std']
                                   for p in (ROOT/directory).rglob('*')
                                   if p.is_file() and p.suffix in ['.c', '.h', '.zi']},
                    caveats=['One host, warm filesystem caches, no CPU/frequency isolation.',
                             'Bundle load includes structural, canonical, semantic and law validation.',
                             'Instance opening includes VM verification and global initialization.',
                             'First execution is separate from repeated runs of the same instance.',
                             'Every execution must match the supplied independently derived result.',
                             'Wall/CPU whole-process times include the measurement launcher when available.',
                             'Peak RSS includes IR and process startup; without a launcher it can include inherited Python memory.',
                             'Live value bytes exclude IR, stacks and metadata caches; they are not total allocated memory.'])
    for name in ['bench/vm_startup.py', 'bench/vm_startup.c']:
        metadata['source_hashes'][name] = digest(ROOT/name)
    variants = {}
    for label, directory in [('before', args.before_dir.resolve()), ('after', args.after_dir.resolve())]:
        library = directory/'libziran.a'
        if not library.is_file(): library = directory.parent/'libziran.a'
        runner = output/f'{label}-runner'
        command = [cc, '-O2', '-std=c11', '-D_GNU_SOURCE', '-I', ROOT/'include', '-I', ROOT/'cmd/zir',
                   ROOT/'bench/vm_startup.c', library, '-lm', '-pthread', '-o', runner]
        subprocess.run([str(p) for p in command], cwd=ROOT, env=env, check=True, capture_output=True)
        variants[label] = dict(runner=runner, checker=directory/'zi2zir', bundler=directory/'zi2zx',
                               launcher=directory/'ziran')
        metadata['artifacts'][label] = dict(library=str(library), library_sha256=digest(library),
                                          runner_sha256=digest(runner), checker_sha256=digest(directory/'zi2zir'),
                                          bundler_sha256=digest(directory/'zi2zx'),
                                          compile_command=[str(p) for p in command])
    rows = []

    def invoke(command, case, variant, phase, repetition, cwd=ROOT):
        argv = [str(p) for p in command]
        rss_file = output/'resource-rss.txt'
        measured = [*launcher, '-f', '%M', '-o', str(rss_file), *argv] if launcher else argv
        with tempfile.TemporaryFile() as stdout, tempfile.TemporaryFile() as stderr:
            begin = time.perf_counter_ns()
            child = subprocess.Popen(measured, cwd=cwd, env=env, stdout=stdout, stderr=stderr)
            _, status, usage = os.wait4(child.pid, 0)
            child.returncode = os.waitstatus_to_exitcode(status)
            elapsed = (time.perf_counter_ns() - begin) / 1e6
            stdout.seek(0); stderr.seek(0)
            text = stdout.read().decode(errors='replace').strip()
            errors = stderr.read().decode(errors='replace')
        if child.returncode:
            raise RuntimeError(f'{case}/{variant}/{phase}: {errors}')
        rss = int(rss_file.read_text().strip()) if launcher else usage.ru_maxrss
        if not launcher and platform.system() == 'Darwin': rss /= 1024
        row = dict(case=case, variant=variant, phase=phase, repetition=repetition,
                   warmup=repetition == 0, argv=argv, cwd=str(cwd), elapsed_ms=elapsed,
                   user_ms=usage.ru_utime * 1000, system_ms=usage.ru_stime * 1000,
                   max_rss_kib=rss, stdout=text, stderr=errors, returncode=child.returncode)
        if phase == 'vm': row.update(json.loads(text))
        with (output/'samples.jsonl').open('a') as log:
            log.write(json.dumps(row)+'\n')
        if repetition: rows.append(row)

    rng = random.Random(20261015)
    for case_index, (name, source, bundle, expected) in enumerate(args.case):
        source, bundle, expected = Path(source).resolve(), Path(bundle).resolve(), int(expected)
        if not -(1 << 63) <= expected < (1 << 63): parser.error('expected results must fit s64')
        bundle_bytes = bundle.read_bytes()
        if bundle_bytes[:4] != b'ZX\0\0': parser.error('invalid bundle signature')
        cursor, entry_names = 8, []
        for _ in range(2):
            length, = struct.unpack_from('<I', bundle_bytes, cursor)
            cursor += 4
            entry_names.append(bundle_bytes[cursor:cursor + length].decode('utf-8'))
            cursor += length
        for label, variant in variants.items():
            generated = output/f'case-{case_index}-{label}.zx'
            subprocess.run([str(p) for p in [variant['bundler'], 'bundle', '--root', source.parent,
                           '--module-path', ROOT/'std', '--entry', ':'.join(entry_names),
                           '-o', generated, source]], cwd=ROOT, env=env, check=True, capture_output=True)
            if generated.read_bytes() != bundle_bytes:
                raise AssertionError(f'{name}/{label}: supplied bundle differs from checked source')
            generated.unlink()
        metadata['cases'].append(dict(name=name, source=str(source), bundle=str(bundle), expected=expected,
                                      bundle_sha256=digest(bundle), source_sha256=digest(source),
                                      source_bundle_verified=True,
                                      source_hashes={str(p.relative_to(source.parent)): digest(p)
                                                     for p in source.parent.rglob('*.zi')}))
        for repetition in range(args.repetitions + 1):
            commands = [(label, phase) for label in variants for phase in ['check', 'vm']]
            rng.shuffle(commands)
            for label, phase in commands:
                variant = variants[label]
                command = ([variant['runner'], bundle, args.runs, expected] if phase == 'vm' else
                           [variant['checker'], '--check-only', '--root', source.parent,
                            '--module-path', ROOT/'std', source])
                invoke(command, name, label, phase, repetition)
        print(f'validated {name}', flush=True)
    for project_index, (name, directory, entry) in enumerate(args.project):
        directory = Path(directory).resolve()
        entry = (directory/entry).resolve()
        project_git = lambda *argv: subprocess.check_output(['git', *argv], cwd=directory, text=True).strip()
        project = dict(name=name, root=str(directory), entry=str(entry), head=project_git('rev-parse', 'HEAD'),
                       status=project_git('status', '--short'), profiles={}, source_hashes={})
        for path in directory.rglob('*.zi'):
            if not {'build', 'backup', 'vendor', '.git', '.ziran'} & set(path.relative_to(directory).parts):
                project['source_hashes'][str(path.relative_to(directory))] = digest(path)
        for filename in ['ziran.toml', 'ziran.lock', 'ziran.local.toml']:
            path = directory/filename
            if path.is_file(): project['source_hashes'][filename] = digest(path)
        for label, variant in variants.items():
            metadata['artifacts'][label]['launcher_sha256'] = digest(variant['launcher'])
            profile = output/f'project-{project_index}-{label}.jsonl'
            command = [str(p) for p in [variant['launcher'], 'check', '--project', '--offline', entry]]
            result = subprocess.run(command, cwd=directory, env={**env, 'ZIRAN_PROFILE': str(profile)},
                                    text=True, capture_output=True, check=True, timeout=300)
            project['profiles'][label] = [json.loads(line) for line in profile.read_text().splitlines()]
        for repetition in range(args.repetitions + 1):
            labels = list(variants)
            rng.shuffle(labels)
            for label in labels:
                invoke([variants[label]['launcher'], 'check', '--project', '--offline', entry],
                       name, label, 'project-check', repetition, cwd=directory)
        project.update(head_after=project_git('rev-parse', 'HEAD'), status_after=project_git('status', '--short'))
        changed = [name for name, sha in project['source_hashes'].items()
                   if not (directory/name).is_file() or digest(directory/name) != sha]
        if changed or project['head'] != project['head_after']:
            raise RuntimeError(f'{name}: project sources changed during measurement: {changed}')
        metadata['projects'].append(project)
        print(f'validated project {name}', flush=True)
    groups = {}
    for row in rows: groups.setdefault((row['case'], row['variant'], row['phase']), []).append(row)
    summary = []
    for (case, variant, phase), samples in groups.items():
        metrics = ['elapsed_ms', 'user_ms', 'system_ms', 'max_rss_kib']
        if phase == 'vm': metrics += ['load_ms', 'open_ms', 'first_run_ms', 'steady_run_ms', 'close_ms', 'live_value_bytes']
        summary.append(dict(case=case, variant=variant, phase=phase, samples=len(samples),
                            **{f'median_{metric}': statistics.median(r[metric] for r in samples) for metric in metrics},
                            min_elapsed_ms=min(r['elapsed_ms'] for r in samples),
                            max_elapsed_ms=max(r['elapsed_ms'] for r in samples)))
    metadata.update(head_after=git('rev-parse', 'HEAD'), status_after=git('status', '--short'))
    (output/'metadata.json').write_text(json.dumps(metadata, indent=2)+'\n')
    (output/'summary.json').write_text(json.dumps(summary, indent=2)+'\n')
    print(f'{len(rows)} validated measured samples: {output}', flush=True)


if __name__ == '__main__':
    main()
