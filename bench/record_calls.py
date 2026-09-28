#!/usr/bin/env python3
"""Validated record-value/call benchmark across source, saved IR, and backends."""

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
parser.add_argument('--small', type=int, default=1000)
parser.add_argument('--large', type=int, default=500000)
args = parser.parse_args()
if (min(args.compile_repetitions, args.run_repetitions, args.small) < 1 or
        args.large <= args.small or args.large > 50000000):
    parser.error('repetitions and small must be positive; large must exceed small and fit s32')

OUT = (args.out or ROOT / 'build/benchmarks' /
       ('record-calls-' + time.strftime('%Y%m%dT%H%M%SZ', time.gmtime()))).resolve()
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


def expected(rounds):
    def signed(value):
        value &= 0xffffffff
        return value if value < 0x80000000 else value - 0x100000000
    points = [(i * 17 + 3, i * 29 + 7) for i in range(32)]
    total = 0
    for round in range(rounds):
        for index in range(32):
            next_index = (index + 1) % 32
            left, right = points[index], points[next_index]
            points[index] = (signed(left[0] + right[1] + index + round),
                             signed(left[1] + right[0] + 31 - index + round))
            total += points[index][0] + points[index][1]
    return total


ZI = write('records.zi', '''Point :: struct {
    x: s32
    y: s32
}

Mix :: (left: Point, right: Point, index: s32, round: s32) -> Point {
    return Point.{
        x = left.x + right.y + index + round,
        y = left.y + right.x + (cast(s32)31 - index) + round
    }
}

#program_export
Kernel :: (rounds: s32) -> s64 {
    points: [32]Point
    index: s32 = 0
    while index < cast(s32)points.count {
        points[index].x = index * 17 + 3
        points[index].y = index * 29 + 7
        index += 1
    }
    total: s64 = 0
    round: s32 = 0
    while round < rounds {
        index = 0
        while index < cast(s32)points.count {
            next: s32 = index + 1
            if next == cast(s32)points.count { next = 0 }
            points[index] = Mix(points[index], points[next], index, round)
            total += cast(s64)points[index].x + cast(s64)points[index].y
            index += 1
        }
        round += 1
    }
    return total
}
#program_export
Answer :: () -> s64 { return Kernel(ROUNDS) }
'''.replace('ROUNDS', str(args.small)))
C = write('hand.c', '''#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct { int32_t x, y; } Point;

static Point Mix(Point left, Point right, int32_t index, int32_t round) {
    Point result;
    result.x = (int32_t)((uint32_t)left.x + (uint32_t)right.y +
                         (uint32_t)index + (uint32_t)round);
    result.y = (int32_t)((uint32_t)left.y + (uint32_t)right.x +
                         (uint32_t)(31 - index) + (uint32_t)round);
    return result;
}

int main(int argc, char **argv) {
    if (argc != 2) return 2;
    int32_t rounds = (int32_t)strtol(argv[1], 0, 10);
    Point points[32];
    for (int32_t i = 0; i < 32; i++) {
        points[i].x = i * 17 + 3;
        points[i].y = i * 29 + 7;
    }
    int64_t total = 0;
    for (int32_t round = 0; round < rounds; round++) {
        for (int32_t index = 0; index < 32; index++) {
            int32_t next = index + 1 == 32 ? 0 : index + 1;
            points[index] = Mix(points[index], points[next], index, round);
            total += (int64_t)points[index].x + points[index].y;
        }
    }
    printf("%lld\\n", (long long)total);
    return 0;
}
''')
CPP = write('hand.cpp', '''#include <cstdint>
#include <cstdio>
#include <cstdlib>

struct Point { int32_t x, y; };

static Point Mix(Point left, Point right, int32_t index, int32_t round) {
    return Point{
        static_cast<int32_t>(static_cast<uint32_t>(left.x) +
            static_cast<uint32_t>(right.y) + static_cast<uint32_t>(index) +
            static_cast<uint32_t>(round)),
        static_cast<int32_t>(static_cast<uint32_t>(left.y) +
            static_cast<uint32_t>(right.x) +
            static_cast<uint32_t>(31 - index) + static_cast<uint32_t>(round))};
}

int main(int argc, char **argv) {
    if (argc != 2) return 2;
    int32_t rounds = static_cast<int32_t>(std::strtol(argv[1], nullptr, 10));
    Point points[32];
    for (int32_t i = 0; i < 32; i++) {
        points[i] = Point{i * 17 + 3, i * 29 + 7};
    }
    int64_t total = 0;
    for (int32_t round = 0; round < rounds; round++) {
        for (int32_t index = 0; index < 32; index++) {
            int32_t next = index + 1 == 32 ? 0 : index + 1;
            points[index] = Mix(points[index], points[next], index, round);
            total += static_cast<int64_t>(points[index].x) + points[index].y;
        }
    }
    std::printf("%lld\\n", static_cast<long long>(total));
    return 0;
}
''')
GO = write('hand.go', '''package main
import ("fmt"; "os"; "strconv")
type Point struct { x, y int32 }
func Mix(left Point, right Point, index int32, round int32) Point {
    return Point{left.x + right.y + index + round,
                 left.y + right.x + (31-index) + round}
}
func main() {
    rounds, err := strconv.Atoi(os.Args[1]); if err != nil { panic(err) }
    points := make([]Point, 32)
    for i := int32(0); i < 32; i++ { points[i] = Point{i*17+3, i*29+7} }
    total := int64(0)
    for round := int32(0); round < int32(rounds); round++ {
        for index := int32(0); index < 32; index++ {
            next := index + 1
            if next == 32 { next = 0 }
            points[index] = Mix(points[index], points[next], index, round)
            total += int64(points[index].x) + int64(points[index].y)
        }
    }
    fmt.Println(total)
}
''')
RUST = write('hand.rs', '''#[derive(Clone, Copy)]
struct Point { x: i32, y: i32 }

fn mix(left: Point, right: Point, index: i32, round: i32) -> Point {
    Point {
        x: left.x.wrapping_add(right.y.wrapping_add(index.wrapping_add(round))),
        y: left.y.wrapping_add(right.x.wrapping_add((31-index).wrapping_add(round))),
    }
}

fn main() {
    let rounds: i32 = std::env::args().nth(1).unwrap().parse().unwrap();
    let mut points: Vec<Point> = (0..32).map(|i| Point { x: i*17+3, y: i*29+7 }).collect();
    let mut total: i64 = 0;
    for round in 0..rounds {
        for index in 0..32 {
            let next = if index + 1 == 32 { 0 } else { index + 1 };
            points[index as usize] = mix(points[index as usize], points[next as usize], index, round);
            total += i64::from(points[index as usize].x) + i64::from(points[index as usize].y);
        }
    }
    println!("{}", total);
}
''')
JAVA = write('RecordCalls.java', '''class RecordCalls {
    record Point(int x, int y) {}
    static Point Mix(Point left, Point right, int index, int round) {
        return new Point(left.x() + right.y() + index + round,
                         left.y() + right.x() + (31-index) + round);
    }
    public static void main(String[] args) {
        int rounds = Integer.parseInt(args[0]);
        Point[] points = new Point[32];
        for (int i = 0; i < points.length; i++) points[i] = new Point(i*17+3, i*29+7);
        long total = 0;
        for (int round = 0; round < rounds; round++) {
            for (int index = 0; index < points.length; index++) {
                int next = index + 1 == points.length ? 0 : index + 1;
                points[index] = Mix(points[index], points[next], index, round);
                total += (long)points[index].x() + points[index].y();
            }
        }
        System.out.println(total);
    }
}
''')
JS = write('hand.js', '''const rounds = Number(process.argv[2]);
let points = [];
for (let i = 0; i < 32; i++) points.push({x:(i*17+3)|0, y:(i*29+7)|0});
let total = 0n;
for (let round = 0; round < rounds; round++) {
  for (let index = 0; index < points.length; index++) {
    const next = index + 1 === points.length ? 0 : index + 1;
    const left = points[index], right = points[next];
    points[index] = {
      x: (left.x + right.y + index + round)|0,
      y: (left.y + right.x + (31-index) + round)|0
    };
    total += BigInt(points[index].x) + BigInt(points[index].y);
  }
}
console.log(total.toString());
''')
PYTHON = write('hand.py', '''import sys

def signed(value):
    value &= 0xffffffff
    return value if value < 0x80000000 else value - 0x100000000

rounds = int(sys.argv[1])
points = [(i*17+3, i*29+7) for i in range(32)]
total = 0
for round in range(rounds):
    for index in range(32):
        next_index = (index + 1) % 32
        left, right = points[index], points[next_index]
        points[index] = (signed(left[0] + right[1] + index + round),
                         signed(left[1] + right[0] + 31-index + round))
        total += points[index][0] + points[index][1]
print(total)
''')

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


