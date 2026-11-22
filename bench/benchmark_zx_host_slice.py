"""Measure ZX execution with matched Linux/i386 archives on physical Alpha."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import socket
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
from benchmark_samples import HostGate, summarize_samples


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('loader', type=Path)
    parser.add_argument('libraries', type=Path)
    parser.add_argument('inputs', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--hot-blocks', action='store_true', help='Use the integer fixture in place of host slices')
    parser.add_argument('--aggregate-loads', action='store_true', help='Also measure 32 KiB aggregate snapshots')
    args = parser.parse_args()
    host = socket.gethostname()
    virtualization = subprocess.run(['systemd-detect-virt'], text=True, capture_output=True).stdout.strip()
    if host.split('.')[0] != 'alpha' or virtualization != 'none':
        parser.error('A physical Alpha host is required')

    inputs = args.inputs.resolve()
    gate = HostGate()
    workloads = [('integer-blocks', 113475636, 'engine', False)] if args.hot_blocks else [
        ('small', 0x235689BC, 'slice', True), ('large', 0x235689BC, 'slice', True),
    ]
    if args.aggregate_loads:
        workloads.append(('aggregate-load', 499008, 'engine', False))
    workloads.extend([
        ('rill-row', 1622073797, 'engine', False),
        ('sieve', 2262, 'engine', False),
        ('records', 68788, 'engine', False),
    ])
    order = ('baseline', 'candidate', 'candidate', 'baseline',
             'candidate', 'baseline', 'baseline', 'candidate')
    receipt = {'host': host, 'virtualization': virtualization, 'kernel': platform.release(),
               'gate': gate.describe(), 'order': order, 'sha256': {}, 'workloads': {}}
    for artifact in sorted(inputs.iterdir()):
        if artifact.is_file():
            with artifact.open('rb') as stream:
                receipt['sha256'][artifact.name] = hashlib.file_digest(stream, 'sha256').hexdigest()

    def save():
        temp = args.output.with_suffix('.tmp')
        temp.write_text(json.dumps(receipt, indent=2) + '\n')
        temp.replace(args.output)

    for workload, expected, driver, borrow in workloads:
        attempts = []
        samples = {'baseline': [], 'candidate': []}
        receipt['workloads'][workload] = {'attempts': attempts, 'summaries': {}}
        for variant in order:
            env = dict(os.environ)
            env.pop('MATRIX_BORROW_SLICE', None)
            if borrow and variant == 'candidate':
                env['MATRIX_BORROW_SLICE'] = '1'
            command = [args.loader, '--library-path', args.libraries,
                       inputs / f'{variant}-{driver}', inputs / f'{workload}.zx', str(expected), '15']
            for _ in range(gate.ATTEMPTS):
                measurement, observation = gate.observe(lambda: json.loads(
                    subprocess.check_output(command, env=env, text=True)))
                rejection = gate.rejection(observation)
                attempts.append(dict(variant=variant, borrowed=borrow and variant == 'candidate',
                                     measurement=measurement, host_gate=observation, rejection=rejection))
                save()
                print(f'{workload} {variant} cpu{gate.cpu}: {rejection or "accepted"}', flush=True)
                if rejection is None:
                    samples[variant].extend(ms / measurement['batches'] for ms in measurement['batch_samples_ms'])
                    break
                gate.choose_core()
            else:
                raise SystemExit('Host gate rejected every attempt')
        result = receipt['workloads'][workload]
        result['summaries'] = {variant: summarize_samples(values) for variant, values in samples.items()}
        result['speedup'] = result['summaries']['baseline']['median_ms'] / result['summaries']['candidate']['median_ms']
        save()


if __name__ == '__main__':
    main()
