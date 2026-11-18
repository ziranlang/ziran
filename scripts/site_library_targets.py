#!/usr/bin/env python3
"""Measure an ordinary library's manifest-defined entries on desktop C, Go, native ZX and Wasm.

Library behavior and host bindings stay upstream. Publish the exact checked IR,
generated files, source fixtures and raw samples, without adding compiler cases.
"""
import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import platform
import random
import shutil
import zipfile

import site_targets as base
import site_target_platforms as platforms
from benchmark_samples import GATE_METHOD, HostGate

OUTPUT = base.SITE / 'generated/library-targets'
WORK = base.REPO / 'build/site-library-targets'


def public(path):
    return platforms.public(path)


def loadavg():
    return Path('/proc/loadavg').read_text().split()[:3]


def archive(directory, output):
    with zipfile.ZipFile(output, 'w', zipfile.ZIP_DEFLATED) as bundle:
        for path in sorted(directory.rglob('*')):
            if path.is_file():
                # Stable archive metadata; snapshots change only when contents do.
                info = zipfile.ZipInfo(path.relative_to(directory).as_posix())
                info.compress_type = zipfile.ZIP_DEFLATED
                bundle.writestr(info, path.read_bytes())
    return public(output)


TARGETS = ('c', 'go', 'zx', 'wasm')


def build(project, manifest_path, samples, revision=None):
    assert platform.node() == 'alpha', 'Measurements require the selected physical host'
    assert base.execute(['systemd-detect-virt'], allow_failure=True)[0].stdout.strip() == 'none'
    manifest = json.loads(manifest_path.read_text())
    source = project / manifest['source']
    module = source.stem
    ziran = base.REPO / 'build/bin/ziran'
    env = platforms.environment()
    env["ZIRAN_LEARN"] = "off"
    OUTPUT.mkdir(parents=True, exist_ok=True)
    WORK.mkdir(parents=True, exist_ok=True)
    ir = WORK / 'ir'
    ir.mkdir(exist_ok=True)
    bindings = [part for binding in manifest['bindings'] for part in ('--bind', binding)]
    common = ['--root', source.parent, '--project', '--module-path', source.parent]

    def run(command):
        return platforms.run(command, cwd=project, env=env)

    compiler_package = Path(run([ziran, 'pkg', 'path', 'ziran'])).resolve()
    assert compiler_package == base.REPO.resolve(), 'Set the project ziran override to the compiler being measured'
    run([ziran, 'ir', *common, '-o', ir, source])
    source_dir = WORK / 'sources'
    source_dir.mkdir(exist_ok=True)
    for name in manifest['sources']:
        destination = source_dir / name
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(project / name, destination)
    shutil.copyfile(manifest_path, source_dir / manifest_path.name)
    # Named for the library's repository, not the folder it was exported to.
    licenses = {manifest['repository'].rstrip('/').rsplit('/', 1)[-1] + '-LICENSE': project / 'LICENSE',
                'Ziran-LICENSE': base.REPO / 'LICENSE',
                'Unicode-LICENSE': base.REPO / 'LICENSE-Unicode'}

    def add_licenses(directory):
        destination = directory / 'licenses'
        shutil.rmtree(destination, ignore_errors=True)  # no stale copies from an earlier run
        destination.mkdir()
        for name, path in licenses.items():
            if path.is_file():
                shutil.copyfile(path, destination / name)

    add_licenses(source_dir)
    add_licenses(ir)
    data = {'schema_version': 1, 'name': manifest['name'], 'repository': manifest['repository'],
            # An exported tree (git archive) carries no history: it names its commit.
            'revision': revision or run(['git', 'rev-parse', 'HEAD']),
            'dirty_inputs': False if revision else
                bool(run(['git', 'status', '--porcelain', '--', 'src', 'tests', 'ziran.toml', 'ziran.lock'])),
            'measured_at': datetime.now(timezone.utc).isoformat(), 'host': base.cpu_name(),
            'hostname': platform.node(), 'loadavg_before': loadavg(),
            'versions': {'cc': run(['cc', '--version']).splitlines()[0], 'go': run(['go', 'version'])},
            'methodology': manifest['methodology'] + ' Symmetric 1.5×IQR filtering excludes outliers from the median; raw samples and excluded indices are retained. The Wasm module is unbounded, like the C and Go builds: it charges no step budget.' + GATE_METHOD, 'samples': samples,
            'sources': archive(source_dir, OUTPUT / 'sources.zip'),
            'checked_ir': archive(ir, OUTPUT / 'checked-ir.zip'),
            'compiler': {'library_sha256': base.sha(base.REPO / 'build/libziran.a'),
                         'package_path': str(compiler_package),
                         'revision': base.git('rev-parse', 'HEAD'),
                         'dirty': bool(base.git('status', '--porcelain', '--', 'cmd', 'std', 'include', 'bootstrap', 'Makefile')),
                         'tools': {name: base.sha(base.REPO / 'build/bin' / name) for name in ('zi2c', 'zi2go', 'zi2zx', 'zx2wasm')},
                         'source_hashes': {str(p.relative_to(base.REPO)): base.sha(p) for p in
                                           sorted([p for p in (base.REPO / 'cmd/zir').iterdir() if p.suffix in ('.c', '.h')] +
                                                  sorted((base.REPO / 'cmd').glob('compiler_zx_wasm*.zi')) +
                                                  [base.REPO / 'cmd/zx2wasm/main.c'])}},
            'harnesses': {p: base.sha(base.REPO / p) for p in
                          ['scripts/site_library_targets.py', 'scripts/site_target_platforms.py', 'scripts/benchmark_samples.py',
                           'bench/site_target_host.c', 'bench/site_target_host.go', 'bench/site_target_backend.h',
                           'scripts/site_target_node.cjs', 'site/target-abi.js', 'site/target-measure.js']}, 'examples': []}
    runner = WORK / 'zx-runner'
    # Explicit Native policy runs ZX as machine code; a native gap fails
    # instantiation and never falls back to the
    # instruction engine, so a completed measurement is a native one.
    assert platform.system() == 'Linux' and platform.machine() in ('x86_64', 'aarch64'), 'native ZX needs Linux x86-64 or AArch64'
    platforms.runtime_build('cc', base.REPO / 'build/libziran.a', runner,
                            ['-include', base.REPO / 'bench/site_target_backend.h',
                             '-DMATRIX_ZX_BACKEND=BUNDLE_BACKEND_NATIVE'])
    data['zx_backend'] = 'native'
    data['virtualization'] = 'none'
    data['checks'] = False
    data['learning'] = env.get('ZIRAN_LEARN', 'unknown')
    jobs = []
    for case in manifest['cases']:
        row = {**case, 'module': module, 'stdout': '', 'targets': {}}
        entry = f'{module}:{case["entry"]}'
        for target in TARGETS:
            directory = WORK / case['id'] / target
            directory.mkdir(parents=True, exist_ok=True)
            command = [ziran, 'bundle' if target == 'zx' else 'build',
                       *([] if target == 'zx' else ['--target=' + target, '--no-main']),
                       *(['--pkg', 'main'] if target == 'go' else []),
                       *common, *bindings, '--entry', entry, '-o', directory / (module + '.zx') if target == 'zx' else directory, source]
            if target == 'wasm':
                # Like the C and Go builds, a run here has no step budget, so
                # the module is unbounded: no fuel charges, no zx.budget import.
                command = [base.REPO / 'build/bin/zx2wasm', '--unbounded', '-o', directory / (module + '.wasm'),
                           WORK / case['id'] / 'zx' / (module + '.zx')]
            run(command)
            emitted = []
            public_dir = OUTPUT / case['id'] / target
            public_dir.mkdir(parents=True, exist_ok=True)
            for path in sorted(directory.rglob('*')):
                if path.suffix not in ({'.c', '.h'} if target == 'c' else {'.go'} if target == 'go' else {'.' + target}):
                    continue
                dest = public_dir / path.relative_to(directory)
                dest.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(path, dest)
                emitted.append(public(dest))
            cell = {'status': 'ok', 'command': base.command_text(command), 'files': emitted}
            row['targets'][target] = cell
            if target == 'c':
                executable = WORK / case['id'] / 'c-runner'
                cell['build_command'] = base.command_text(platforms.c_build(row, 'cc', executable))
                invocation = [executable, '-', case['expected'], samples]
            elif target == 'go':
                executable = WORK / case['id'] / 'go-runner'
                build_command = ['go', 'build', '-p=2', '-o', executable,
                                 *platforms.go_source(row, WORK / case['id'] / 'go-host')]
                platforms.run(build_command, env=env)
                cell['build_command'] = base.command_text(build_command)
                invocation = [executable, '-', case['expected'], samples]
            elif target == 'wasm':
                invocation = ['node', base.REPO / 'scripts/site_target_node.cjs',
                              base.SITE / emitted[0]['path'], case['expected'], samples]
            else:
                invocation = [runner, base.SITE / emitted[0]['path'], case['expected'], samples]
            jobs.append((row, cell, invocation))
            add_licenses(public_dir)
            cell['download'] = archive(public_dir, OUTPUT / case['id'] / (target + '.zip')) if target in ('c', 'go') else emitted[0]
        data['examples'].append(row)
    random.Random(42).shuffle(jobs)
    gate = HostGate()
    timed_env = gate.environment(env)
    for row, cell, invocation in jobs:
        measured = platforms.measure(invocation, row, timed_env, gate)
        cell['measurement'] = {**measured, 'command': base.command_text(invocation)}
    assert data['compiler']['library_sha256'] == base.sha(base.REPO / 'build/libziran.a'), 'Runtime changed during measurement'
    data['loadavg_after'] = loadavg()
    (OUTPUT / 'matrix.json').write_text(json.dumps(data, indent=2) + '\n')
    print(f'{len(data["examples"])} library entries: C, Go, native ZX and Wasm executed; every batch verified')


