#!/usr/bin/env python3
"""Matched frozen-runtime A/B on the existing wasm_tier kernels (physical Alpha)."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import platform
import random
import subprocess
import sys

import wasm_tier as tier
from benchmark_samples import GATE_METHOD, HostGate, summarize_samples


def identity(path):
    path = Path(path).resolve()
    digest = hashlib.sha256()
    with path.open('rb') as source:
        for block in iter(lambda: source.read(1024 * 1024), b''):
            digest.update(block)
    return {'path': str(path), 'sha256': digest.hexdigest(), 'bytes': path.stat().st_size}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--before', type=Path, required=True)
    parser.add_argument('--after', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    virt = subprocess.run(['systemd-detect-virt'], capture_output=True, text=True)
    if platform.node().split('.')[0].lower() != 'alpha' or virt.returncode != 1 or virt.stdout.strip() != 'none':
        parser.error('measurement requires physical Alpha')
    if args.output.exists():
        parser.error('output already exists')
    os.environ['ZIRAN_LEARN'] = 'off'
    env = HostGate.environment(tier.platforms.environment())
    gate = HostGate()
    rng = random.Random(29)
    kernels, artifacts, oracles = {}, {}, {}
    for name in tier.KERNELS:
        bundle = tier.WORK / (name + '.zx')
        output = subprocess.run([tier.tool('zi2zx'), 'run', '--backend', 'engine', bundle],
                                env=env, check=True, capture_output=True, text=True).stdout
        expected = '0' if name == 'Printing' else output.splitlines()[-1]
        stdout = output if name == 'Printing' else output[:output.rstrip('\n').rfind('\n') + 1]
        example = {'id': name, 'expected': expected, 'stdout': stdout}
        commands = {'c': [tier.WORK / (name + '-c'), '-', expected],
                    'before': [args.before, bundle, expected], 'after': [args.after, bundle, expected]}
        kernels[name] = (example, commands)
        oracles[name] = example
        artifacts[name] = {'bundle': identity(bundle), 'c': identity(commands['c'][0])}
    artifacts.update(before=identity(args.before), after=identity(args.after),
                     engine=identity(tier.tool('zi2zx')), driver=identity(__file__))
    runs = {name: {label: [] for label in ('c', 'before', 'after')} for name in kernels}
    order = []
    for round_index in range(11):
        jobs = [(name, label) for name in kernels for label in runs[name]]
        rng.shuffle(jobs)
        for name, label in jobs:
            example, commands = kernels[name]
            command = [*commands[label], '11']
            measured = tier.platforms.measure(command, example, env, gate)
            measured['command'] = [str(part) for part in command]
            runs[name][label].append(measured)
            order.append([round_index, name, label])
        print(f'round {round_index + 1}/11 complete', flush=True)
    summary = {name: {label: summarize_samples([run['execution_ms'] for run in values])
                      for label, values in labels.items()} for name, labels in runs.items()}
    report = {'host': platform.node(), 'virtualization': virt.stdout.strip(),
              'measured_at': datetime.now(timezone.utc).isoformat(), 'learning': 'off',
              'rounds': 11, 'samples_per_run': 11, 'seed': 29, 'artifacts': artifacts,
              'oracles': oracles, 'order': order, 'runs': runs, 'summary': summary,
              'methodology': '11 shuffled matched rounds; identical bundles, frozen static runtime runners and C references. '
                             'Each run warms 150 ms then measures 11 calibrated batches; every checksum and stdout is verified '
                             'against the instruction engine. Symmetric inclusive 1.5xIQR applied within each run and across '
                             'run medians; raw samples and excluded indices retained. ' + GATE_METHOD}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print('kernel | C us | before us | after us | after/before')
    for name, labels in summary.items():
        c, before, after = (labels[label]['median_ms'] * 1000 for label in ('c', 'before', 'after'))
        print(f'{name} | {c:.3f} | {before:.3f} | {after:.3f} | {after / before:.4f}')


if __name__ == '__main__':
    main()
