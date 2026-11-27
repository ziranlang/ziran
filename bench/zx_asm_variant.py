#!/usr/bin/env python3
"""Measure one ZX artifact on the native backend with AVX2 enabled and masked."""

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import struct
import subprocess
import sys
import tempfile
import time


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
from benchmark_samples import GATE_METHOD, HostGate, summarize_samples

TOOLS = ROOT / 'build/bin'
FIXTURE = ROOT / 'bench/zx_asm_variant_fixture.zi'
HOST = ROOT / 'bench/zx_compare_host.c'


def run(*args, env=None):
    return subprocess.run(args, check=True, cwd=ROOT, env=env, text=True, stdout=subprocess.PIPE).stdout


def identity(path):
    with path.open('rb') as stream:
        return {'bytes': path.stat().st_size, 'sha256': hashlib.file_digest(stream, 'sha256').hexdigest()}


def checksum(seed, rounds, batches):
    return sum(((seed + batch + lane + rounds * (lane * 3 + 1)) & 65535) * (lane + 1)
               for batch in range(batches) for lane in range(16))


def function_code(directory, name):
    dump, = directory.glob('*.dump')
    data = dump.read_bytes()
    magic, version, size, machine = struct.unpack_from('<IIII', data)
    if (magic, version, size, machine) != (0x4a695444, 1, len(data), 62):
        raise RuntimeError('invalid x86-64 JIT dump')
    at = 40
    code_bytes = 0
    selected = None
    while at < len(data):
        _, length, _, code_size = struct.unpack_from('<IIQI', data, at)
        end = at + length
        start = end - code_size
        if code_size == 0 or start < at + 48 or end > len(data):
            raise RuntimeError('invalid JIT function extent')
        symbol = data[at + 40:start].split(b'\0', 1)[0].decode()
        code_bytes += code_size
        if symbol.rsplit(':', 1)[-1] == name:
            selected = data[start:end]
        at = end
    if selected is None:
        raise RuntimeError('benchmark kernel missing from JIT dump')
    return selected, code_bytes


def summarize(samples):
    filtered = summarize_samples(samples)
    return {'median': filtered['median_ms'], 'excluded_indices': filtered['excluded_indices'],
            'lower_fence': filtered['lower_ms'], 'upper_fence': filtered['upper_ms']}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--samples', type=int, default=9)
    args = parser.parse_args()
    if args.samples < 5:
        parser.error('at least five paired samples are required')
    virtualization = subprocess.run(['systemd-detect-virt'], text=True, capture_output=True)
    if platform.node() != 'alpha' or virtualization.returncode != 1 or virtualization.stdout.strip() != 'none':
        parser.error('measurement requires physical Alpha')
    model = next(line.split(':', 1)[1].strip() for line in Path('/proc/cpuinfo').read_text().splitlines()
                 if line.startswith('model name'))
    features = next(line.split(':', 1)[1].split() for line in Path('/proc/cpuinfo').read_text().splitlines()
                    if line.startswith('flags'))
    if 'avx2' not in features:
        parser.error('AVX2 is required on Alpha')
    modes = {'portable': '0', 'avx2': None}
    seed, rounds, batches = 81, 20000, 48
    expected = checksum(seed, rounds, batches)
    rows = {mode: [] for mode in modes}
    evidence = {}
    with tempfile.TemporaryDirectory(dir=ROOT / 'build', prefix='zx-asm-benchmark-') as work:
        work = Path(work)
        # Use the module name expected by the existing comparison host's binding.
        source = work / 'zx_compare.zi'
        shutil.copyfile(FIXTURE, source)
        bundle = work / 'variant.zx'
        started = time.monotonic_ns()
        run(TOOLS / 'zi2zx', '--root', work, '--entry', 'zx_compare:Answer', '--opt', '-o', bundle, source)
        producer_ms = (time.monotonic_ns() - started) / 1e6
        runner = work / 'zx-host'
        run('cc', '-O3', '-std=c11', '-Iinclude', HOST, ROOT / 'build/libziran.a', '-lm', '-ldl', '-pthread', '-o', runner)

        def trial(mode, dump=None):
            env = os.environ.copy()
            env.pop('ZIRAN_ZX_ASM_FEATURE_MASK', None)
            env.pop('ZIRAN_JIT_DUMP', None)
            env['ZIRAN_NATIVE_CACHE'] = 'off'
            if modes[mode] is not None:
                env['ZIRAN_ZX_ASM_FEATURE_MASK'] = modes[mode]
            if dump is not None:
                env['ZIRAN_JIT_DUMP'] = str(dump)
            sample = json.loads(run(runner, 'native', bundle, str(seed), str(rounds), '0', str(batches), str(expected), env=env))
            if not sample['native_ok'] or sample['backend'] != 'NativeX86_64' or sample['checksum'] != expected:
                raise RuntimeError(f'incorrect {mode} run: {sample}')
            sample['lanes_per_second'] = batches * rounds * 16000 / sample['warm_ms']
            return sample

        for mode in modes:
            dump = work / mode
            dump.mkdir()
            trial(mode, dump)
            code, code_bytes = function_code(dump, 'AddLanes')
            raw = dump / 'kernel.bin'
            raw.write_bytes(code)
            disassembly = run('objdump', '-D', '-b', 'binary', '-m', 'i386:x86-64', raw)
            if ('vpaddw' in disassembly) != (mode == 'avx2'):
                raise RuntimeError(f'{mode} did not select the expected implementation')
            evidence[mode] = {'kernel': identity(raw), 'total_function_code_bytes': code_bytes,
                              'vector_add_mnemonic': 'vpaddw' if mode == 'avx2' else None}
        host_gate = HostGate()
        for trial_index in range(args.samples):
            for mode in (('portable', 'avx2') if trial_index % 2 == 0 else ('avx2', 'portable')):
                host_gate.choose_core()
                rows[mode].append(host_gate.measure(lambda: trial(mode)))
        artifacts = {'bundle': identity(bundle), 'runner': identity(runner), 'compiler': identity(TOOLS / 'zi2zx'),
                     'runtime': identity(ROOT / 'build/libziran.a'), 'fixture': identity(FIXTURE), 'host_source': identity(HOST)}

    metrics = ('load_ms', 'instantiate_ms', 'first_ms', 'warm_ms', 'lanes_per_second')
    summary = {mode: {metric: summarize([sample[metric] for sample in cohort]) for metric in metrics}
               for mode, cohort in rows.items()}
    report = {'host': platform.node(), 'virtualization': 'none', 'cpu': model, 'cpu_flags': features,
              'measured_at': datetime.now(timezone.utc).isoformat(), 'compiler_revision': run('git', 'rev-parse', 'HEAD').strip(),
              'seed': seed, 'rounds_per_batch': rounds, 'batches': batches, 'checksum': expected,
              'settings': '--opt, native backend, disk cache off, paired alternating order',
              'cpu_feature_mask': modes, 'producer_ms': producer_ms, 'artifacts': artifacts, 'selection': evidence,
              'sample_policy': 'symmetric 1.5xIQR, inclusive quartiles, zero-based indices.' + GATE_METHOD,
              'samples': rows, 'summary': summary}
    with args.output.open('x') as output:
        json.dump(report, output, indent=2)
        output.write('\n')
    for mode, by_metric in summary.items():
        print(f"{mode}: {by_metric['lanes_per_second']['median']:.0f} lanes/s")


if __name__ == '__main__':
    main()
