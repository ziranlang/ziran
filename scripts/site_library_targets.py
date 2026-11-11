#!/usr/bin/env python3
"""Measure an ordinary library's manifest-defined entries on desktop C, Go and ZX.

Library behavior and host bindings stay upstream. Publish the exact checked IR,
generated files, source fixtures and raw samples, without adding compiler cases.
"""
import argparse
from datetime import datetime, timezone
import html
import json
from pathlib import Path
import re
import shutil
import statistics
import zipfile

import site_targets as base
import site_target_platforms as platforms

OUTPUT = base.SITE / 'generated/library-targets'
WORK = base.REPO / 'build/site-library-targets'
MARKER = 'library_matrix'


def public(path):
    return platforms.public(path)


def archive(directory, output):
    with zipfile.ZipFile(output, 'w', zipfile.ZIP_DEFLATED) as bundle:
        for path in sorted(directory.rglob('*')):
            if path.is_file():
                # Stable archive metadata; snapshots change only when contents do.
                info = zipfile.ZipInfo(path.relative_to(directory).as_posix())
                info.compress_type = zipfile.ZIP_DEFLATED
                bundle.writestr(info, path.read_bytes())
    return public(output)


def build(project, manifest_path, samples):
    manifest = json.loads(manifest_path.read_text())
    source = project / manifest['source']
    module = source.stem
    ziran = base.REPO / 'build/bin/ziran'
    env = platforms.environment()
    OUTPUT.mkdir(parents=True, exist_ok=True)
    WORK.mkdir(parents=True, exist_ok=True)
    ir = WORK / 'ir'
    ir.mkdir(exist_ok=True)
    bindings = [part for binding in manifest['bindings'] for part in ('--bind', binding)]
    common = ['--root', source.parent, '--project', '--module-path', source.parent]

    def run(command):
        return platforms.run(command, cwd=project, env=env)

    run([ziran, 'ir', *common, '-o', ir, source])
    source_dir = WORK / 'sources'
    source_dir.mkdir(exist_ok=True)
    for name in manifest['sources']:
        destination = source_dir / name
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(project / name, destination)
    shutil.copyfile(manifest_path, source_dir / manifest_path.name)
    licenses = {project.name + '-LICENSE': project / 'LICENSE',
                'Ziran-LICENSE': base.REPO / 'LICENSE',
                'Unicode-LICENSE': base.REPO / 'LICENSE-Unicode'}

    def add_licenses(directory):
        destination = directory / 'licenses'
        destination.mkdir(exist_ok=True)
        for name, path in licenses.items():
            if path.is_file():
                shutil.copyfile(path, destination / name)

    add_licenses(source_dir)
    add_licenses(ir)
    data = {'schema_version': 1, 'name': manifest['name'], 'repository': manifest['repository'],
            'revision': run(['git', 'rev-parse', 'HEAD']),
            'dirty_inputs': bool(run(['git', 'status', '--porcelain', '--', 'src', 'tests', 'ziran.toml', 'ziran.lock'])),
            'measured_at': datetime.now(timezone.utc).isoformat(), 'host': base.cpu_name(),
            'versions': {'cc': run(['cc', '--version']).splitlines()[0], 'go': run(['go', 'version'])},
            'methodology': manifest['methodology'] + ' Symmetric 1.5×IQR filtering excludes outliers from the median; raw samples and excluded indices are retained.', 'samples': samples,
            'sources': archive(source_dir, OUTPUT / 'sources.zip'),
            'checked_ir': archive(ir, OUTPUT / 'checked-ir.zip'),
            'compiler': {'library_sha256': base.sha(base.REPO / 'build/libziran.a'),
                         'revision': base.git('rev-parse', 'HEAD'),
                         'dirty': bool(base.git('status', '--porcelain', '--', 'cmd', 'std', 'include', 'bootstrap', 'Makefile')),
                         'tools': {name: base.sha(base.REPO / 'build/bin' / name) for name in ('zi2c', 'zi2go', 'zi2zx')},
                         'source_hashes': {str(p.relative_to(base.REPO)): base.sha(p) for p in
                                           sorted(p for p in (base.REPO / 'cmd/zir').iterdir() if p.suffix in ('.c', '.h'))}},
            'harnesses': {p: base.sha(base.REPO / p) for p in
                          ['scripts/site_library_targets.py', 'scripts/site_target_platforms.py', 'scripts/benchmark_samples.py',
                           'bench/site_target_host.c', 'bench/site_target_host.go']}, 'examples': []}
    runner = WORK / 'zx-runner'
    platforms.runtime_build('cc', base.REPO / 'build/libziran.a', runner)
    for case in manifest['cases']:
        row = {**case, 'module': module, 'stdout': '', 'targets': {}}
        entry = f'{module}:{case["entry"]}'
        for target in ('c', 'go', 'zx'):
            directory = WORK / case['id'] / target
            directory.mkdir(parents=True, exist_ok=True)
            command = [ziran, 'bundle' if target == 'zx' else 'build',
                       *([] if target == 'zx' else ['--target=' + target, '--no-main']),
                       *(['--pkg', 'main'] if target == 'go' else []),
                       *common, *bindings, '--entry', entry, '-o', directory / (module + '.zx') if target == 'zx' else directory, source]
            run(command)
            emitted = []
            public_dir = OUTPUT / case['id'] / target
            public_dir.mkdir(parents=True, exist_ok=True)
            for path in sorted(directory.rglob('*')):
                if path.suffix not in ({'.c', '.h'} if target == 'c' else {'.go'} if target == 'go' else {'.zx'}):
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
            else:
                invocation = [runner, base.SITE / emitted[0]['path'], case['expected'], samples]
            measured = platforms.summarize(platforms.read_measurement(platforms.run(invocation, env=env), row), case['expected'])
            cell['measurement'] = {**measured, 'command': base.command_text(invocation)}
            add_licenses(public_dir)
            cell['download'] = archive(public_dir, OUTPUT / case['id'] / (target + '.zip')) if target != 'zx' else emitted[0]
        data['examples'].append(row)
    assert data['compiler']['library_sha256'] == base.sha(base.REPO / 'build/libziran.a'), 'Runtime changed during measurement'
    (OUTPUT / 'matrix.json').write_text(json.dumps(data, indent=2) + '\n')
    render(data, write=True)
    print(f'{len(data["examples"])} library entries: C, Go and portable ZX executed; every batch verified')


