#!/usr/bin/env python3
"""Compare checked resource loading with two runtime libraries and real bundles."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import random
import statistics
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--before-library', required=True, type=Path)
    parser.add_argument('--library', required=True, type=Path)
    parser.add_argument('--bundle', required=True, action='append', type=Path)
    parser.add_argument('--out', required=True, type=Path)
    parser.add_argument('--repetitions', type=int, default=9)
    parser.add_argument('--runs', type=int, default=4)
    args = parser.parse_args()
    if args.repetitions < 1 or args.runs < 2:
        parser.error('positive repetitions and at least two runs required')
    out = args.out.resolve()
    if out.exists():
        parser.error('output exists; preserve previous measurements')
    out.mkdir(parents=True)
    libraries = {'before': args.before_library.resolve(), 'after': args.library.resolve()}
    bundles = list(dict.fromkeys(p.resolve() for p in args.bundle))
    environment = dict(os.environ)
    for name in ('DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'DBUS_SESSION_BUS_ADDRESS',
                 'ZIRAN_VM_PROFILE'):
        environment.pop(name, None)
    commands = []
    for version, library in libraries.items():
        command = [os.environ.get('CC', 'cc'), '-O2', '-D_GNU_SOURCE', '-std=c11',
                   '-I' + str(ROOT / 'include'), str(ROOT / 'bench/bundle_assets.c'),
                   str(library), '-lm', '-pthread', '-o', str(out / version)]
        subprocess.run(command, check=True, env=environment)
        commands.append(command)
    trials = [(version, mode, bundle) for version in libraries
              for mode in ('bytes', 'file') for bundle in bundles] * args.repetitions
    random.Random(31).shuffle(trials)
    samples = []
    expected = {}
    for version, mode, bundle in trials:
        command = [str(out / version), str(bundle), str(args.runs)]
        if mode == 'file':
            command.append('--file')
        result = subprocess.run(command, check=True, capture_output=True, text=True,
                                env=environment, timeout=180)
        rows = [json.loads(line) for line in result.stdout.splitlines()]
        assert len(rows) == args.runs
        for row in rows:
            identity = tuple(row[key] for key in ('assets', 'asset_bytes', 'checksum'))
            assert expected.setdefault(str(bundle), identity) == identity
        samples.extend(dict(version=version, mode=mode, bundle=str(bundle), **row)
                       for row in rows[1:])
    summary = []
    for version in libraries:
        for mode in ('bytes', 'file'):
            for bundle in bundles:
                rows = [r for r in samples if (r['version'], r['mode'], r['bundle']) ==
                        (version, mode, str(bundle))]
                summary.append(dict(version=version, mode=mode, bundle=str(bundle),
                                    **{key: statistics.median(r[key] for r in rows)
                                       for key in rows[0] if key.endswith('_ms')}))
    report = dict(
        metadata=dict(platform=platform.platform(),
                      cpu_affinity=(sorted(os.sched_getaffinity(0))
                                    if hasattr(os, 'sched_getaffinity') else None),
                      repetitions=args.repetitions, runs=args.runs,
                      discarded_warmups_per_process=1, commands=commands,
                      source_sha256={p.name: digest(p) for p in
                                     (Path(__file__), ROOT / 'bench/bundle_assets.c')},
                      library_sha256={name: digest(path) for name, path in libraries.items()},
                      bundle_sha256={str(p): digest(p) for p in bundles},
                      caveats=['Warm file cache on one shared host; no hardware isolation.',
                               'File mode rereads the path; bytes mode receives an in-memory input.',
                               'Inspection checks every name and byte after freeing caller input.',
                               'These measure loading, not execution or UI response.']),
        summary=summary, samples=samples)
    (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n')
    for row in summary:
        print(f"{row['version']} {row['mode']} {Path(row['bundle']).name}: "
              f"open {row['open_ms']:.3f} ms wall / {row['open_cpu_ms']:.3f} ms CPU")


if __name__ == '__main__':
    main()
