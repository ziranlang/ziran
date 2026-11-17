#!/usr/bin/env python3
"""Compare native and Wasm code with the instruction engine and generated C.

For each kernel of bench/wasm_tier.zi this builds generated C (cc -O2), a
revision-28 bundle, its native ELF (zx2native), and its wasm32 module (zx2wasm),
checks those and independent Go references against the engine's result and output, and times batched calls with
the site matrix's harnesses: bench/site_target_host.c for C and the engine,
scripts/site_target_node.cjs for the module in Node. Medians follow the
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

SOURCE = ROOT / 'bench/wasm_tier.zi'
WORK = ROOT / 'build/wasm-tier'
KERNELS = ['Sieve', 'Records', 'Hash', 'Vectors', 'Printing']
TARGETS = ['c', 'go', 'zx', 'native', 'wasm']
NATIVE_BATCHES = 32


def tool(name):
    return ROOT / 'build/bin' / name


def loadavg():
    try:
        return Path('/proc/loadavg').read_text().split()[:3]
    except OSError:
        return []


def host_snapshot():
    """Read host diagnostics outside the sample's timed interval."""
    frequencies = {}
    try:
        paths = sorted(Path('/sys/devices/system/cpu').glob('cpu[0-9]*/cpufreq/scaling_cur_freq'))
    except OSError:
        paths = []
    for path in paths:
        try:
            value = int(path.read_text().strip())
            frequencies[path.parent.parent.name] = value if value > 0 else None
        except (OSError, ValueError):
            frequencies[path.parent.parent.name] = None
    try:
        affinity = sorted(os.sched_getaffinity(0))
    except (AttributeError, OSError):
        affinity = None
    return {'monotonic_ns': time.monotonic_ns(), 'loadavg': loadavg(),
            'allowed_cpus': affinity, 'scaling_cur_freq_khz': frequencies or None}


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
    runner = WORK / 'zx-runner'
    platforms.runtime_build(cc, ROOT / 'build/libziran.a', runner,
                            ['-include', ROOT / 'bench/site_target_backend.h',
                             '-DMATRIX_ZX_BACKEND=BUNDLE_BACKEND_ENGINE'])
    kernels = []
    for name in KERNELS:
        bundle, module = WORK / (name + '.zx'), WORK / (name + '.wasm')
        native_check = WORK / (name + '-check.native')
        platforms.run([tool('zi2zx'), '--target', 'wasm', *common, '--entry', 'wasm_tier:' + name, '-o', bundle, SOURCE], env=env)
        platforms.run([tool('zx2wasm'), '-o', module, bundle], env=env)
        platforms.run([tool('zx2native'), '-o', native_check, bundle], env=env)
        # The engine's own run is the oracle: its output, then its result line.
        output = subprocess.run([tool('zi2zx'), 'run', bundle], env=env, check=True, capture_output=True, text=True).stdout
        void = name == 'Printing'
        expected, stdout = ('0', output) if void else (output.splitlines()[-1], output[:output.rstrip('\n').rfind('\n') + 1])
        checked = subprocess.run([native_check], env=env, capture_output=True, text=True)
        if checked.returncode != int(expected) % 256 or checked.stdout != stdout:
            raise RuntimeError(f'{name}/native: output {checked.stdout!r}, status {checked.returncode}; '
                               f'expected {stdout!r}, status {int(expected) % 256}')
        wrapper = WORK / ('native_' + name + '.zi')
        call = f'Tier.{name}()'
        if void:
            body = f'for iteration: 0..{NATIVE_BATCHES - 1} {{ {call} }}\n    return 42'
        else:
            body = (f'checksum: s64\n    for iteration: 0..{NATIVE_BATCHES - 1} '
                    f'{{ checksum += {call} }}\n    if checksum != {int(expected) * NATIVE_BATCHES} {{ return 1 }}\n    return 42')
        wrapper.write_text(f'Tier :: #import "wasm_tier";\n#program_export\nmain :: () -> s32 {{\n    {body}\n}}\n')
        native_bundle = WORK / (name + '-native.zx')
        native = WORK / (name + '.native')
        platforms.run([tool('zi2zx'), '--root', WORK, '--module-path', ROOT / 'bench', '--module-path', ROOT / 'std',
                       '--entry', wrapper.stem + ':main', '-o', native_bundle, wrapper], env=env)
        platforms.run([tool('zx2native'), '-o', native, native_bundle], env=env)
        symbol = re.search(r'\b(\w*' + name + r')\(void\)', header.read_text())[1]
        program = WORK / (name + '-c')
        platforms.run([cc, '-O2', '-std=c11', '-DMATRIX_NATIVE', '-I' + str(generated), '-DMATRIX_HEADER="wasm_tier.h"',
                       '-DMATRIX_ENTRY=' + symbol, *(['-DMATRIX_VOID'] if void else []), ROOT / 'bench/site_target_host.c',
                       *sources, '-lm', '-o', program], env=env)
        kernels.append({'id': name, 'expected': expected, 'stdout': stdout, 'artifacts': {
            'c': program, 'go': go_reference, 'zx': bundle, 'native': native, 'wasm': module}, 'commands': {
            'c': [program, '-', expected],
            'go': [go_reference, name, expected],
            'zx': [runner, bundle, expected],
            'native': [native],
            'wasm': ['node', ROOT / 'scripts/site_target_node.cjs', module, expected]}})
    return kernels