def check():
    data = json.loads((OUTPUT / 'matrix.json').read_text())
    assert data['schema_version'] == 1 and data['samples'] >= 3 and data.get('zx_backend') == 'native'
    for name, digest in data['harnesses'].items():
        assert base.sha(base.REPO / name) == digest, f'Stale library harness: {name}'
    files = [data['sources'], data['checked_ir']]
    for row in data['examples']:
        assert set(row['targets']) == set(TARGETS)
        for cell in row['targets'].values():
            files += cell['files'] + [cell['download']]
            measured = cell['measurement']
            assert measured['stdout_verified'] and measured['result'] == row['expected']
            assert len(measured['batch_samples_ms']) == data['samples']
            assert platforms.summarize(measured, row['expected']) == measured
    for file in files:
        path = base.SITE / file['path']
        assert path.stat().st_size == file['bytes'] and base.sha(path) == file['sha256'], f'Stale library output: {path}'
    print('Library output hashes, raw samples, checksums verified')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--project', type=Path)
    parser.add_argument('--manifest', type=Path)
    parser.add_argument('--samples', type=int, default=9)
    parser.add_argument('--revision', help='the commit a --project tree exported with git archive holds')
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    if args.check:
        check()
    else:
        if not args.project or not args.manifest or not 3 <= args.samples <= 31:
            parser.error('Pass --project, --manifest and 3–31 samples')
        build(args.project.resolve(), args.manifest.resolve(), args.samples, args.revision)
