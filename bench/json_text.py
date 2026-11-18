#!/usr/bin/env python3
"""Build Ziran-authored JSON/UTF-8 workloads and measure all four targets on Alpha.

Uses the existing target hosts and their independent checksum validation. This
is build/measurement orchestration; workload logic and oracles live in Ziran.
"""
import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import platform
import random
import shutil
import sys

ROOT = Path(__file__).resolve().parents[1]
SOURCE_SITE = ROOT / 'site'
SITE = SOURCE_SITE
HARNESS_ROOT = Path(os.environ.get('ZIRAN_BENCH_HARNESS_ROOT', ROOT)).resolve()
sys.path.insert(0, str(HARNESS_ROOT / 'scripts'))
import site_targets as base
import site_target_platforms as hosts
from benchmark_samples import GATE_METHOD, HostGate
base.REPO = ROOT
base.SITE = SITE
hosts.ROOT = HARNESS_ROOT
hosts.SITE = base.SITE

CASES = [
    ('json_string_scan', 'JSON string scan · 16 KiB', 16418),
    ('json_parse', 'JSON parse · 16 KiB string', 16624),
    ('json_escaped', 'JSON parse · 128 escaped Unicode objects', 43904),
    ('json_write', 'JSON write · 256 mixed Unicode strings', 5446),
    ('json_quote_plain', 'JSON write · 64 KiB ASCII string', 65606),
    ('text_processing', 'UTF-8 · scan, search, replace and split · 18 KiB', 33788),
]
OUTPUT = SITE / 'generated/json-text'
WORK = ROOT / 'build/benchmarks/json-text'
HARNESSES = ['bench/json_text.py', 'scripts/site_target_platforms.py', 'scripts/site_targets.py',
             'scripts/benchmark_samples.py', 'scripts/site_target_node.cjs',
             'bench/site_target_host.c', 'bench/site_target_host.go', 'site/target-abi.js', 'site/target-measure.js']


def build(compiler, samples):
    if platform.node() != 'alpha':
        raise RuntimeError('Benchmark compilation and measurement require the selected Alpha physical host')
    os.nice(19)
    OUTPUT.mkdir(parents=True, exist_ok=True)
    WORK.mkdir(parents=True, exist_ok=True)
    env = hosts.environment()
    env['PATH'] = str(Path.home() / '.local/bin') + ':' + env['PATH']
    env['ZIRAN_JOBS'] = '1'
    env['ZIRAN_LEARN'] = 'off'
    # All helpers use the same scrubbed environment, including downstream Go.
    hosts.environment = lambda: env.copy()
    data = {'schema_version': 1, 'revision': hosts.run(['git', 'rev-parse', 'HEAD'], cwd=compiler), 'checks': False, 'learning': 'off', 'host': platform.node() + ' · ' + base.cpu_name(),
            'measured_at': datetime.now(timezone.utc).isoformat(), 'samples': samples,
            'compiler': {tool: base.sha(compiler / 'build/bin' / tool)
                         for tool in ('zi2c', 'zi2go', 'zi2zx', 'zx2wasm')},
            'stdlib': {str(p.relative_to(compiler)): base.sha(p) for p in sorted((compiler / 'std').glob('*.zi'))},
            'harnesses': {p: base.sha((ROOT if p == 'bench/json_text.py' else HARNESS_ROOT) / p) for p in HARNESSES},
            'versions': {name: hosts.run(command, env=env).splitlines()[0] for name, command in
                         {'cc': ['cc', '--version'], 'go': ['go', 'version'], 'node': ['node', '--version']}.items()},
            'methodology': hosts.metadata('', '', {}, samples)['methodology'].removesuffix(' Shared machines and uncontrolled clocks; no universal target ranking.') + GATE_METHOD, 'examples': []}
    zx = WORK / 'zx-runner'
    hosts.runtime_build('cc', compiler / 'build/libziran.a', zx,
                        ['-include', HARNESS_ROOT / 'bench/site_target_backend.h',
                         '-DMATRIX_ZX_BACKEND=BUNDLE_BACKEND_NATIVE'])
    data['zx_backend'] = 'native'
    jobs = []
    for name, title, expected in CASES:
        source = ROOT / 'bench/json_text' / (name + '.zi')
        public_source=OUTPUT/'sources'/source.name
        public_source.parent.mkdir(parents=True,exist_ok=True)
        shutil.copyfile(source,public_source)
        row = {'id': name, 'title': title, 'source': public_source.relative_to(SITE).as_posix(),
               'source_input':source.relative_to(ROOT).as_posix(), 'entry': 'Answer',
               'expected': str(expected), 'stdout': '', 'source_sha256': base.sha(source), 'targets': {}}
        for target in ('c', 'go', 'zx', 'wasm'):
            directory = WORK / name / target
            if directory.exists():
                shutil.rmtree(directory)
            directory.mkdir(parents=True, exist_ok=True)
            common = ['--root', source.parent, '--module-path', compiler / 'std',
                      '--entry', name + ':Answer']
            if target in ('c', 'go'):
                command = [compiler / 'build/bin' / ('zi2' + target), '--no-main',
                           *(['--pkg', 'main'] if target == 'go' else []), *common,
                           '-o', directory, source]
                commands = [command]
            elif target == 'zx':
                command = [compiler / 'build/bin/zi2zx', *common,
                           '-o', directory / (name + '.zx'), source]
                commands = [command]
            else:
                wire = directory / (name + '.zx')
                command = [compiler / 'build/bin/zi2zx', '--target', 'wasm', *common,
                           '-o', wire, source]
                commands = [command, [compiler / 'build/bin/zx2wasm',
                                      '-o', directory / (name + '.wasm'), wire]]
            try:
                for stage in commands:
                    hosts.run(stage, env=env)
            except RuntimeError as error:
                if target!='wasm' or 'outside the wasm execution subset' not in str(error):
                    raise
                row['targets'][target]={'status':'unavailable','files':[], 'diagnostic':str(error)}
                continue
            public = OUTPUT / name / target
            public.mkdir(parents=True, exist_ok=True)
            emitted = []
            for path in sorted(directory.rglob('*')):
                if path.suffix not in ({'.c', '.h'} if target == 'c' else {'.go'} if target == 'go' else {'.' + target}):
                    continue
                destination = public / path.relative_to(directory)
                destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(path, destination)
                emitted.append(hosts.public(destination))
            cell = {'status': 'ok', 'files': emitted,
                    'command': '\n'.join(base.command_text(stage) for stage in commands)}
            row['targets'][target] = cell
            if target == 'c':
                program = WORK / name / 'c-program'
                cell['build_command'] = base.command_text(hosts.c_build(row, 'cc', program))
                invoke = [program, '-', expected, samples]
            elif target == 'go':
                program = WORK / name / 'go-program'
                build_command = ['go', 'build', '-p=1', '-o', program, *hosts.go_source(row, WORK / name / 'go-host')]
                hosts.run(build_command, env=env)
                cell['build_command'] = base.command_text(build_command)
                invoke = [program, '-', expected, samples]
            elif target == 'zx':
                invoke = [zx, base.SITE / emitted[0]['path'], expected, samples]
            else:
                invoke = ['node', HARNESS_ROOT / 'scripts/site_target_node.cjs', base.SITE / emitted[0]['path'], expected, samples]
            jobs.append((row, cell, invoke))
        data['examples'].append(row)
    random.Random(42).shuffle(jobs)
    gate = HostGate()
    timed_env = gate.environment(env)
    for row, cell, command in jobs:
        print('Measuring', row['id'], command[0], flush=True)
        measured = hosts.measure(command, row, timed_env, gate)
        cell['measurement'] = {**measured, 'command': base.command_text(command)}
    (OUTPUT / 'matrix.json').write_text(json.dumps(data, indent=2) + '\n')