def main():
    required = [BIN / name for name in ['zi2zir', 'zi2c', 'zi2cpp', 'zi2go', 'zi2zib']]
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
                         'Each implementation updates a fixed 32-record table with wrapping s32 arithmetic.',
                         'JavaScript uses BigInt for the sum to preserve exact checksums.',
                         'No CPU isolation; medians are not regression thresholds.',
                         'VM large run omitted because record/call execution exceeds its instruction budget.',
                         'RSS from wait4 includes launcher accounting and is omitted from the table.'],
                unsupported=[])
    root_args = ['--root', FIX, '--module-path', ROOT / 'std']
    repeated([BIN / 'zi2zir', '--check-only', *root_args, ZI], 'ziran', 'source check')
    IR = OUT / 'ir'
    repeated([BIN / 'zi2zir', *root_args, '-o', IR, ZI], 'ziran', 'source to saved IR')
    SAVED = IR / 'records.zir'
    repeated([BIN / 'zi2zir', '--check-only', '--root', IR,
              '--module-path', ROOT / 'std', SAVED], 'ziran', 'saved IR check')
    runtime = {}
    equality = {}
    for target, extension, tool, compiler, standard in [
        ('c', 'c', 'zi2c', 'gcc', '-std=c99'),
        ('cpp', 'cpp', 'zi2cpp', 'g++', '-std=c++17'),
        ('go', 'go', 'zi2go', 'go', None),
    ]:
        generated = {}
        for kind, root, source in [('source', FIX, ZI), ('saved', IR, SAVED)]:
            output = OUT / f'{target}-{kind}'
            command = [BIN / tool, '--root', root, '--module-path', ROOT / 'std',
                       '--entry', 'records:Answer', '-o', output]
            if target == 'go':
                command += ['--pkg', 'main']
            repeated([*command, source], 'ziran', f'{kind} to {target}')
            generated[kind] = {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                               for p in output.glob(f'*.{extension}')}
            if target == 'go':
                wrapper = output / 'main.go'
                wrapper.write_text('package main\nimport ("fmt";"os";"strconv")\n'
                                   'func main(){n,e:=strconv.Atoi(os.Args[1]);'
                                   'if e!=nil{panic(e)};fmt.Println(Records_Kernel(int32(n)))}\n')
                build = ['go', 'build', '-o', output / 'app',
                         *sorted(output.glob('*.go'))]
            else:
                wrapper = output / f'main.{extension}'
                header = 'records.h' if target == 'c' else 'records.hpp'
                wrapper.write_text(f'#include "{header}"\n#include <stdio.h>\n#include <stdlib.h>\n'
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
        repeated([BIN / 'zi2zib', 'bundle', '--root', root,
                  '--module-path', ROOT / 'std', '--entry', 'records:Answer',
                  '-o', OUT / f'{kind}.zib', source], 'ziran', f'{kind} to zib')
    if (OUT / 'source.zib').read_bytes() != (OUT / 'saved.zib').read_bytes():
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
        runtime['hand Java'] = ['java', '-cp', FIX, 'RecordCalls']
    if shutil.which('node'):
        runtime['JavaScript'] = ['node', JS]
    else:
        meta['unsupported'].append('JavaScript comparison: node unavailable')
    for n in [args.small, args.large]:
        cases = list(runtime.items())
        if n == args.small:
            for kind in ['source', 'saved']:
                cases.append((f'ziran VM ({kind})', [BIN / 'zi2zib', 'run',
                                                     OUT / f'{kind}.zib']))
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
    products = [OUT / f'{kind}.zib' for kind in ['source', 'saved']]
    for target, extension in [('c', 'c'), ('cpp', 'cpp'), ('go', 'go')]:
        for kind in ['source', 'saved']:
            directory = OUT / f'{target}-{kind}'
            products.extend(directory.glob(f'*.{extension}'))
            products.append(directory / 'app')
    products += [FIX / 'hand-c', FIX / 'hand-cpp', FIX / 'hand-go']
    if 'hand Rust' in comparisons:
        products.append(FIX / 'hand-rust')
    if 'hand Java' in comparisons:
        products.append(FIX / 'RecordCalls.class')
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
    table = ['# Record-call benchmark', '',
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
