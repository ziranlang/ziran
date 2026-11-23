#!/usr/bin/env python3
"""Freeze public two-frame Kryon widget builds and measure matched native/C runs.

This writes no site data. Run through managed desktop_run on the selected host.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import platform
import random
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
import site_target_platforms as platforms
from benchmark_samples import HostGate, GATE_METHOD, summarize_samples
WORK = ROOT / 'build/widget-native'


def identity(path):
    path = Path(path).resolve()
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(chunk)
    return {'path': str(path), 'sha256': digest.hexdigest(), 'bytes': path.stat().st_size}


def execute(command, cwd=ROOT):
    print(' '.join(map(str, command)), flush=True)
    subprocess.run(list(map(str, command)), cwd=cwd, env=environment(), check=True)


def environment():
    return dict(platforms.environment(), ZIRAN_LEARN='off')


def prepare(label, project, runtime_label=None, program_label=None):
    destination = WORK / label
    if destination.exists():
        raise ValueError(f'Frozen build already exists: {destination}')
    manifest = json.loads((project / 'tests/widget_benchmarks.json').read_text())
    source = project / manifest['source']
    ziran = ROOT / 'build/bin/ziran'
    compiler = identity(ziran)
    program_origin = None
    frozen_cases = {}
    if program_label:
        origin_path = WORK / program_label / 'manifest.json'
        origin = json.loads(origin_path.read_text())
        assert origin['host'] == platform.node()
        assert origin['methodology'] == manifest['methodology']
        for case in origin['cases']:
            for key in ('bundle', 'c'):
                assert identity(case[key]['path']) == case[key]
            frozen_cases[case['id']] = case
        program_origin = {'manifest': identity(origin_path), 'inputs': origin['inputs']}
    resolved = subprocess.check_output([str(ziran), 'pkg', 'path', 'ziran'], cwd=project,
                                      env=environment(), text=True).strip()
    assert Path(resolved).resolve() == ROOT
    destination.mkdir(parents=True)
    library = destination / 'libziran.a'
    runner = destination / 'zx-runner'
    runtime_origin = None
    if runtime_label:
        origin_path = WORK / runtime_label / 'manifest.json'
        origin = json.loads(origin_path.read_text())
        assert origin['host'] == platform.node()
        for key in ('library', 'runner'):
            assert identity(origin[key]['path']) == origin[key]
        shutil.copyfile(origin['library']['path'], library)
        shutil.copy2(origin['runner']['path'], runner)
        runtime_origin = {'manifest': identity(origin_path),
                          'library': origin['library'], 'runner': origin['runner']}
    else:
        shutil.copyfile(ROOT / 'build/libziran.a', library)
        platforms.runtime_build('cc', library, runner,
                                ['-include', ROOT / 'bench/site_target_backend.h',
                                 '-DMATRIX_ZX_BACKEND=BUNDLE_BACKEND_NATIVE'])
    bindings = [part for binding in manifest['bindings'] for part in ('--bind', binding)]
    common = ['--root', source.parent, '--project', '--module-path', source.parent, *bindings]
    cases = []
    for case in manifest['cases']:
        directory = destination / case['id']
        directory.mkdir()
        bundle = directory / 'widget_benchmarks.zx'
        c_runner = directory / 'c-runner'
        if program_label:
            frozen = frozen_cases[case['id']]
            assert all(case[key] == frozen[key] for key in ('entry', 'expected'))
            shutil.copyfile(frozen['bundle']['path'], bundle)
            shutil.copy2(frozen['c']['path'], c_runner)
            cases.append({**case, 'stdout': frozen['stdout'],
                          'bundle': identity(bundle), 'c': identity(c_runner)})
            continue
        execute([ziran, 'bundle', *common, '--entry', 'widget_benchmarks:' + case['entry'],
                 '-o', bundle, source], project)
        cdir = directory / 'c'
        execute([ziran, 'build', '--target=c', '--no-main', *common,
                 '--entry', 'widget_benchmarks:' + case['entry'], '-o', cdir, source], project)
        header = cdir / 'widget_benchmarks.h'
        symbol = re.search(r'\b(\w*' + case['entry'] + r')\(void\)', header.read_text())[1]
        execute(['cc', '-O2', '-std=c11', '-DMATRIX_NATIVE', '-I' + str(cdir),
                 '-DMATRIX_HEADER="widget_benchmarks.h"', '-DMATRIX_ENTRY=' + symbol,
                 ROOT / 'bench/site_target_host.c', *sorted(cdir.glob('*.c')), '-lm', '-o', c_runner])
        cases.append({**case, 'stdout': '', 'bundle': identity(bundle), 'c': identity(c_runner)})
    inputs = [ROOT / 'Makefile', ROOT / 'bench/site_target_host.c', ROOT / 'bench/site_target_backend.h',
              ROOT / 'scripts/benchmark_samples.py', ROOT / 'scripts/site_target_platforms.py', Path(__file__),
              *sorted((ROOT / 'cmd').glob('*.[ch]')),
              *sorted((ROOT / 'cmd').glob('compiler*.zi')),
              *sorted((ROOT / 'cmd/zir').glob('*.[ch]')),
              *sorted((ROOT / 'include').rglob('*.h')),
              *sorted((ROOT / 'std').rglob('*.zi')),
              project / 'ziran.toml', project / 'ziran.lock',
              *[project / name for name in manifest['sources']],
              *sorted((project / 'src/ui').rglob('*.zi'))]
    assert identity(ziran) == compiler, 'Compiler changed while preparing artifacts'
    data = {'host': platform.node(), 'machine': platform.machine(),
            'created_at': datetime.now(timezone.utc).isoformat(), 'methodology': manifest['methodology'],
            'backend': 'Native', 'checks': False, 'unbounded': False, 'learning': 'off',
            'revision': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
            'project_revision': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=project, text=True).strip(),
            'compiler': compiler, 'runtime_origin': runtime_origin, 'program_origin': program_origin,
            'runner': identity(runner), 'library': identity(library), 'cases': cases,
            'inputs': [identity(path) for path in inputs if path.is_file()]}
    (destination / 'manifest.json').write_text(json.dumps(data, indent=2) + '\n')
    print(f'Frozen {label} on {data["host"]}: {destination}', flush=True)


def measure(labels, rounds, output):
    if output.exists():
        raise ValueError(f'Receipt already exists: {output}')
    builds = {label: json.loads((WORK / label / 'manifest.json').read_text()) for label in labels}
    assert all(data['host'] == platform.node() for data in builds.values())
    artifacts = {label: data['runner'] for label, data in builds.items()}
    for data in builds.values():
        for artifact in [data['runner'], data['library'],
                         *[case[key] for case in data['cases'] for key in ('bundle', 'c')]]:
            assert identity(artifact['path']) == artifact, artifact['path']
    jobs = []
    runs = {}
    reference = builds[labels[0]]
    for case in reference['cases']:
        name = case['id']
        runs[name] = {'c': [], **{label: [] for label in labels}}
        jobs.append((name, 'c', [case['c']['path'], '-', case['expected'], '11'], case))
        for label, data in builds.items():
            match = next(row for row in data['cases'] if row['id'] == name)
            assert all(case[key] == match[key] for key in ('entry', 'expected', 'stdout'))
            jobs.append((name, label, [data['runner']['path'], match['bundle']['path'],
                                      case['expected'], '11'], case))
    gate = HostGate()
    env = gate.environment(environment())
    rng = random.Random(53)
    order = []
    for iteration in range(rounds):
        rng.shuffle(jobs)
        for name, label, command, case in jobs:
            measured = platforms.measure(command, case, env, gate)
            runs[name][label].append(measured)
            order.append([iteration, name, label])
            print(f'{iteration + 1}/{rounds} {name} {label}: {measured["execution_ms"] * 1000:.3f} us', flush=True)
    summary = {name: {label: summarize_samples([run['execution_ms'] for run in values])
                      for label, values in groups.items()} for name, groups in runs.items()}
    result = {'host': platform.node(), 'measured_at': datetime.now(timezone.utc).isoformat(),
              'cpu': next(line.split(':', 1)[1].strip() for line in Path('/proc/cpuinfo').read_text().splitlines()
                          if line.startswith('model name')),
              'rounds': rounds, 'samples_per_run': 11, 'seed': 53, 'learning': 'off',
              'methodology': reference['methodology'] + ' Matched shuffled runs on frozen artifacts; 11 calibrated batches/run, '
                             'symmetric inclusive 1.5xIQR within and across runs; all raw values and exclusions retained. ' + GATE_METHOD,
              'builds': builds, 'order': order, 'runs': runs, 'summary': summary}
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({name: {label: group['median_ms'] * 1000 for label, group in groups.items()}
                      for name, groups in summary.items()}, indent=2))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prepare')
    parser.add_argument('--runtime', help='Reuse the immutable runtime of a prepared label')
    parser.add_argument('--programs', help='Reuse immutable bundles and C controls from a prepared label')
    parser.add_argument('--project', type=Path, default=ROOT.parents[1] / 'kryonlabs/kryon')
    parser.add_argument('--labels', nargs='+')
    parser.add_argument('--rounds', type=int, default=11)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    if args.prepare:
        assert re.fullmatch(r'[a-z][a-z0-9-]*', args.prepare)
        assert not args.runtime or re.fullmatch(r'[a-z][a-z0-9-]*', args.runtime)
        assert not args.programs or re.fullmatch(r'[a-z][a-z0-9-]*', args.programs)
        prepare(args.prepare, args.project.resolve(), args.runtime, args.programs)
    else:
        assert not args.runtime and not args.programs and args.labels and args.output and args.rounds >= 3
        measure(args.labels, args.rounds, args.output)


if __name__ == '__main__':
    main()
