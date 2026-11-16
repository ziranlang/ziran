#!/usr/bin/env python3
"""Compare native and Wasm code with the instruction engine and generated C.

For each kernel of bench/wasm_tier.zi this builds generated C (cc -O2), a
revision-28 bundle, its native ELF (zx2native), and its wasm32 module (zx2wasm),
checks that all four give the engine's result and output, and times batched calls with
the site matrix's harnesses: bench/site_target_host.c for C and the engine,
scripts/site_target_node.cjs for the module in Node. Medians follow the
symmetric 1.5×IQR rule; raw samples and exclusions are kept.

Run it on Alpha's physical host (AGENTS.md):

    python3 bench/wasm_tier.py --samples 11 --output bench/results/wasm-tier-NAME.json
"""
import argparse
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
KERNELS = ['Sieve', 'Records', 'Hash', 'Vectors', 'Calls', 'Printing']
TARGETS = ['c', 'zx', 'native', 'wasm']
NATIVE_BATCHES = 32


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
            'c': program, 'zx': bundle, 'native': native, 'wasm': module}, 'commands': {
            'c': [program, '-', expected],
            'zx': [runner, bundle, expected],
            'native': [native],
            'wasm': ['node', ROOT / 'scripts/site_target_node.cjs', module, expected]}})
    return kernels


def measure(kernels, samples):
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
            for _ in range(samples):
                started = time.perf_counter_ns()
                result = subprocess.run(command, env=platforms.environment(), stdout=subprocess.DEVNULL,
                                        stderr=subprocess.PIPE, text=True)
                batch_samples.append((time.perf_counter_ns() - started) / 1e6)
                if result.returncode != 42:
                    raise RuntimeError(f'{kernel["id"]}/native sample exited {result.returncode}: {result.stderr}')
            measured = platforms.summarize({'result': kernel['expected'], 'batches': NATIVE_BATCHES,
                                             'batch_samples_ms': batch_samples}, kernel['expected'])
            measured.update(stdout_verified=True, warmup_count=warmup_count,
                            warmup_ms=(time.perf_counter_ns() - warmup_started) / 1e6)
        else:
            command = [*command, str(samples)]
            text = platforms.run(command, env=platforms.environment())
            measured = platforms.summarize(platforms.read_measurement(text, kernel), kernel['expected'])
        measured['command'] = base.command_text(command)
        measured['artifact_sha256'] = platforms.sha(kernel['artifacts'][target])
        measured['artifact_bytes'] = Path(kernel['artifacts'][target]).stat().st_size
        results[kernel['id']][target] = measured
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--samples', type=int, default=11)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--label', default='')
    arguments = parser.parse_args()
    if not 3 <= arguments.samples <= 31:
        parser.error('--samples must be between 3 and 31')
    os.environ['ZIRAN_LEARN'] = 'off'
    cc = os.environ.get('CC', 'cc')
    kernels = build(cc)
    before = loadavg()
    results = measure(kernels, arguments.samples)
    data = {'schema_version': 1, 'label': arguments.label, 'host': platform.node() + ' · ' + base.cpu_name(),
            'virtualization': subprocess.run(['systemd-detect-virt'], capture_output=True, text=True).stdout.strip(),
            'measured_at': datetime.now(timezone.utc).isoformat(), 'samples': arguments.samples,
            'revision': base.git('rev-parse', 'HEAD'),
            'checks': False, 'learning': 'off',
            'engine_policy': 'explicit-engine',
            'harness_sha256': {name: base.sha(ROOT / name) for name in
                               ('bench/wasm_tier.py', 'bench/site_target_host.c',
                                'bench/site_target_backend.h', 'scripts/site_target_node.cjs')},
            'compiler_dirty': bool(base.git('status', '--porcelain', '--', 'cmd', 'std', 'include', 'bootstrap', 'Makefile')),
            'loadavg_before': before, 'loadavg_after': loadavg(),
            'runtimes': {'c': 'C · cc -O2', 'zx': 'ZX · instruction engine',
                         'native': 'Native ELF · zx2native', 'wasm': 'WebAssembly · zx2wasm in Node / V8'},
            'methodology': 'Warm in-process calls timed by the site matrix harnesses; median per call after symmetric 1.5×IQR '
                           'outlier filtering, raw samples and exclusions retained. Every timed checksum and the printed output are '
                           'verified against the instruction engine. Native samples execute 32 checked calls in one ELF process and '
                           'divide by 32, amortizing one process startup per sample; other targets exclude process startup. Compilation is excluded.',
            'kernels': results}
    if arguments.output:
        arguments.output.parent.mkdir(parents=True, exist_ok=True)
        arguments.output.write_text(json.dumps(data, indent=2) + '\n')
    print(f'{"kernel":10} {"C µs":>10} {"engine µs":>11} {"native µs":>11} {"wasm µs":>10} {"native/wasm":>12} {"wasm/engine":>12}')
    for name, row in results.items():
        c, zx, native, wasm = (row[target]['execution_ms'] * 1000 for target in TARGETS)
        print(f'{name:10} {c:10.3f} {zx:11.3f} {native:11.3f} {wasm:10.3f} {native / wasm:12.3f} {wasm / zx:12.3f}')
    print('host', data['host'], data['virtualization'], 'load', before, data['loadavg_after'])


if __name__ == '__main__':
    try:
        main()
    except (RuntimeError, AssertionError, subprocess.CalledProcessError) as error:
        sys.exit(str(error))