def check():
    data = json.loads((OUTPUT / 'matrix.json').read_text())
    assert data['schema_version'] == 1 and data['host'].startswith('alpha · ') and 3 <= data['samples'] <= 31
    assert [(row['id'], row['expected']) for row in data['examples']] == [(name, str(oracle)) for name, _, oracle in CASES]
    base.check_committed(base.published_revision(OUTPUT / 'matrix.json'), data['harnesses'], 'Benchmark harness')
    base.check_committed(data['revision'], data['stdlib'], 'Standard library')
    base.check_committed(data['revision'], {row['source_input']: row['source_sha256'] for row in data['examples']}, 'Workload')
    for row in data['examples']:
        assert base.sha(SITE / row['source']) == row['source_sha256']
        assert set(row['targets']) == {'c', 'go', 'zx', 'wasm'}
        for cell in row['targets'].values():
            if cell['status']=='unavailable':
                assert row['targets']['wasm'] is cell and 'outside the wasm execution subset' in cell['diagnostic'] and not cell['files']
                continue
            measured = cell['measurement']
            assert measured['stdout_verified'] and hosts.summarize(measured, row['expected']) == measured
            assert len(measured['batch_samples_ms']) == data['samples']
            for file in cell['files']:
                path = SITE / file['path']
                assert path.stat().st_size == file['bytes'] and base.sha(path) == file['sha256']
    print('Alpha JSON/text source, artifact, oracle and raw-sample local validation passed')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler-root', type=Path, default=ROOT)
    parser.add_argument('--site-dir', type=Path, default=SOURCE_SITE,
                        help='write generated artifacts and the results page under this directory')
    parser.add_argument('--samples', type=int, default=11)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    SITE = args.site_dir.resolve()
    base.SITE = hosts.SITE = SITE
    OUTPUT = SITE / 'generated/json-text'
    if not 3 <= args.samples <= 31:
        parser.error('--samples must be between 3 and 31')
    check() if args.check else build(args.compiler_root.resolve(), args.samples)