def render(data, write=False):
    rows = []
    for row in data['examples']:
        cells = ''.join(f'<td><a class="library-cell" href="{html.escape(cell["download"]["path"])}" download>'
                        f'<span class="matrix-state is-ok">Verified</span><strong class="matrix-value">'
                        f'{base.format_time(cell["measurement"]["execution_ms"])}</strong></a></td>' for cell in row['targets'].values())
        rows.append(f'<tr><th scope="row">{html.escape(row["title"])}</th>{cells}</tr>')
    content = '<div class="matrix-scroll" tabindex="0" role="region" aria-label="Desktop widget comparison"><table class="target-matrix library-matrix"><caption class="sr-only">Desktop widget scenarios compiled to C, Go and portable ZX</caption><thead><tr><th scope="col">Two-frame scenario</th><th scope="col">C<small>cc -O2</small></th><th scope="col">Go<small>native</small></th><th scope="col">ZX<small>portable VM</small></th></tr></thead><tbody>' + ''.join(rows) + '</tbody></table></div>'
    page = base.SITE / 'targets.html'
    original = page.read_text()
    result = re.sub(r'(?s)(<!-- generated:' + MARKER + r' -->).*?(<!-- /generated:' + MARKER + r' -->)', lambda m: m[1] + '\n' + content + '\n' + m[2], original)
    assert result != original or content in original, 'Missing library matrix template'
    if write:
        page.write_text(result)
    return result


def check():
    data = json.loads((OUTPUT / 'matrix.json').read_text())
    assert data['schema_version'] == 1 and data['samples'] >= 3
    for name, digest in data['harnesses'].items():
        assert base.sha(base.REPO / name) == digest, f'Stale library harness: {name}'
    files = [data['sources'], data['checked_ir']]
    for row in data['examples']:
        assert set(row['targets']) == {'c', 'go', 'zx'}
        for cell in row['targets'].values():
            files += cell['files'] + [cell['download']]
            measured = cell['measurement']
            assert measured['stdout_verified'] and measured['result'] == row['expected']
            assert len(measured['batch_samples_ms']) == data['samples']
            assert platforms.summarize(measured, row['expected']) == measured
    for file in files:
        path = base.SITE / file['path']
        assert path.stat().st_size == file['bytes'] and base.sha(path) == file['sha256'], f'Stale library output: {path}'
    assert render(data) == (base.SITE / 'targets.html').read_text()
    print('Library output hashes, raw samples, checksums and generated table verified')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--project', type=Path)
    parser.add_argument('--manifest', type=Path)
    parser.add_argument('--samples', type=int, default=9)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    if args.check:
        check()
    else:
        if not args.project or not args.manifest or not 3 <= args.samples <= 31:
            parser.error('Pass --project, --manifest and 3–31 samples')
        build(args.project.resolve(), args.manifest.resolve(), args.samples)
