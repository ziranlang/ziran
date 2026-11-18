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
from benchmark_samples import summarize_samples

REPO = Path(__file__).resolve().parents[1]
SITE = REPO / 'site'
OUTPUT = SITE / 'generated' / 'targets'
WORK = REPO / 'build' / 'site-targets'
TARGETS = {'c': 'C', 'go': 'Go', 'zx': 'ZX', 'native': 'Native ELF',
           'wasm': 'WebAssembly', 'zir': 'Checked IR'}


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def committed_sha(revision, paths):
    """Hash each path as the measured revision commits it, so later edits do
    not invalidate an older receipt."""
    paths = list(paths)
    query = ''.join(f'{revision}:{path}\n' for path in paths).encode()
    output = subprocess.run(['git', 'cat-file', '--batch'], cwd=REPO, input=query,
                            capture_output=True, check=True).stdout
    digests, at = {}, 0
    for path in paths:
        end = output.index(b'\n', at)
        header = output[at:end].split()
        assert header[-1] != b'missing', f'Measured revision {revision} has no {path}; fetch it'
        size = int(header[2])
        digests[path] = hashlib.sha256(output[end + 1:end + 1 + size]).hexdigest()
        at = end + 2 + size
    return digests


def published_revision(receipt):
    """The commit that published a receipt, or None while it is uncommitted."""
    try:
        path = receipt.resolve().relative_to(REPO)
    except ValueError:
        return None
    if git('status', '--porcelain', '--', path):
        return None
    return git('log', '-1', '--format=%H', '--', path) or None


def check_committed(revision, recorded, label):
    """Compiler inputs are checked at the measured revision, harnesses at the
    commit that published their receipt; None checks the working tree."""
    committed = committed_sha(revision, recorded) if revision else {path: sha(REPO / path) for path in recorded}
    for path, digest in recorded.items():
        assert committed[path] == digest, f'{label} differs from {revision or "the working tree"}: {path}'


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


OUTPUT_ORACLES = {
    'arrays': 'empty: 0 0 0 0 0\nset: 100, count: 5\nmiddle has 3 items starting with 2\nsum of all: 110\nsum of middle: 9\n',
    'enums': 'a circle has 0 corners\na square has 4 corners\na triangle has 3 corners\n',
    'loops': 'while 1\nwhile 2\nwhile 3\nrange 0\nrange 1\nrange 2\napple is at 0\nbanana is at 1\ncherry is at 2\nodd 1\nodd 3\nodd 5\nodd 7\n',
}


def fixtures():
    return [
        ('score', 'Records & calls', 'examples/score.zi', 42),
        ('text_demo', 'Text & imports', 'examples/text_demo.zi', 1),
        ('arrays', 'Arrays & slices', 'examples/arrays.zi', 0),
        ('enums', 'Enums & branches', 'examples/enums.zi', 0),
        ('loops', 'Loops & collections', 'examples/loops.zi', 0),
    ]


def store(source, destination):
    target = OUTPUT / destination
    target.parent.mkdir(parents=True, exist_ok=True)
    data = source.read_bytes()
    if not target.exists() or target.read_bytes() != data:
        target.write_bytes(data)
    return {'path': target.relative_to(SITE).as_posix(), 'bytes': len(data), 'sha256': sha(target)}


def capture_files(directory, name, target):
    extensions = {'c': {'.c', '.h'}, 'go': {'.go'}, 'zir': {'.zir'}, 'zx': {'.zx'},
                  'native': {'.native'}, 'wasm': {'.wasm'}}
    return [store(path, Path(name) / target / path.relative_to(directory))
            for path in sorted(directory.rglob('*'), key=lambda path: (path.stem != name, path.suffix, path.as_posix()))
            if path.suffix in extensions[target] and not path.name.startswith('matrix_host.')]


def command_text(command):
    return shlex.join(str(x.relative_to(REPO)) if isinstance(x, Path) and x.is_relative_to(REPO)
                      else str(x) for x in command)


