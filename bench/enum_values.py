#!/usr/bin/env python3
"""Measure enum-heavy source checks and portable loading against preserved tools."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def fixture(size, iterations):
    values = [7 + 3 * index for index in range(size)]
    members = '\n'.join(f'V{index} :: {value};' for index, value in enumerate(values))
    uses = '\n'.join(f'total += cast(s64)(Value.V{index});' for index in range(size))
    source = f'''Value :: enum s64 {{
{members}
}}
#program_export
Answer :: () -> s64 {{
    total: s64 = 0
    for 0..{iterations - 1} {{
{uses}
    }}
    return total
}}
'''
    return source, sum(values) * iterations


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--before-dir', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--sizes', type=int, nargs='+', default=[32, 128])
    parser.add_argument('--iterations', type=int, default=32)
    parser.add_argument('--repetitions', type=int, default=9)
    parser.add_argument('--runs', type=int, default=9)
    parser.add_argument('--project', nargs=3, action='append', default=[],
                        metavar=('NAME', 'ROOT', 'ENTRY'))
    args = parser.parse_args()
    if not args.sizes or len(set(args.sizes)) != len(args.sizes) or any(
            size < 1 or size > 256 for size in args.sizes):
        parser.error('unique enum sizes from 1 to 256 required')
    if not 1 <= args.iterations <= 128 or args.repetitions < 1 or not 1 <= args.runs <= 10000:
        parser.error('iterations 1..128, positive repetitions and runs 1..10000 required')
    before = args.before_dir.resolve()
    for name in ['zi2zir', 'zi2zx', 'libziran.a']:
        if not (before/name).is_file(): parser.error(f'missing preserved baseline: {name}')
    output = args.out.resolve()
    if output.exists():
        parser.error('archive needed receipts and remove inactive outputs before reusing --out')
    output.mkdir(parents=True)
    env = dict(os.environ)
    for name in ['DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'DBUS_SESSION_BUS_ADDRESS',
                 'ZIRAN_PROFILE', 'ZIRAN_VM_PROFILE']:
        env.pop(name, None)
    command = [sys.executable, str(ROOT/'bench/vm_startup.py'),
               '--before-dir', str(before), '--out', str(output/'comparison'),
               '--repetitions', str(args.repetitions), '--runs', str(args.runs)]
    manifest = dict(generator_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                    iterations=args.iterations, cases=[],
                    oracle='Sum the explicit integer sequence 7 + 3*i, then multiply by loop iterations.')
    for size in args.sizes:
        directory = output/f'enums-{size}'
        directory.mkdir()
        source, expected = fixture(size, args.iterations)
        entry = directory/'values.zi'
        entry.write_text(source)
        bundle = directory/'values.zx'
        commands = [
            [ROOT/'build/bin/zi2zir', '--root', directory, '-o', directory/'ir', entry],
            [ROOT/'build/bin/zi2zx', 'bundle', '--root', directory, '--entry', 'values:Answer', '-o', bundle, entry],
            [ROOT/'build/bin/zi2zx', 'bundle', '--root', directory/'ir', '--entry', 'values:Answer',
             '-o', directory/'saved.zx', directory/'ir/values.zir'],
        ]
        for argv in commands:
            subprocess.run([str(part) for part in argv], cwd=ROOT, env=env, check=True)
        if bundle.read_bytes() != (directory/'saved.zx').read_bytes():
            raise AssertionError('source and saved enum bundles differ')
        manifest['cases'].append(dict(size=size, source=str(entry), bundle=str(bundle), expected=expected,
                                      source_sha256=hashlib.sha256(entry.read_bytes()).hexdigest(),
                                      bundle_sha256=hashlib.sha256(bundle.read_bytes()).hexdigest()))
        command.extend(['--case', f'enums{size}', str(entry), str(bundle), str(expected)])
    for project in args.project: command.extend(['--project', *project])
    (output/'fixtures.json').write_text(json.dumps(manifest, indent=2)+'\n')
    subprocess.run(command, cwd=ROOT, env=env, check=True)


if __name__ == '__main__':
    main()