def measure(kernels, samples, *, observe_host=False):
    jobs = [(kernel, target) for kernel in kernels for target in TARGETS]
    random.Random(42).shuffle(jobs)
    results = {kernel['id']: {} for kernel in kernels}
    for kernel, target in jobs:
        command = kernel['commands'][target]
        if target == 'native':
            warmup_count = 0
            warmup_started = time.perf_counter_ns()
            while (time.perf_counter_ns() - warmup_started) / 1e6 < 150:
                result = subprocess.run(command, env=platforms.environment(), stdout=subprocess.DEVNULL,
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
                result = subprocess.run(command, env=platforms.environment(), stdout=subprocess.DEVNULL,
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
        else:
            command = [*command, str(samples)]
            text = platforms.run(command, env=platforms.environment())
            measured = platforms.summarize(platforms.read_measurement(text, kernel), kernel['expected'])
        measured['command'] = base.command_text(command)
        measured['artifact_sha256'] = platforms.sha(kernel['artifacts'][target])
        measured['artifact_bytes'] = Path(kernel['artifacts'][target]).stat().st_size
        results[kernel['id']][target] = measured
    return results


# Standard suite: independent scalar C and Go references, identical runtime sizes.
STANDARD_CASES = [
    ('fib', 35, 10), ('n-body', 500000, 10), ('spectral-norm', 500, 10),
    ('mandelbrot', 1000, 17), ('fannkuch-redux', 9, 5), ('binary-trees', 14, 6),
    ('fasta', 1000000, 10), ('nqueens', 12, 5), ('matmul', 192, 8),
    ('quicksort', 200000, 17), ('string-hash', 100000, 10),
]


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
        run(['go', 'build', '-o', work/'go', src/'reference.go', src/'host.go'])
        run([tool('zi2zx'), '--root', src, '--module-path', ROOT/'std', '--entry', 'entry:main', '-o', work/'suite.zx', src/'entry.zi'])
        run([tool('zx2wasm'), '-o', work/'suite.wasm', work/'suite.zx'])
        # Instantiate explicitly selects native; compilation occurs before warmup/timing.
        run([cc, '-O2', '-std=c11', '-I'+str(ROOT/'include'), src/'host.c',
             ROOT/'build/libziran.a', '-lm', '-pthread', '-ldl', '-o', work/'native'])
    if arguments.standard_resume:
        # Native runtime is static and Wasm is precompiled. Keep the original
        # build epoch even if another lane has since updated checkout tools.
        provenance = {key: saved[key] for key in ('revision','compiler_dirty','tools','sources','runtime_sha256','artifacts')}
    else:
        provenance = {'revision': base.git('rev-parse','HEAD'),
                      'compiler_dirty': bool(base.git('status', '--porcelain', '--', 'cmd', 'std', 'include', 'bootstrap', 'Makefile')),
                      'tools': {name: standard_sha(tool(name)) for name in ('zi2zx','zx2wasm')},
                      'sources': {str(p.relative_to(ROOT)): standard_sha(p) for p in sorted(src.iterdir()) if p.is_file()},
                      'runtime_sha256': standard_sha(ROOT/'build/libziran.a'),
                      'artifacts': {p.name: standard_sha(p) for p in work.iterdir() if p.is_file()}}
    completed_before = len(rows)
    sizes = {1:50000000, 2:5500, 3:16000, 4:12, 5:21, 6:25000000} if arguments.standard_full else {}
    for kind, (name, size, probe) in enumerate(STANDARD_CASES):
        size = sizes.get(kind, size)
        if kind < completed_before:
            if rows[kind]['id'] != name or rows[kind]['input'] != size:
                raise RuntimeError('Resume preset differs from completed rows')
            continue
        targets = {}
        for n in (probe, size):
            oracle = json.loads(run([work/'c', '-', kind, n, 11, 'oracle']))
            expected = oracle['result']
            commands = {
                'c': [work/'c', '-', kind, n, 11, expected],
                'go': [work/'go', '-', kind, n, 11, expected],
                'zx': [work/'native', work/'suite.zx', kind, n, 11, expected],
                'wasm': ['node', src/'host.cjs', work/'suite.wasm', kind, n, 11, expected],
            }
            for target, command in commands.items():
                measured = platforms.summarize(json.loads(run(command)), expected)
                measured['command'] = base.command_text(command)
                measured['stdout_verified'] = True
                if n == size:
                    targets[target] = {'status': 'ok', 'measurement': measured}
            print(name, n, expected, flush=True)
        rows.append({'id': name, 'title': name, 'input': size, 'probe_input': probe,
                     'expected': expected, 'targets': targets})
        checkpoint.parent.mkdir(parents=True, exist_ok=True)
        staged_checkpoint = checkpoint.with_suffix('.next.json')
        staged_checkpoint.write_text(json.dumps({**provenance, 'complete':False, 'examples':rows}, indent=2)+'\n')
        staged_checkpoint.replace(checkpoint)
        if arguments.standard_case_limit and len(rows)-completed_before >= arguments.standard_case_limit and len(rows)<len(STANDARD_CASES):
            print('Checkpoint saved; queue slot released between workloads', flush=True)
            return
    data = {**provenance, 'schema_version': 1, 'set': 'game-performance' if arguments.standard_full else 'diagnostic',
            'revision': provenance['revision'],
            'host': platform.node(), 'cpu': base.cpu_name(), 'virtualization': 'none',
            'measured_at': datetime.now(timezone.utc).isoformat(), 'samples': 11,
            'checks': False, 'learning': 'off', 'sources': provenance['sources'],
            'compiler_provenance': 'artifact-build-epoch; immutable native runner statically contains runtime',
            'methodology': 'Single thread, 150 ms warmup; 11 in-process single-call samples; process startup and compilation excluded. Every result checked against independent cc -O2 C reference. Symmetric 1.5×IQR inclusive interpolated quartiles; raw samples and excluded indices retained. Binary trees include allocation and reclamation (Go GC). Mandelbrot and FASTA digest output bytes without I/O; floats rounded to nine decimals.',
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
    parser.add_argument('--standard-full', action='store_true', help='use Game performance sizes (requires --standard)')
    parser.add_argument('--standard', action='store_true', help='independent C/Go/ZX native/Wasm standard suite')
    parser.add_argument('--samples', type=int, default=11)
    parser.add_argument('--observe-host', action='store_true',
                        help='record untimed CPU frequency/load snapshots around native kernel samples')
    parser.add_argument('--output', type=Path)
    parser.add_argument('--label', default='')
    arguments = parser.parse_args()
    if not 3 <= arguments.samples <= 31:
        parser.error('--samples must be between 3 and 31')
    if (arguments.standard_full or arguments.standard_resume or arguments.standard_case_limit) and not arguments.standard:
        parser.error('--standard-full requires --standard')
    if arguments.standard:
        if arguments.observe_host:
            parser.error('--observe-host applies to kernel native samples, not the standard suite')
        return standard(arguments)
    os.environ['ZIRAN_LEARN'] = 'off'
    cc = os.environ.get('CC', 'cc')
    kernels = build(cc)
    before = loadavg()
    results = measure(kernels, arguments.samples, observe_host=arguments.observe_host)
    data = {'schema_version': 1, 'label': arguments.label, 'host': platform.node() + ' · ' + base.cpu_name(),
            'virtualization': subprocess.run(['systemd-detect-virt'], capture_output=True, text=True).stdout.strip(),
            'measured_at': datetime.now(timezone.utc).isoformat(), 'samples': arguments.samples,
            'revision': base.git('rev-parse', 'HEAD'),
            'checks': False, 'learning': 'off',
            'engine_policy': 'explicit-engine',
            'harness_sha256': {name: base.sha(ROOT / name) for name in
                               ('bench/wasm_tier.py', 'bench/wasm_tier_reference.go', 'bench/site_target_host.c',
                                'bench/site_target_backend.h', 'scripts/site_target_node.cjs')},
            'compiler_dirty': bool(base.git('status', '--porcelain', '--', 'cmd', 'std', 'include', 'bootstrap', 'Makefile')),
            'loadavg_before': before, 'loadavg_after': loadavg(),
            'runtimes': {'c': 'C · cc -O2', 'go': 'Go · go build', 'zx': 'ZX · instruction engine',
                         'native': 'Native ELF · zx2native', 'wasm': 'WebAssembly · zx2wasm in Node / V8'},
            'methodology': 'Warm in-process calls timed by the site matrix harnesses; median per call after symmetric 1.5×IQR '
                           'outlier filtering, raw samples and exclusions retained. Every timed checksum and the printed output are '
                           'verified against the instruction engine. Native samples execute 32 checked calls in one ELF process and '
                           'divide by 32, amortizing one process startup per sample; other targets exclude process startup. Compilation is excluded.',
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
