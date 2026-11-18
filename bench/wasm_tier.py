#!/usr/bin/env python3
"""Compare native and Wasm code with the instruction engine and generated C.

For each kernel of bench/wasm_tier.zi this builds generated C (cc -O2), a
revision-28 bundle, its native ELF (zx2native), and its wasm32 module (zx2wasm),
checks those and independent Go references against the engine's result and output, and times batched calls with
the site matrix's harnesses: bench/site_target_host.c for C, the engine and in-process
native translation, scripts/site_target_node.cjs for the module in Node. Medians follow the
symmetric 1.5×IQR rule; raw samples and exclusions are kept.

Run it on Alpha's physical host (AGENTS.md):

    python3 bench/wasm_tier.py --samples 11 --output bench/results/wasm-tier-NAME.json
"""
import argparse
import hashlib
import json
import os
import platform
import random
import re
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
import site_target_platforms as platforms  # noqa: E402
import site_targets as base  # noqa: E402
from benchmark_samples import GATE_METHOD, HostGate  # noqa: E402

SOURCE = ROOT / 'bench/wasm_tier.zi'
WORK = ROOT / 'build/wasm-tier'
KERNELS = ['Sieve', 'Records', 'Hash', 'Vectors', 'Printing']
TARGETS = ['c', 'go', 'zx', 'native', 'wasm']


def tool(name):
    return ROOT / 'build/bin' / name


def loadavg():
    try:
        return Path('/proc/loadavg').read_text().split()[:3]
    except OSError:
        return []


def build(cc):
    WORK.mkdir(parents=True, exist_ok=True)
    env = platforms.environment()
    common = ['--root', ROOT / 'bench', '--module-path', ROOT / 'std']
    generated = WORK / 'c'
    platforms.run([tool('ziran'), 'build', '--target=c', *common, '-o', generated, SOURCE], env=env)
    go_reference = WORK / 'go-reference'
    platforms.run(['go', 'build', '-o', go_reference, ROOT / 'bench/wasm_tier_reference.go'], env=env)
    header = generated / 'wasm_tier.h'
    sources = sorted(generated.rglob('*.c'))
    # One host, two explicit backends: the engine, and native code translated from the
    # bundle when it is instantiated (before warmup), so neither pays process startup.
    runners = {}
    for target, backend in (('zx', 'BUNDLE_BACKEND_ENGINE'), ('native', 'BUNDLE_BACKEND_NATIVE')):
        runners[target] = WORK / (target + '-runner')
        platforms.runtime_build(cc, ROOT / 'build/libziran.a', runners[target],
                                ['-include', ROOT / 'bench/site_target_backend.h',
                                 '-DMATRIX_ZX_BACKEND=' + backend])
    kernels = []
    for name in KERNELS:
        bundle, module = WORK / (name + '.zx'), WORK / (name + '.wasm')
        native_check = WORK / (name + '-check.native')
        platforms.run([tool('zi2zx'), '--target', 'wasm', *common, '--entry', 'wasm_tier:' + name, '-o', bundle, SOURCE], env=env)
        # Like C, Go and the in-process ZX runners, a timed call has no step
        # budget, so the module charges none: no fuel, no zx.budget import.
        platforms.run([tool('zx2wasm'), '--unbounded', '-o', module, bundle], env=env)
        platforms.run([tool('zx2native'), '-o', native_check, bundle], env=env)
        # The engine's own run is the oracle: its output, then its result line.
        output = subprocess.run([tool('zi2zx'), 'run', bundle], env=env, check=True, capture_output=True, text=True).stdout
        void = name == 'Printing'
        expected, stdout = ('0', output) if void else (output.splitlines()[-1], output[:output.rstrip('\n').rfind('\n') + 1])
        checked = subprocess.run([native_check], env=env, capture_output=True, text=True)
        if checked.returncode != int(expected) % 256 or checked.stdout != stdout:
            raise RuntimeError(f'{name}/native: output {checked.stdout!r}, status {checked.returncode}; '
                               f'expected {stdout!r}, status {int(expected) % 256}')
        symbol = re.search(r'\b(\w*' + name + r')\(void\)', header.read_text())[1]
        program = WORK / (name + '-c')
        platforms.run([cc, '-O2', '-std=c11', '-DMATRIX_NATIVE', '-I' + str(generated), '-DMATRIX_HEADER="wasm_tier.h"',
                       '-DMATRIX_ENTRY=' + symbol, *(['-DMATRIX_VOID'] if void else []), ROOT / 'bench/site_target_host.c',
                       *sources, '-lm', '-o', program], env=env)
        kernels.append({'id': name, 'expected': expected, 'stdout': stdout, 'artifacts': {
            'c': program, 'go': go_reference, 'zx': bundle, 'native': bundle, 'wasm': module}, 'commands': {
            'c': [program, '-', expected],
            'go': [go_reference, name, expected],
            'zx': [runners['zx'], bundle, expected],
            'native': [runners['native'], bundle, expected],
            'wasm': ['node', ROOT / 'scripts/site_target_node.cjs', module, expected]}})
    return kernels


