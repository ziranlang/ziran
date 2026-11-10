#!/usr/bin/env python3
"""Build the website's real output matrix and a reproducible timing snapshot.

Every runnable cell is checked against an independent expected result. Timings
are whole processes, including startup (and Node's Wasm compilation), with one
discarded warmup and shuffled measured rounds. ZIR is an intermediate stage.
Use --check to verify published artifact/source hashes without rebenchmarking.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import html
import json
import math
import os
from pathlib import Path
import platform
import random
import re
import shlex
import statistics
import subprocess
import sys
import time

REPO = Path(__file__).resolve().parents[1]
SITE = REPO / 'site'
OUTPUT = SITE / 'generated' / 'targets'
WORK = REPO / 'build' / 'site-targets'
TARGETS = {'c': 'C', 'go': 'Go', 'zx': 'ZX', 'wasm': 'WebAssembly', 'zir': 'Checked IR'}


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def execute(command, *, cwd=REPO, env=None, allow_failure=False):
    started = time.perf_counter_ns()
    result = subprocess.run([str(x) for x in command], cwd=cwd, env=env,
                            capture_output=True, text=True, timeout=120)
    elapsed = (time.perf_counter_ns() - started) / 1e6
    if result.returncode and not allow_failure:
        raise RuntimeError(f'{shlex.join(map(str, command))}\n{result.stdout}{result.stderr}')
    return result, elapsed


def git(*arguments):
    return execute(['git', *arguments])[0].stdout.strip()


def fixtures():
    value = 7
    for i in range(2000):
        value = (value * 17 + i) % 1000003
    points = [[i, i * 2] for i in range(16)]
    for round_ in range(100):
        for i, point in enumerate(points):
            point[0] = (point[0] + point[1] + round_) % 1009
            point[1] = (point[1] + i + 1) % 1013
    return [
        ('score', 'Records & calls', 'site/examples/score.zi', 42),
        ('text_demo', 'Text & imports', 'site/examples/text_demo.zi', 1),
        ('loops', 'Integer recurrence', 'site/examples/targets/loops.zi', value),
        ('records', 'Record arrays', 'site/examples/targets/records.zi', sum(map(sum, points))),
        ('enums', 'Enum branches', 'site/examples/targets/enums.zi', 60),
    ]


def store(source, destination):
    target = OUTPUT / destination
    target.parent.mkdir(parents=True, exist_ok=True)
    data = source.read_bytes()
    if not target.exists() or target.read_bytes() != data:
        target.write_bytes(data)
    return {'path': target.relative_to(SITE).as_posix(), 'bytes': len(data), 'sha256': sha(target)}


def capture_files(directory, name, target):
    extensions = {'c': {'.c', '.h'}, 'go': {'.go'}, 'zir': {'.zir'}, 'zx': {'.zx'}, 'wasm': {'.wasm'}}
    return [store(path, Path(name) / target / path.relative_to(directory))
            for path in sorted(directory.rglob('*'), key=lambda path: (path.stem != name, path.suffix, path.as_posix()))
            if path.suffix in extensions[target] and not path.name.startswith('matrix_host.')]


def command_text(command):
    return shlex.join(str(x.relative_to(REPO)) if isinstance(x, Path) and x.is_relative_to(REPO)
                      else str(x) for x in command)


def build(bin_dir, samples):
    OUTPUT.mkdir(parents=True, exist_ok=True)
    WORK.mkdir(parents=True, exist_ok=True)
    environment = dict(os.environ, GO111MODULE='off', GOMAXPROCS='2')
    for key in ('DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'DBUS_SESSION_BUS_ADDRESS', 'ZIRAN_PROFILE'):
        environment.pop(key, None)
    environment['YUE_DESKTOP_RECOVERY'] = '0'
    rows, runs = [], []
    for name, title, source_path, expected in fixtures():
        source = REPO / source_path
        row = {'id': name, 'title': title, 'source': source_path.removeprefix('site/'),
               'source_sha256': sha(source), 'expected': str(expected), 'targets': {}}
        root = source.parent.relative_to(REPO).as_posix()
        inputs = ['--root', root, '--module-path', 'std', '--entry', f'{name}:Answer']
        for target in TARGETS:
            directory = WORK / name / target
            directory.mkdir(parents=True, exist_ok=True)
            tool = bin_dir / ('zi2' + target)
            options = [] if target in ('zx', 'wasm', 'zir') else ['--no-main']
            if target == 'go':
                options += ['--pkg', 'main']
            if target == 'zx':
                options = ['bundle']
            output = directory / (f'{name}.{target}' if target in ('zx', 'wasm') else '')
            command = [tool, *options, *inputs, '-o', output, source.relative_to(REPO)]
            result, _ = execute(command, env=environment, allow_failure=True)
            cell = {'command': command_text(command), 'files': [], 'emission_samples_ms': []}
            row['targets'][target] = cell
            if result.returncode:
                diagnostic = result.stderr.strip() or result.stdout.strip()
                # Only documented Wasm subset errors are expected here. Other failures
                # must stop publication rather than masquerading as a capability limit.
                if target != 'wasm' or not any(message in diagnostic for message in
                    ('zx.wasm', 'outside the wasm execution subset')):
                    raise RuntimeError(diagnostic)
                cell.update(status='unsupported', diagnostic=diagnostic)
                diagnostic_path = directory / 'diagnostic.txt'
                diagnostic_path.write_text(diagnostic + '\n')
                cell['diagnostic_file'] = store(diagnostic_path, Path(name) / target / 'diagnostic.txt')
                continue
            cell['status'] = 'ok'
            cell['files'] = capture_files(directory, name, target)
            cell['bytes'] = sum(file['bytes'] for file in cell['files'])
            # Discarded warmup above. Emission excludes downstream C/Go compilation.
            for _ in range(samples):
                _, elapsed = execute(command, env=environment)
                cell['emission_samples_ms'].append(elapsed)
            cell['emission_ms'] = statistics.median(cell['emission_samples_ms'])
            if target in ('zir', 'zx'):
                for file in cell['files']:
                    relative = Path(file['path']).relative_to(Path('generated/targets') / name / target)
                    artifact = directory / relative
                    listing = execute([bin_dir / 'zi-inspect', artifact.relative_to(REPO)], env=environment)[0].stdout
                    listing_path = artifact.with_suffix(artifact.suffix + '.txt')
                    listing_path.write_text(listing)
                    file['inspection'] = store(listing_path, Path(name) / target / relative.with_suffix(relative.suffix + '.txt'))
                cell['inspection'] = cell['files'][0]['inspection']
            if target == 'zir':
                continue
            run_cwd = REPO
            if target == 'c':
                header = next(directory.rglob(name + '.h'))
                symbol = re.search(r'\b(\w*Answer)\(void\)', header.read_text()).group(1)
                host = directory / 'matrix_host.c'
                host.write_text(f'#include <stdio.h>\n#include <stdint.h>\n#include <stdbool.h>\n#include "{header.relative_to(directory)}"\n'
                                f'int main(void) {{ printf("%lld\\n", (long long){symbol}()); return 0; }}\n')
                compile_command = [os.environ.get('CC', 'cc'), '-std=c11', '-O2', '-I' + str(directory),
                                   *sorted(directory.rglob('*.c')), '-lm', '-o', directory / 'program']
                execute(compile_command, env=environment)
                runner = [directory / 'program']
            elif target == 'go':
                generated = next(directory.rglob(name + '.go')).read_text()
                symbol = re.search(r'func (\w+_Answer)\(', generated).group(1)
                (directory / 'matrix_host.go').write_text(f'package main\nimport "fmt"\nfunc main() {{ fmt.Println({symbol}()) }}\n')
                compile_command = ['go', 'build', '-p=2', '-o', directory / 'program', *sorted(directory.rglob('*.go'))]
                execute(compile_command, env=environment)
                runner = [directory / 'program']
            elif target == 'zx':
                runner = [bin_dir / 'zi2zx', 'run', '--vm', output]
            else:
                runner = ['node', REPO / 'scripts/site_target_wasm.cjs', output]
            cell['run_command'] = command_text(runner)
            if target in ('c', 'go'):
                cell['build_command'] = command_text(compile_command)
            cell['run_samples_ms'] = []
            observed = execute(runner, env=environment, cwd=run_cwd)[0].stdout.strip()
            if observed != str(expected):
                raise RuntimeError(f'{name}/{target}: {observed!r} != oracle {expected}')
            cell['result'] = observed
            runs.append((name, target, runner, run_cwd, expected, cell))
        rows.append(row)
    randomizer = random.Random(42)
    for _ in range(samples):
        randomizer.shuffle(runs)
        for name, target, runner, cwd, expected, cell in runs:
            result, elapsed = execute(runner, env=environment, cwd=cwd)
            if result.stdout.strip() != str(expected):
                raise RuntimeError(f'{name}/{target}: measured result differs from oracle')
            cell['run_samples_ms'].append(elapsed)
    for _, _, _, _, _, cell in runs:
        cell['run_ms'] = statistics.median(cell['run_samples_ms'])
    tools = {name: {'sha256': sha(bin_dir / name)}
             for name in ('zi2c', 'zi2go', 'zi2zx', 'zi2wasm', 'zi2zir', 'zi-inspect')}
    versions = {name: execute(command)[0].stdout.splitlines()[0]
                for name, command in {'cc': ['cc', '--version'], 'go': ['go', 'version'], 'node': ['node', '--version']}.items()}
    data = {'schema_version': 1, 'generated_at': datetime.now(timezone.utc).isoformat(),
            'revision': git('rev-parse', 'HEAD'), 'compiler_dirty': bool(git('status', '--porcelain', '--', 'cmd', 'std', 'include', 'bootstrap', 'Makefile')),
            'platform': platform.system() + ' ' + platform.machine(), 'cpu': cpu_name(),
            'samples': samples, 'warmups': 1, 'tools': tools, 'versions': versions,
            'source_inputs': {path: sha(REPO / path) for path in [*[item[2] for item in fixtures()], 'std/text.zi']},
            'harnesses': {path: sha(REPO / path) for path in ['scripts/site_targets.py', 'scripts/site_target_wasm.cjs']},
            'methodology': 'Median whole-process wall time after one discarded warmup; shuffled execution order. '
                           'C: cc -O2; Go: go build; ZX: portable VM (--vm); WebAssembly: Node, including engine startup, compilation and instantiation. '
                           'Emission includes loading, checking, linking and writing output; it excludes downstream C/Go builds. '
                           'Output size is emitted files, including C headers, rather than linked executables. '
                           'Shared host, warm filesystem caches, no CPU isolation. Small examples emphasize startup; these are not language rankings.',
            'examples': rows}
    (OUTPUT / 'matrix.json').write_text(json.dumps(data, indent=2) + '\n')
    (SITE / 'targets.html').write_text(render(data))
    print(f'Target matrix: {len(rows)} examples; {len(runs)} verified execution cells; {samples} samples each')


def cpu_name():
    try:
        return next(line.split(':', 1)[1].strip() for line in Path('/proc/cpuinfo').read_text().splitlines() if line.startswith('model name'))
    except (OSError, StopIteration):
        return platform.processor() or platform.machine()


def render(data):
    escape = html.escape
    rows = []
    for example in data['examples']:
        cells = []
        for target, cell in example['targets'].items():
            supported = cell['status'] == 'ok'
            href = (cell.get('inspection') or (cell['files'][0] if supported else cell['diagnostic_file']))['path']
            label = ('Inspect' if target == 'zir' else 'Verified') if supported else 'Outside subset'
            value = '—' if not supported else f'{cell.get("run_ms", cell["emission_ms"]):.2f} ms'
            cells.append(f'<td><a href="{escape(href)}" class="matrix-cell" data-example="{example["id"]}" data-target="{target}" '
                         f'aria-label="{escape(example["title"])}: {TARGETS[target]}, {label}">'
                         f'<span class="matrix-state {"is-ok" if supported else "is-limited"}">{label}</span>'
                         f'<strong class="matrix-value">{value}</strong></a></td>')
        rows.append(f'<tr><th scope="row"><strong>{escape(example["title"])}</strong><small>{escape(example["id"])}.zi</small></th>{"".join(cells)}</tr>')
    first = data['examples'][0]
    content = (SITE / first['source']).read_text()
    fragment = '<!-- generated:target_matrix -->'
    end = '<!-- /generated:target_matrix -->'
    page = SITE / 'targets.html'
    source = page.read_text()
    start = source.index(fragment) + len(fragment)
    finish = source.index(end, start)
    source = source[:start] + '\n' + ''.join(rows) + '\n' + source[finish:]
    source = re.sub(r'(?s)(<!-- generated:target_source -->).*?(<!-- /generated:target_source -->)',
                    lambda match: match[1] + escape(content) + match[2], source)
    return source


def check():
    data = json.loads((OUTPUT / 'matrix.json').read_text())
    expected = {name: (path.removeprefix('site/'), str(result)) for name, _, path, result in fixtures()}
    assert data['schema_version'] == 1 and data['samples'] >= 3 and data['warmups'] == 1
    assert {example['id'] for example in data['examples']} == set(expected)
    for path, digest in data.get('source_inputs', {}).items():
        assert sha(REPO / path) == digest, f'Stale input: {path}'
    for example in data['examples']:
        assert (example['source'], example['expected']) == expected[example['id']], 'Fixture oracle changed; regenerate the snapshot'
        assert set(example['targets']) == set(TARGETS)
        assert sha(SITE / example['source']) == example['source_sha256'], f'Stale source: {example["id"]}'
        for cell in example['targets'].values():
            for file in cell['files'] + [cell[key] for key in ('inspection', 'diagnostic_file') if key in cell]:
                path = SITE / file['path']
                assert sha(path) == file['sha256'] and path.stat().st_size == file['bytes'], f'Stale output: {path}'
                if 'inspection' in file:
                    listing = file['inspection']
                    assert sha(SITE / listing['path']) == listing['sha256']
            if cell['status'] == 'ok' and 'run_samples_ms' in cell:
                assert len(cell['run_samples_ms']) == data['samples'] and cell['result'] == example['expected']
                assert all(math.isfinite(value) and value > 0 for value in cell['run_samples_ms'])
                assert statistics.median(cell['run_samples_ms']) == cell['run_ms']
            if cell['status'] == 'ok':
                assert len(cell['emission_samples_ms']) == data['samples']
                assert all(math.isfinite(value) and value > 0 for value in cell['emission_samples_ms'])
                assert statistics.median(cell['emission_samples_ms']) == cell['emission_ms']
                assert cell['bytes'] == sum(file['bytes'] for file in cell['files'])
    original = (SITE / 'targets.html').read_text()
    assert render(data) == original, 'Regenerate targets.html from its snapshot'
    print('Target matrix source hashes, output hashes, sizes, sample counts and generated HTML verified')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bin-dir', type=Path, default=REPO / 'build/bin')
    parser.add_argument('--samples', type=int, default=5)
    parser.add_argument('--check', action='store_true')
    arguments = parser.parse_args()
    if arguments.samples < 3:
        parser.error('--samples must be at least 3')
    try:
        check() if arguments.check else build(arguments.bin_dir.resolve(), arguments.samples)
    except (RuntimeError, AssertionError) as error:
        sys.exit(str(error))
