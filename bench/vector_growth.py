#!/usr/bin/env python3
"""Validated vector-growth benchmark across source, saved IR, and backends."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import random
import shutil
import statistics
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--out', type=Path)
parser.add_argument('--bin-dir', type=Path, default=ROOT / 'build/bin')
parser.add_argument('--compile-repetitions', type=int, default=3)
parser.add_argument('--run-repetitions', type=int, default=5)
parser.add_argument('--small', type=int, default=4000)
parser.add_argument('--large', type=int, default=2000000)
args = parser.parse_args()
if (min(args.compile_repetitions, args.run_repetitions, args.small) < 1 or
        args.large <= args.small or args.large > 50000000):
    parser.error('repetitions and small must be positive; large must exceed small and fit s32')

OUT = (args.out or ROOT / 'build/benchmarks' /
       ('collections-' + time.strftime('%Y%m%dT%H%M%SZ', time.gmtime()))).resolve()
if OUT.exists():
    parser.error(f'output directory already exists: {OUT}')
OUT.mkdir(parents=True)
FIX = OUT / 'fixtures'
FIX.mkdir()
BIN = args.bin_dir.resolve()
ENV = dict(os.environ)
ENV.pop('DISPLAY', None)
ENV.pop('WAYLAND_DISPLAY', None)
ENV['GO111MODULE'] = 'off'
ENV['GOCACHE'] = str(OUT / 'go-cache')
SAMPLES = []


def write(name, contents):
    path = FIX / name
    path.write_text(contents)
    return path


def expected(n):
    return 37 * n * (n - 1) // 2 + 11 * n


def invoke(command, phase, case, measured=True):
    argv = [str(item) for item in command]
    with tempfile.TemporaryFile() as stdout, tempfile.TemporaryFile() as stderr:
        started = time.perf_counter_ns()
        child = subprocess.Popen(argv, cwd=ROOT, env=ENV, stdout=stdout, stderr=stderr)
        _, status, usage = os.wait4(child.pid, 0)
        elapsed = (time.perf_counter_ns() - started) / 1e6
        stdout.seek(0)
        stderr.seek(0)
        output = stdout.read().decode(errors='replace')
        errors = stderr.read().decode(errors='replace')
    sample = dict(phase=phase, case=case, argv=argv, elapsed_ms=elapsed,
                  max_rss_kib=usage.ru_maxrss, user_s=usage.ru_utime,
                  system_s=usage.ru_stime, returncode=os.waitstatus_to_exitcode(status),
                  stdout=output, stderr=errors)
    if measured:
        SAMPLES.append(sample)
        with (OUT / 'samples.jsonl').open('a') as stream:
            stream.write(json.dumps(sample) + '\n')
    if sample['returncode']:
        raise RuntimeError(f'{case} failed ({sample["returncode"]}): {errors}')
    return output.strip()


def repeated(command, phase, case, count=None, result=None):
    count = args.compile_repetitions if count is None else count
    for index in range(count + 1):
        output = invoke(command, phase, case, measured=index != 0)
        if result is not None and output != str(result):
            raise AssertionError(f'{case}: {output!r} != {result}')
    print(f'{phase}: {case}', flush=True)


def source_hashes():
    paths = sorted(list(ROOT.glob('cmd/**/*')) + list(ROOT.glob('std/**/*')) +
                   list(ROOT.glob('include/**/*')) +
                   [ROOT / 'Makefile', Path(__file__).resolve()])
    return {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in paths if path.is_file()}


def versions():
    tools = {'gcc': ['gcc', '--version'], 'g++': ['g++', '--version'],
             'go': ['go', 'version'], 'rustc': ['rustc', '--version'],
             'javac': ['javac', '-version'], 'java': ['java', '-version'],
             'node': ['node', '--version'], 'python': ['python3', '--version']}
    result = {}
    for name, command in tools.items():
        if not shutil.which(command[0]):
            result[name] = None
        else:
            run = subprocess.run(command, cwd=ROOT, env=ENV,
                                 capture_output=True, text=True, check=False)
            result[name] = (run.stdout + run.stderr).strip()
    return result


def cpu_name():
    for line in Path('/proc/cpuinfo').read_text().splitlines():
        if line.startswith('model name'):
            return line.partition(':')[2].strip()
    return platform.processor()


ZI = write('collection.zi', '''#import "vec"
#program_export
Kernel :: (n: s32) -> s64 {
    values: Vec(s32)
    index: s32 = 0
    while index < n {
        if !VecPush(values, index * 37 + 11) { return -1 }
        index += 1
    }
    total: s64 = 0
    cursor: s64 = 0
    while cursor < values.count {
        total += cast(s64)values[cursor]
        cursor += 1
    }
    return total
}
#program_export
Answer :: () -> s64 { return Kernel(SMALL) }
'''.replace('Kernel(SMALL)', f'Kernel({args.small})'))
C = write('hand.c', '''#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
int main(int argc, char **argv) {
    if (argc != 2) return 2;
    int32_t n = (int32_t)strtol(argv[1], 0, 10), count = 0, capacity = 0;
    int32_t *values = NULL;
    for (int32_t i = 0; i < n; i++) {
        if (count == capacity) {
            capacity = capacity ? capacity * 2 : 8;
            void *grown = realloc(values, (size_t)capacity * sizeof(*values));
            if (!grown) return 3;
            values = grown;
        }
        values[count++] = i * 37 + 11;
    }
    int64_t total = 0;
    for (int32_t i = 0; i < count; i++) total += values[i];
    free(values);
    printf("%lld\\n", (long long)total);
    return 0;
}
''')
CPP = write('hand.cpp', '''#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>
int main(int argc, char **argv) {
    if (argc != 2) return 2;
    int32_t n = (int32_t)std::strtol(argv[1], nullptr, 10);
    std::vector<int32_t> values;
    for (int32_t i = 0; i < n; i++) values.push_back(i * 37 + 11);
    int64_t total = 0;
    for (int32_t value : values) total += value;
    std::printf("%lld\\n", (long long)total);
    return 0;
}
''')
GO = write('hand.go', '''package main
import ("fmt"; "os"; "strconv")
func main() {
    n,e := strconv.Atoi(os.Args[1]); if e != nil { panic(e) }
    values := make([]int32, 0)
    for i:=0; i<n; i++ { values = append(values, int32(i*37+11)) }
    var total int64
    for _,value := range values { total += int64(value) }
    fmt.Println(total)
}
''')
RUST = write('hand.rs', '''fn main() {
    let n: i32 = std::env::args().nth(1).unwrap().parse().unwrap();
    let mut values: Vec<i32> = Vec::new();
    for i in 0..n { values.push(i * 37 + 11); }
    let mut total: i64 = 0;
    for value in values { total += i64::from(value); }
    println!("{}", total);
}
''')
JAVA = write('CollectionGrowth.java', '''class CollectionGrowth {
    public static void main(String[] args) {
        int n = Integer.parseInt(args[0]);
        int[] values = new int[0];
        int count = 0;
        for (int i = 0; i < n; i++) {
            if (count == values.length) {
                int[] grown = new int[values.length == 0 ? 8 : values.length * 2];
                System.arraycopy(values, 0, grown, 0, count);
                values = grown;
            }
            values[count++] = i * 37 + 11;
        }
        long total = 0;
        for (int i = 0; i < count; i++) total += values[i];
        System.out.println(total);
    }
}
''')
JS = write('hand.js', '''const n = Number(process.argv[2]);
const values = [];
for (let i = 0; i < n; i++) values.push(i * 37 + 11);
let total = 0n;
for (const value of values) total += BigInt(value);
console.log(total.toString());
''')
PYTHON = write('hand.py', '''import sys
n = int(sys.argv[1])
values = []
for i in range(n):
    values.append(i * 37 + 11)
total = 0
for value in values:
    total += value
print(total)
''')


def main():
    required = [BIN / name for name in ['zi2zir', 'zi2c', 'zi2go', 'zi2zx']]
    missing = [str(path) for path in required if not path.is_file()]
    missing += [name for name in ['gcc', 'g++', 'go'] if not shutil.which(name)]
    if missing:
        raise RuntimeError('missing toolchain: ' + ', '.join(missing))
    (OUT / 'samples.jsonl').write_text('')
    before = source_hashes()
    meta = dict(created_utc=time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()),
                head=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
                status=subprocess.check_output(['git', 'status', '--short'], cwd=ROOT, text=True),
                platform=platform.platform(), cpu=cpu_name(),
                cpu_affinity=sorted(os.sched_getaffinity(0)),
                versions=versions(), source_hashes=before,
                toolchain_hashes={p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                                  for p in BIN.iterdir() if p.is_file()},
                sizes=[args.small, args.large],
                expected={str(n): expected(n) for n in [args.small, args.large]},
                repetitions={'compile': args.compile_repetitions,
                             'run': args.run_repetitions, 'discarded_warmups': 1},
                caveats=['Fresh processes include startup, loading, printing, and shutdown.',
                         'Go build uses a dedicated warm cache; it is not a forced recompilation.',
                         'C and Java use doubling arrays; C++, Go, Rust, JavaScript, and Python use growable collections.',
                         'JavaScript uses BigInt for the sum to preserve exact checksums.',
                         'No CPU isolation; medians are not regression thresholds.',
                         'VM large run omitted because its instruction budget is too small.',
                         'RSS from wait4 includes launcher accounting and is omitted from the table.'],
                unsupported=[])
    root_args = ['--root', FIX, '--module-path', ROOT / 'std']
    repeated([BIN / 'zi2zir', '--check-only', *root_args, ZI], 'ziran', 'source check')
    IR = OUT / 'ir'
    repeated([BIN / 'zi2zir', *root_args, '-o', IR, ZI], 'ziran', 'source to saved IR')
    SAVED = IR / 'collection.zir'
    repeated([BIN / 'zi2zir', '--check-only', '--root', IR,
              '--module-path', ROOT / 'std', SAVED], 'ziran', 'saved IR check')
    runtime = {}
    equality = {}
    for target, extension, tool, compiler, standard in [
        ('c', 'c', 'zi2c', 'gcc', '-std=c99'),
        ('go', 'go', 'zi2go', 'go', None),
    ]:
        generated = {}
        for kind, root, source in [('source', FIX, ZI), ('saved', IR, SAVED)]:
            output = OUT / f'{target}-{kind}'
            command = [BIN / tool, '--root', root, '--module-path', ROOT / 'std',
                       '--entry', 'collection:Answer', '-o', output]
            if target == 'go':
                command += ['--pkg', 'main']
            repeated([*command, source], 'ziran', f'{kind} to {target}')
            generated[kind] = {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                               for p in output.glob(f'*.{extension}')}
            if target == 'go':
                wrapper = output / 'main.go'
                wrapper.write_text('package main\nimport ("fmt";"os";"strconv")\n'
                                   'func main(){n,e:=strconv.Atoi(os.Args[1]);'
                                   'if e!=nil{panic(e)};fmt.Println(Collection_Kernel(int32(n)))}\n')
                build = ['go', 'build', '-o', output / 'app',
                         *sorted(output.glob('*.go'))]
            else:
                wrapper = output / f'main.{extension}'
                wrapper.write_text('#include "collection.h"\n#include <stdio.h>\n#include <stdlib.h>\n'
                                   'int main(int argc,char **argv){if(argc!=2)return 2;'
                                   'printf("%lld\\n",(long long)Kernel((int32_t)strtol(argv[1],0,10)));'
                                   'return 0;}\n')
                build = [compiler, standard, '-O2', '-I', ROOT / 'include',
                         '-I', output, *sorted(output.glob(f'*.{extension}')),
                         '-o', output / 'app', '-lm']
            repeated(build, 'cached go build' if target == 'go' else 'downstream compile',
                     f'ziran {target} ({kind})')
            runtime[f'ziran {target} ({kind})'] = [output / 'app']
        equality[target] = generated['source'] == generated['saved']
        if not equality[target]:
            raise AssertionError(f'{target} source and saved IR emission differ')
    for kind, root, source in [('source', FIX, ZI), ('saved', IR, SAVED)]:
        repeated([BIN / 'zi2zx', 'bundle', '--root', root,
                  '--module-path', ROOT / 'std', '--entry', 'collection:Answer',
                  '-o', OUT / f'{kind}.zx', source], 'ziran', f'{kind} to zx')
    if (OUT / 'source.zx').read_bytes() != (OUT / 'saved.zx').read_bytes():
        raise AssertionError('source and saved bundles differ')
    comparisons = {
        'hand C': ['gcc', '-O2', '-std=c99', C, '-o', FIX / 'hand-c'],
        'hand C++': ['g++', '-O2', '-std=c++17', CPP, '-o', FIX / 'hand-cpp'],
        'hand Go': ['go', 'build', '-o', FIX / 'hand-go', GO],
    }
    if shutil.which('rustc'):
        comparisons['hand Rust'] = ['rustc', '-C', 'opt-level=2', RUST,
                                    '-o', FIX / 'hand-rust']
    else:
        meta['unsupported'].append('Rust comparison: rustc unavailable')
    if shutil.which('javac') and shutil.which('java'):
        comparisons['hand Java'] = ['javac', '-d', FIX, JAVA]
    else:
        meta['unsupported'].append('Java comparison: javac/java unavailable')
    for name, command in comparisons.items():
        repeated(command, 'cached go build' if name == 'hand Go' else 'comparison compile', name)
    runtime.update({'hand C': [FIX / 'hand-c'], 'hand C++': [FIX / 'hand-cpp'],
                    'hand Go': [FIX / 'hand-go'],
                    'Python': ['python3', PYTHON]})
    if 'hand Rust' in comparisons:
        runtime['hand Rust'] = [FIX / 'hand-rust']
    if 'hand Java' in comparisons:
        runtime['hand Java'] = ['java', '-cp', FIX, 'CollectionGrowth']
    if shutil.which('node'):
        runtime['JavaScript'] = ['node', JS]
    else:
        meta['unsupported'].append('JavaScript comparison: node unavailable')
    for n in [args.small, args.large]:
        cases = list(runtime.items())
        if n == args.small:
            for kind in ['source', 'saved']:
                cases.append((f'ziran VM ({kind})', [BIN / 'zi2zx', 'run',
                                                     OUT / f'{kind}.zx']))
        oracle = str(expected(n))
        for name, command in cases:
            result = invoke([*command, *([] if name.startswith('ziran VM') else [str(n)])],
                            f'run n={n}', name, measured=False)
            if result != oracle:
                raise AssertionError(f'{name}: {result!r} != {oracle}')
        rng = random.Random(20260927 + n)
        for _ in range(args.run_repetitions):
            rng.shuffle(cases)
            for name, command in cases:
                result = invoke([*command, *([] if name.startswith('ziran VM') else [str(n)])],
                                f'run n={n}', name)
                if result != oracle:
                    raise AssertionError(f'{name}: {result!r} != {oracle}')
        print(f'run n={n}: {len(cases)} implementations x {args.run_repetitions}', flush=True)
    meta['generated_source_equality'] = equality
    meta['source_saved_bundle_identical'] = True
    meta['source_hashes_unchanged_during_measurement'] = before == source_hashes()
    meta['fixture_hashes'] = {str(p.relative_to(OUT)): hashlib.sha256(p.read_bytes()).hexdigest()
                              for p in FIX.iterdir() if p.is_file()}
    products = [OUT / f'{kind}.zx' for kind in ['source', 'saved']]
    for target, extension in [('c', 'c'), ('go', 'go')]:
        for kind in ['source', 'saved']:
            directory = OUT / f'{target}-{kind}'
            products.extend(directory.glob(f'*.{extension}'))
            products.append(directory / 'app')
    products += [FIX / 'hand-c', FIX / 'hand-cpp', FIX / 'hand-go']
    if 'hand Rust' in comparisons:
        products.append(FIX / 'hand-rust')
    if 'hand Java' in comparisons:
        products.append(FIX / 'CollectionGrowth.class')
    meta['artifact_hashes'] = {
        str(path.relative_to(OUT)): hashlib.sha256(path.read_bytes()).hexdigest()
        for path in products
    }
    groups = {}
    for sample in SAMPLES:
        groups.setdefault((sample['phase'], sample['case']), []).append(sample)
    rows = []
    for (phase, case), samples in groups.items():
        times = [sample['elapsed_ms'] for sample in samples]
        rows.append(dict(phase=phase, case=case, n=len(samples),
                         median_ms=statistics.median(times),
                         min_ms=min(times), max_ms=max(times)))
    rows.sort(key=lambda row: (row['phase'], row['case']))
    (OUT / 'metadata.json').write_text(json.dumps(meta, indent=2) + '\n')
    (OUT / 'summary.json').write_text(json.dumps(rows, indent=2) + '\n')
    table = ['# Collection-growth benchmark', '',
             'Fresh-process wall time; validated output for every sample. Not a language ranking.', '',
             '| Phase | Case | Median ms | Range ms | Samples |',
             '|---|---|---:|---:|---:|']
    for row in rows:
        table.append(f'| {row["phase"]} | {row["case"]} | {row["median_ms"]:.3f} | '
                     f'{row["min_ms"]:.3f}–{row["max_ms"]:.3f} | {row["n"]} |')
    table += ['', 'Expected checksums: ' + json.dumps(meta['expected']), '',
              'Generated source equality: ' + json.dumps(equality), '',
              'Source and saved-IR bundles are byte-identical.', '',
              'Unavailable comparisons: ' +
              (', '.join(meta['unsupported']) if meta['unsupported'] else 'none'), '',
              *('- ' + caveat for caveat in meta['caveats'])]
    (OUT / 'benchmark.md').write_text('\n'.join(table) + '\n')
    print('DONE: ' + str(OUT / 'benchmark.md'), flush=True)


if __name__ == '__main__':
    main()