def native_samples(kernel, command, samples, observe_host, env):
    warmup_count = 0
    warmup_started = time.perf_counter_ns()
    while (time.perf_counter_ns() - warmup_started) / 1e6 < 150:
        result = subprocess.run(command, env=env, stdout=subprocess.DEVNULL,
                                stderr=subprocess.PIPE, text=True)
        if result.returncode != 42:
            raise RuntimeError(f'{kernel["id"]}/native warmup exited {result.returncode}: {result.stderr}')
        warmup_count += 1
    batch_samples = []
    observations = []
    for sample_index in range(samples):
        if observe_host:
            before = host_snapshot()
        started = time.perf_counter_ns()
        result = subprocess.run(command, env=env, stdout=subprocess.DEVNULL,
                                stderr=subprocess.PIPE, text=True)
        batch_samples.append((time.perf_counter_ns() - started) / 1e6)
        if observe_host:
            observations.append({'sample_index': sample_index,
                                 'before': before, 'after': host_snapshot()})
        if result.returncode != 42:
            raise RuntimeError(f'{kernel["id"]}/native sample exited {result.returncode}: {result.stderr}')
    measured = platforms.summarize({'result': kernel['expected'], 'batches': NATIVE_BATCHES,
                                     'batch_samples_ms': batch_samples}, kernel['expected'])
    measured.update(stdout_verified=True, warmup_count=warmup_count,
                    warmup_ms=(time.perf_counter_ns() - warmup_started) / 1e6)
    if observe_host:
        measured['host_observations'] = observations
        measured['host_observation_scope'] = (
            'Untimed Linux sysfs snapshots for all logical CPUs, not the clock frequency '
            'of the child process; an unpinned process can migrate. Missing readings are null. '
            'Observations do not filter timing samples or change affinity or governor.')
    return measured


def measure(kernels, samples, *, gate=None):
    jobs = [(kernel, target) for kernel in kernels for target in TARGETS]
    random.Random(42).shuffle(jobs)
    results = {kernel['id']: {} for kernel in kernels}
    env = HostGate.environment(platforms.environment()) if gate else platforms.environment()
    for kernel, target in jobs:
        command = [*kernel['commands'][target], str(samples)]
        job = lambda: platforms.summarize(platforms.read_measurement(
            platforms.run(command, env=env), kernel), kernel['expected'])
        measured = gate.measure(job) if gate else job()
        measured['command'] = base.command_text(command)
        measured['artifact_sha256'] = platforms.sha(kernel['artifacts'][target])
        measured['artifact_bytes'] = Path(kernel['artifacts'][target]).stat().st_size
        results[kernel['id']][target] = measured
    return results