def build(bin_dir, samples, render_page=True):
    OUTPUT.mkdir(parents=True, exist_ok=True)
    WORK.mkdir(parents=True, exist_ok=True)
    environment = dict(os.environ, GO111MODULE='off', GOMAXPROCS='2')
    for key in ('DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'DBUS_SESSION_BUS_ADDRESS', 'ZIRAN_PROFILE'):
        environment.pop(key, None)
    environment['YUE_DESKTOP_RECOVERY'] = '0'
    environment['ZIRAN_LEARN'] = 'off'
    rows, runs = [], []
    for name, title, source_path, expected in fixtures():
        source = REPO / source_path
        row = {'id': name, 'title': title, 'source': source_path.removeprefix('site/'),
               'source_sha256': sha(source), 'expected': str(expected), 'entry': 'main' if name in OUTPUT_ORACLES else 'Answer',
               'stdout': OUTPUT_ORACLES.get(name, ''), 'targets': {}}
        root = source.parent.relative_to(REPO).as_posix()
        inputs = ['--root', root, '--module-path', 'std', '--entry', f'{name}:{row["entry"]}']
        for target in TARGETS:
            directory = WORK / name / target
            directory.mkdir(parents=True, exist_ok=True)
            tool = bin_dir / ('zi2zx' if target in ('native', 'wasm') else 'zi2' + target)
            options = [] if target in ('zx', 'native', 'zir') else ['--no-main']
            if target == 'wasm':
                options = ['--target', 'wasm']
            if target == 'go':
                options += ['--pkg', 'main']
            output = directory / (f'{name}.{target}' if target in ('zx', 'native', 'wasm') else '')
            verified = directory / f'{name}.zx'
            lower_output = verified if target in ('native', 'wasm') else output
            lower = [tool, *options, *inputs, '-o', lower_output, source.relative_to(REPO)]
            commands = [lower]
            if target == 'wasm':
                commands.append([bin_dir / 'zx2wasm', '-o', output, verified])
            elif target == 'native':
                commands.append([bin_dir / 'zx2native', '-o', output, verified])
            elapsed = 0.0
            result = None
            for command in commands:
                result, duration = execute(command, env=environment, allow_failure=True)
                elapsed += duration
                if result.returncode:
                    break
            cell = {'command': ' && '.join(command_text(command) for command in commands),
                    'files': [], 'emission_samples_ms': []}
            row['targets'][target] = cell
            if result.returncode:
                diagnostic = result.stderr.strip() or result.stdout.strip()
                raise RuntimeError(diagnostic)
            cell['status'] = 'ok'
            cell['files'] = capture_files(directory, name, target)
            cell['bytes'] = sum(file['bytes'] for file in cell['files'])
            # Discarded warmup above. Emission excludes downstream C/Go compilation.
            for _ in range(samples):
                elapsed = 0.0
                for command in commands:
                    _, duration = execute(command, env=environment)
                    elapsed += duration
                cell['emission_samples_ms'].append(elapsed)
            cell['emission_statistics'] = summarize_samples(cell['emission_samples_ms'])
            cell['emission_ms'] = cell['emission_statistics']['median_ms']
            if target in ('zir', 'zx'):
                for file in cell['files']:
                    relative = Path(file['path']).relative_to(Path('generated/targets') / name / target)
                    artifact = directory / relative
                    # Checked IR lists its declarations; executable ZX lists its instructions.
                    inspect = ([bin_dir / 'zi-inspect', artifact.relative_to(REPO)] if target == 'zir' else
                               [bin_dir / 'zi2zx', '--emit', 'lir', '-o', '-', artifact.relative_to(REPO)])
                    listing = execute(inspect, env=environment)[0].stdout
                    listing_path = artifact.with_suffix(artifact.suffix + '.txt')
                    listing_path.write_text(listing)
                    file['inspection'] = store(listing_path, Path(name) / target / relative.with_suffix(relative.suffix + '.txt'))
                cell['inspection'] = cell['files'][0]['inspection']
            if target in ('zir', 'native'):
                continue
            run_cwd = REPO
            if target == 'c':
                header = next(directory.rglob(name + '.h'))
                symbol = re.search(r'\b(\w*' + row['entry'] + r')\(void\)', header.read_text()).group(1)
                host = directory / 'matrix_host.c'
                call = f'{symbol}();' if row['stdout'] else f'printf("%lld\\n", (long long){symbol}());'
                host.write_text(f'#include <stdio.h>\n#include <stdint.h>\n#include <stdbool.h>\n#include "{header.relative_to(directory)}"\n'
                                f'int main(void) {{ {call} return 0; }}\n')
                compile_command = [os.environ.get('CC', 'cc'), '-std=c11', '-O2', '-I' + str(directory),
                                   *sorted(directory.rglob('*.c')), '-lm', '-o', directory / 'program']
                execute(compile_command, env=environment)
                runner = [directory / 'program']
            elif target == 'go':
                generated = next(directory.rglob(name + '.go')).read_text()
                symbol = re.search(r'func (\w+_' + row['entry'].capitalize() + r')\(', generated).group(1)
                body = f'{symbol}()' if row['stdout'] else f'fmt.Println({symbol}())'
                imports = '' if row['stdout'] else 'import "fmt"\n'
                (directory / 'matrix_host.go').write_text(f'package main\n{imports}func main() {{ {body} }}\n')
                compile_command = ['go', 'build', '-p=2', '-o', directory / 'program', *sorted(directory.rglob('*.go'))]
                execute(compile_command, env=environment)
                runner = [directory / 'program']
            elif target == 'zx':
                runner = [bin_dir / 'zi2zx', 'run', output]
            elif target == 'native':
                runner = [output]
            else:
                runner = ['node', REPO / 'scripts/site_target_wasm.cjs', output, *(['--output'] if row['stdout'] else [])]
            cell['run_command'] = command_text(runner)
            if target in ('c', 'go'):
                cell['build_command'] = command_text(compile_command)
            cell['run_samples_ms'] = []
            result, _ = execute(runner, env=environment, cwd=run_cwd, allow_failure=target == 'native')
            observed = result.stdout.strip()
            wanted = row['stdout'].strip() if row['stdout'] else str(expected)
            if target == 'native':
                native_output = row['stdout'].strip()
                if observed != native_output or result.returncode != expected % 256:
                    raise RuntimeError(f'{name}/native: output {observed!r}, status {result.returncode}; '
                                       f'expected {native_output!r}, status {expected % 256}')
                observed = wanted
            elif observed != wanted:
                raise RuntimeError(f'{name}/{target}: {observed!r} != oracle {expected}')
            cell['result'] = observed
            runs.append((name, target, runner, run_cwd, row['stdout'].strip() if row['stdout'] else str(expected), cell))
        rows.append(row)
    randomizer = random.Random(42)
    for _ in range(samples):
        randomizer.shuffle(runs)
        for name, target, runner, cwd, expected, cell in runs:
            result, elapsed = execute(runner, env=environment, cwd=cwd, allow_failure=target == 'native')
            if target == 'native':
                example = next(row for row in rows if row['id'] == name)
                valid = result.stdout.strip() == example['stdout'].strip() and result.returncode == int(example['expected']) % 256
            else:
                valid = result.stdout.strip() == str(expected)
            if not valid:
                raise RuntimeError(f'{name}/{target}: measured result differs from oracle')
            cell['run_samples_ms'].append(elapsed)
    for _, _, _, _, _, cell in runs:
        cell['run_statistics'] = summarize_samples(cell['run_samples_ms'])
        cell['run_ms'] = cell['run_statistics']['median_ms']
    tools = {name: {'sha256': sha(bin_dir / name)}
             for name in ('zi2c', 'zi2go', 'zi2zx', 'zx2native', 'zx2wasm', 'zi2zir', 'zi-inspect')}
    versions = {name: execute(command)[0].stdout.splitlines()[0]
                for name, command in {'cc': ['cc', '--version'], 'go': ['go', 'version'], 'node': ['node', '--version']}.items()}
    import site_target_platforms as hosts
    assert platform.node() == 'alpha', 'Publish measurements from the selected physical host'
    assert execute(['systemd-detect-virt'], allow_failure=True)[0].stdout.strip() == 'none'
    platforms = {'desktop': hosts.desktop({'examples': rows}, samples, host_gate=True)}
    platform_harnesses = {name: sha(REPO / name) for name in [
        'scripts/site_target_platforms.py', 'scripts/site_target_node.cjs',
        'bench/site_target_host.c', 'bench/site_target_host.go', 'site/target-measure.js']}
    data = {'schema_version': 2, 'platforms': platforms, 'generated_at': datetime.now(timezone.utc).isoformat(),
            'revision': git('rev-parse', 'HEAD'), 'compiler_dirty': bool(git('status', '--porcelain', '--', 'cmd', 'std', 'include', 'bootstrap', 'Makefile')),
            'platform': platform.system() + ' ' + platform.machine(), 'cpu': cpu_name(),
            'samples': samples, 'warmups': 1, 'tools': tools, 'versions': versions,
            'source_inputs': {path: sha(REPO / path) for path in [*[item[2] for item in fixtures()], 'std/text.zi']},
            'harnesses': {path: sha(REPO / path) for path in ['scripts/site_targets.py', 'scripts/benchmark_samples.py', 'scripts/site_target_wasm.cjs', 'site/target-abi.js']},
            'methodology': 'Median whole-process wall time after one discarded warmup; shuffled execution order; symmetric 1.5×IQR outliers excluded, raw samples retained. '
                           'C: cc -O2; Go: go build; ZX: the instruction engine (zi2zx run); Native: zi2zx then zx2native; WebAssembly: zi2zx then zx2wasm, run by Node including engine startup, compilation and instantiation. '
                           'Emission includes loading, checking, linking and writing output; it excludes downstream C/Go builds. '
                           'Output size is emitted files, including C headers, rather than linked executables. '
                           'Shared host, warm filesystem caches, no CPU isolation. Small examples emphasize startup; these are not language rankings.',
            'examples': rows}
    if platform_harnesses:
        data['platform_harnesses'] = platform_harnesses
    (OUTPUT / 'matrix.json').write_text(json.dumps(data, indent=2) + '\n')
    if render_page:
        (SITE / 'targets.html').write_text(render(data))
    print(f'Target matrix: {len(rows)} examples; {len(runs)} verified execution cells; {samples} samples each')


def cpu_name():
    try:
        return next(line.split(':', 1)[1].strip() for line in Path('/proc/cpuinfo').read_text().splitlines() if line.startswith('model name'))
    except (OSError, StopIteration):
        return platform.processor() or platform.machine()


def cell_texts(data):
    # Every displayed time and size, formatted once here; targets.js only shows them.
    runs = data['platforms'].get('desktop', {}).get('examples', {})
    measured = {(example['id'], target): runs.get(example['id'], {}).get(target, {})
                for example in data['examples'] for target in example['targets']}
    units = {'run': 'µs',
             'emission': time_unit([cell.get('emission_ms') for example in data['examples']
                                    for cell in example['targets'].values()], ('ns', 'µs', 'ms'))}
    cells = {}
    for example in data['examples']:
        for target, cell in example['targets'].items():
            run = measured[example['id'], target]
            included = run.get('sample_statistics', {}).get('included_samples_ms')
            cells.setdefault(example['id'], {})[target] = {
                'run': format_time(run.get('execution_ms'), units['run']),
                'emission': format_time(cell.get('emission_ms'), units['emission']),
                'size': '—' if cell.get('bytes') is None else f'{cell["bytes"]:,}',
                'range': format_time(min(included), units['run']) + '–' + format_time(max(included), units['run'])
                if included else ''}
    return {'units': units, 'cells': cells}


def render(data):
    escape = html.escape
    texts = cell_texts(data)
    rows = []
    for example in data['examples']:
        cells = []
        for target, cell in example['targets'].items():
            if target in ('zir', 'native'):
                continue
            supported = cell['status'] == 'ok'
            href = (cell.get('inspection') or (cell['files'][0] if supported else cell['diagnostic_file']))['path']
            label = ('Shared stage' if target == 'zir' else 'Verified') if supported else 'Unavailable'
            value = texts['cells'][example['id']][target]['run']
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
    source = re.sub(r'<small id="matrix-unit">[^<]*</small>',
                    lambda match: '<small id="matrix-unit">' + texts['units']['run'] + ' per call</small>', source)
    payload = json.dumps(texts, ensure_ascii=False, separators=(',', ':')).replace('</', '<\\/')
    return re.sub(r'(?s)(<!-- generated:target_texts -->).*?(<!-- /generated:target_texts -->)',
                  lambda match: match[1] + '<script type="application/json" id="matrix-texts">' + payload + '</script>' + match[2],
                  source)


# One number format for every benchmark table: three significant digits,
# thousands separators, never scientific notation. scripts/site_numbers.zi
# implements the same steps for the Ziran renderer; both pass NUMBER_CASES.
UNIT_SCALE = {'ns': 1e6, 'µs': 1e3, 'ms': 1.0}
NUMBER_CASES = ((0.0011370088958740235, '0.00114'), (1.1370088958740235, '1.14'), (4.881, '4.88'),
                (4.8, '4.8'), (56.63, '56.6'), (100.0, '100'), (999.6, '1,000'), (9.996, '10'),
                (1147.849, '1,150'), (8528.05, '8,530'), (37783425.4, '37,800,000'), (0.0, '0'))


def format_number(value):
    if value is None:
        return '—'
    if value <= 0:
        return '0'
    exponent, scale = 0, 1.0
    while value >= scale * 10:
        scale *= 10
        exponent += 1
    while value < scale:
        scale /= 10
        exponent -= 1
    decimals = 2 - exponent
    power = float(10 ** abs(decimals))
    digits = int((value * power if decimals >= 0 else value / power) + 0.5)
    if digits >= 1000:  # rounding carried into a fourth digit, as 999.6 does
        digits //= 10
        decimals -= 1
    whole, fraction = (digits // 10 ** decimals, str(digits % 10 ** decimals).rjust(decimals, '0').rstrip('0')) \
        if decimals > 0 else (digits * 10 ** -decimals, '')
    return f'{whole:,}' + ('.' + fraction if fraction else '')


def time_unit(values_ms, units=('ns', 'µs')):
    # The unit that puts the most cells between 1 and 9,999; ties keep the larger unit.
    measured = [value for value in values_ms if value is not None]
    order = sorted(units, key=lambda unit: UNIT_SCALE[unit])
    return max(order, key=lambda unit: sum(1 <= value * UNIT_SCALE[unit] < 9999.5 for value in measured))


def format_time(value_ms, unit):
    return '—' if value_ms is None else format_number(value_ms * UNIT_SCALE[unit])


def format_microseconds(value_ms):
    return format_time(value_ms, 'µs')


def check():
    data = json.loads((OUTPUT / 'matrix.json').read_text())
    expected = {name: (path.removeprefix('site/'), str(result)) for name, _, path, result in fixtures()}
    assert data['schema_version'] == 2 and data['samples'] >= 3 and data['warmups'] == 1
    assert set(data['platforms']) == {'desktop'}, 'Publish one measured host'
    assert {example['id'] for example in data['examples']} == set(expected)
    for group in ('harnesses', 'platform_harnesses'):
        assert data.get(group), f'Missing {group}'
        check_committed(published_revision(OUTPUT / 'matrix.json'), data[group], 'Harness')
    for snapshot in data['platforms'].values():
        assert 3 <= snapshot['samples'] <= 31
        assert set(snapshot['examples']) == set(expected)
        assert snapshot['host'] and snapshot['measured_at'] and snapshot['methodology']
    check_committed(data['revision'], data.get('source_inputs', {}), 'Input')
    for example in data['examples']:
        assert (example['source'], example['expected']) == expected[example['id']], 'Fixture oracle changed; regenerate the snapshot'
        assert set(example['targets']) == set(TARGETS)
        assert sha(SITE / example['source']) == example['source_sha256'], f'Stale source: {example["id"]}'
        for cell in example['targets'].values():
            assert cell['status'] == 'ok', 'Every matrix example must support all four targets'
            for file in cell['files'] + [cell[key] for key in ('inspection', 'diagnostic_file') if key in cell]:
                path = SITE / file['path']
                assert sha(path) == file['sha256'] and path.stat().st_size == file['bytes'], f'Stale output: {path}'
                if 'inspection' in file:
                    listing = file['inspection']
                    assert sha(SITE / listing['path']) == listing['sha256']
            if cell['status'] == 'ok' and 'run_samples_ms' in cell:
                assert len(cell['run_samples_ms']) == data['samples'] and cell['result'] == (example['stdout'].strip() if example['stdout'] else example['expected'])
                assert all(math.isfinite(value) and value > 0 for value in cell['run_samples_ms'])
                assert summarize_samples(cell['run_samples_ms']) == cell['run_statistics']
                assert cell['run_statistics']['median_ms'] == cell['run_ms']
            if cell['status'] == 'ok':
                assert len(cell['emission_samples_ms']) == data['samples']
                assert all(math.isfinite(value) and value > 0 for value in cell['emission_samples_ms'])
                assert summarize_samples(cell['emission_samples_ms']) == cell['emission_statistics']
                assert cell['emission_statistics']['median_ms'] == cell['emission_ms']
                assert cell['bytes'] == sum(file['bytes'] for file in cell['files'])
            for file in cell.get('web', {}).get('files', []):
                path = SITE / file['path']
                assert sha(path) == file['sha256'] and path.stat().st_size == file['bytes'], f'Stale web artifact: {path}'
        for platform in data['platforms'].values():
            row = platform['examples'][example['id']]
            assert set(row) == {'c', 'go', 'zx', 'wasm'}
            for measured in row.values():
                assert measured['result'] == example['expected'] and measured['stdout_verified']
                assert measured['batches'] > 0 and len(measured['batch_samples_ms']) == platform['samples']
                assert all(math.isfinite(value) and value > 0 for value in measured['batch_samples_ms'])
                assert measured['execution_samples_ms'] == [value / measured['batches'] for value in measured['batch_samples_ms']]
                assert measured['sample_statistics'] == summarize_samples(measured['execution_samples_ms'])
                assert measured['execution_ms'] == measured['sample_statistics']['median_ms']
                assert measured['checksum'] == str(int(example['expected']) * measured['batches'])
    original = (SITE / 'targets.html').read_text()
    assert render(data) == original, 'Regenerate targets.html from its snapshot'
    print('Target matrix source hashes, output hashes, sizes, sample counts and generated HTML verified')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bin-dir', type=Path, default=REPO / 'build/bin')
    parser.add_argument('--samples', type=int, default=5)
    parser.add_argument('--check', action='store_true')
    parser.add_argument('--render', action='store_true', help='rewrite the page from the saved matrix without measuring')
    arguments = parser.parse_args()
    if arguments.samples < 3:
        parser.error('--samples must be at least 3')
    try:
        if arguments.render:
            (SITE / 'targets.html').write_text(render(json.loads((OUTPUT / 'matrix.json').read_text())))
        else:
            check() if arguments.check else build(arguments.bin_dir.resolve(), arguments.samples)
    except (RuntimeError, AssertionError) as error:
        sys.exit(str(error))