# Standard suite: independent scalar C and Go references, identical runtime sizes.
def standard_sha(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as source:
        for block in iter(lambda: source.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def standard(arguments):
    # Direct invocation must enforce the same physical-host policy as bench.zi.
    virt = subprocess.run(['systemd-detect-virt'], capture_output=True, text=True)
    if platform.node().split('.')[0].lower() != 'alpha' or virt.returncode != 1 or virt.stdout.strip() != 'none':
        raise RuntimeError('Standard benchmarks require the physical Alpha host')
    if arguments.samples != 11:
        raise RuntimeError('The standard suite requires exactly 11 samples')
    os.environ['ZIRAN_LEARN'] = 'off'
    env = platforms.environment()
    src = ROOT / 'bench/standard'
    work = ROOT / 'build/standard'
    work.mkdir(parents=True, exist_ok=True)
    cc = os.environ.get('CC', 'cc')
    def run(command):
        return platforms.run(command, env=env, timeout=3600)
    catalog = run([tool('ziran'), 'run', '--target=c', '--root', ROOT/'bench', ROOT/'bench/standard_cases.zi'])
    cases = [line.split('\t') for line in catalog.splitlines()]
    output = arguments.output or ROOT/'bench/results/standard/matrix.json'
    checkpoint = output.with_suffix('.partial.json')
    rows = []
    if arguments.standard_resume:
        saved = json.loads(checkpoint.read_text())
        for name, digest in saved['artifacts'].items():
            if standard_sha(work/name) != digest:
                raise RuntimeError('Cannot resume changed artifact: '+name)
        for name, digest in saved['sources'].items():
            if standard_sha(ROOT/name) != digest:
                raise RuntimeError('Cannot resume changed workload source: '+name)
        rows = saved['examples']
    else:
        run([cc, '-O2', '-std=c11', '-DSTANDARD_REFERENCE', src/'reference.c', src/'host.c', '-lm', '-o', work/'c'])
        run(['go', 'build', '-o', work/'go', src/'reference.go', src/'reference_extra.go', src/'host.go'])
        run([tool('zi2zx'), '--root', src, '--module-path', ROOT/'std', '--entry', 'entry:main', '-o', work/'suite.zx', src/'entry.zi'])
        run([tool('zx2wasm'), '--unbounded', '-o', work/'suite.wasm', work/'suite.zx'])
        # Instantiate explicitly selects native; compilation occurs before warmup/timing.
        run([cc, '-O2', '-std=c11', '-I'+str(ROOT/'include'), src/'host.c',
             ROOT/'build/libziran.a', '-lm', '-pthread', '-ldl', '-o', work/'native'])
    if arguments.standard_resume:
        # Native runtime is static and Wasm is precompiled. Keep the original
        # build epoch even if another lane has since updated checkout tools.
        provenance = {key: saved[key] for key in ('revision','compiler_dirty','host_gate','tools','sources','runtime_sha256','artifacts')}
        if provenance['host_gate'] != arguments.host_gate:
            raise RuntimeError('Resume must keep the --host-gate choice of the saved rows')
    else:
        provenance = {'revision': base.git('rev-parse','HEAD'),
                      'compiler_dirty': bool(base.git('status', '--porcelain', '--', 'cmd', 'std', 'include', 'bootstrap', 'Makefile')),
                      'host_gate': arguments.host_gate,
                      'tools': {name: standard_sha(tool(name)) for name in ('zi2zx','zx2wasm')},
                      'sources': {str(p.relative_to(ROOT)): standard_sha(p) for p in [ROOT/'bench/standard_cases.zi', *sorted(src.iterdir())] if p.is_file()},
                      'runtime_sha256': standard_sha(ROOT/'build/libziran.a'),
                      'artifacts': {name: standard_sha(work/name) for name in ('c', 'go', 'native', 'suite.zx', 'suite.wasm')}}
    gate = HostGate() if arguments.host_gate else None
    if gate:
        env.update(HostGate.environment(platforms.environment()))
    completed_before = len(rows)
    for kind, (name, size, probe, game_size) in enumerate(cases):
        size, probe, game_size = int(size), int(probe), int(game_size)
        if arguments.standard_full and game_size:
            size = game_size
        if kind < completed_before:
            if rows[kind]['id'] != name or rows[kind]['input'] != size:
                raise RuntimeError('Resume preset differs from completed rows')
            continue
        targets = {}
        for n in dict.fromkeys((probe, size)):
            samples = 0 if arguments.standard_verify or n != size else 11
            oracle = json.loads(run([work/'c', '-', kind, n, 0, 'oracle']))
            expected = oracle['result']
            commands = {
                'c': [work/'c', '-', kind, n, samples, expected],
                'go': [work/'go', '-', kind, n, samples, expected],
                'zx': [work/'native', work/'suite.zx', kind, n, samples, expected],
                'wasm': ['node', src/'host.cjs', work/'suite.wasm', kind, n, samples, expected],
            }
            for target, command in commands.items():
                if samples == 0:
                    run(command)
                    continue
                if gate:
                    measured = gate.measure(lambda: platforms.summarize(json.loads(run(command)), expected))
                else:
                    measured = platforms.summarize(json.loads(run(command)), expected)
                measured['command'] = base.command_text(command)
                measured['stdout_verified'] = True
                if n == size:
                    targets[target] = {'status': 'ok', 'measurement': measured}
            print(name, n, expected, flush=True)
        if arguments.standard_verify:
            continue
        rows.append({'id': name, 'title': name, 'input': size, 'probe_input': probe,
                     'expected': expected, 'targets': targets})
        checkpoint.parent.mkdir(parents=True, exist_ok=True)
        staged_checkpoint = checkpoint.with_suffix('.next.json')
        staged_checkpoint.write_text(json.dumps({**provenance, 'complete':False, 'examples':rows}, indent=2)+'\n')
        staged_checkpoint.replace(checkpoint)
        if arguments.standard_case_limit and len(rows)-completed_before >= arguments.standard_case_limit and len(rows)<len(cases):
            print('Checkpoint saved; queue slot released between workloads', flush=True)
            return
    if arguments.standard_verify:
        return
    data = {**provenance, 'schema_version': 2, 'set': 'game-performance' if arguments.standard_full else 'diagnostic',
            'revision': provenance['revision'],
            'host': platform.node(), 'cpu': base.cpu_name(), 'virtualization': 'none',
            'measured_at': datetime.now(timezone.utc).isoformat(), 'samples': 11,
            'checks': False, 'learning': 'off', 'sources': provenance['sources'],
            'compiler_provenance': 'artifact-build-epoch; immutable native runner statically contains runtime',
            'methodology': 'Single thread, 150 ms warmup; 11 in-process single-call samples; process startup and compilation excluded; the Wasm module charges no step budget. Every result checked against independent cc -O2 C reference. Symmetric 1.5×IQR inclusive interpolated quartiles; raw samples and excluded indices retained. Binary trees include allocation and reclamation (Go GC). Mandelbrot and FASTA digest output bytes without I/O; floats rounded to nine decimals.' +
                           (GATE_METHOD if provenance['host_gate'] else ''),
            'examples': rows}
    output = arguments.output or ROOT/'bench/results/standard/matrix.json'
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(data, indent=2)+'\n')
    output.with_suffix('.partial.json').unlink(missing_ok=True)
    print('benchmark | C µs | Go µs | ZX native µs | Wasm µs')
    for row in rows:
        print(row['id'], *(f"{row['targets'][t]['measurement']['execution_ms']*1000:.3f}" for t in ('c','go','zx','wasm')), sep=' | ')


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--standard-resume', action='store_true', help='resume an artifact-verified partial standard receipt')
    parser.add_argument('--standard-case-limit', type=int, default=0, help='release queue after this many completed standard workloads')
    parser.add_argument('--standard-verify', action='store_true', help='check C, Go, native and Wasm output without timing')
    parser.add_argument('--standard-full', action='store_true', help='use Game performance sizes (requires --standard)')
    parser.add_argument('--standard', action='store_true', help='independent C/Go/ZX native/Wasm standard suite')
    parser.add_argument('--samples', type=int, default=11)
    parser.add_argument('--host-gate', action='store_true',
                        help='pin each measured job to a quiet CPU and repeat any run whose clock throttled')
    parser.add_argument('--output', type=Path)
    parser.add_argument('--label', default='')
    arguments = parser.parse_args()
    if not 3 <= arguments.samples <= 31:
        parser.error('--samples must be between 3 and 31')
    if (arguments.standard_full or arguments.standard_resume or arguments.standard_case_limit or arguments.standard_verify) and not arguments.standard:
        parser.error('standard suite options require --standard')
    if arguments.standard_verify and (arguments.standard_resume or arguments.standard_case_limit or arguments.host_gate):
        parser.error('--standard-verify does not accept measurement options')
    if arguments.standard:
        return standard(arguments)
    os.environ['ZIRAN_LEARN'] = 'off'
    cc = os.environ.get('CC', 'cc')
    kernels = build(cc)
    before = loadavg()
    gate = HostGate() if arguments.host_gate else None
    results = measure(kernels, arguments.samples, gate=gate)
    data = {'schema_version': 1, 'label': arguments.label, 'host': platform.node() + ' · ' + base.cpu_name(),
            'virtualization': subprocess.run(['systemd-detect-virt'], capture_output=True, text=True).stdout.strip(),
            'measured_at': datetime.now(timezone.utc).isoformat(), 'samples': arguments.samples,
            'revision': base.git('rev-parse', 'HEAD'),
            'checks': False, 'learning': 'off',
            'engine_policy': 'explicit-engine',
            'host_gate': gate.describe() if gate else None,
            'harness_sha256': {name: base.sha(ROOT / name) for name in
                               ('bench/wasm_tier.py', 'bench/wasm_tier_reference.go', 'bench/site_target_host.c',
                                'bench/site_target_backend.h', 'scripts/site_target_node.cjs')},
            'compiler_dirty': bool(base.git('status', '--porcelain', '--', 'cmd', 'std', 'include', 'bootstrap', 'Makefile')),
            'loadavg_before': before, 'loadavg_after': loadavg(),
            'runtimes': {'c': 'C · cc -O2', 'go': 'Go · go build', 'zx': 'ZX · instruction engine',
                         'native': 'ZX native · translated in process', 'wasm': 'WebAssembly · zx2wasm in Node / V8'},
            'methodology': 'Warm in-process calls timed by the site matrix harnesses; median per call after symmetric 1.5×IQR '
                           'outlier filtering, raw samples and exclusions retained. Every timed checksum and the printed output are '
                           'verified against the instruction engine, and the zx2native ELF of each kernel is run once and checked the same way. '
                           'ZX native times the bundle translated to machine code when it is instantiated, in the same host as C and the engine. '
                           'Process startup and compilation are excluded for every target. The Wasm module charges no step budget.' +
                           (GATE_METHOD if gate else ''),
            'kernels': results}
    if arguments.output:
        arguments.output.parent.mkdir(parents=True, exist_ok=True)
        arguments.output.write_text(json.dumps(data, indent=2) + '\n')
    print(f'{"kernel":10} {"C µs":>10} {"Go µs":>10} {"engine µs":>11} {"native µs":>11} {"wasm µs":>10} {"native/wasm":>12} {"wasm/engine":>12}')
    for name, row in results.items():
        c, go, zx, native, wasm = (row[target]['execution_ms'] * 1000 for target in TARGETS)
        print(f'{name:10} {c:10.3f} {go:10.3f} {zx:11.3f} {native:11.3f} {wasm:10.3f} {native / wasm:12.3f} {wasm / zx:12.3f}')
    print('host', data['host'], data['virtualization'], 'load', before, data['loadavg_after'])


if __name__ == '__main__':
    try:
        main()
    except (RuntimeError, AssertionError, subprocess.CalledProcessError) as error:
        sys.exit(str(error))
