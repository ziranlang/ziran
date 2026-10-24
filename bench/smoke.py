#!/usr/bin/env python3
"""Repeatable cross-target smoke benchmark; process times include startup."""
import argparse
from pathlib import Path
import hashlib
import json
import os
import platform
import random
import shutil
import statistics
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--out', type=Path, help='new output directory (default: build/benchmarks/<UTC timestamp>)')
parser.add_argument('--bin-dir', type=Path, default=ROOT / 'build/bin')
parser.add_argument('--compile-repetitions', type=int, default=3)
parser.add_argument('--run-repetitions', type=int, default=5)
parser.add_argument('--small', type=int, default=20000)
parser.add_argument('--large', type=int, default=2000000)
args = parser.parse_args()
if args.compile_repetitions < 1 or args.run_repetitions < 1 or args.small < 1 or args.large <= args.small:
    parser.error('repetitions and small input must be positive; large must exceed small')
OUT = (args.out or ROOT / 'build/benchmarks' / time.strftime('%Y%m%dT%H%M%SZ', time.gmtime())).resolve()
if OUT.exists():
    parser.error(f'output directory already exists: {OUT}')
OUT.mkdir(parents=True)
FIX = OUT / 'fixtures'
BIN = args.bin_dir.resolve()
ENV = dict(os.environ)
ENV.pop('DISPLAY', None)
ENV.pop('WAYLAND_DISPLAY', None)
ENV['GO111MODULE'] = 'off'
ENV['GOCACHE'] = str(OUT / 'go-cache')
SMALL = args.small
LARGE = args.large
COMPILE_REPETITIONS = args.compile_repetitions
RUN_REPETITIONS = args.run_repetitions
FIX.mkdir(exist_ok=True)
SAMPLES = []

def write(name, text):
    path = FIX / name
    path.write_text(text)
    return path

def invoke(command, phase, case, measured=True):
    argv = [str(x) for x in command]
    with tempfile.TemporaryFile() as stdout, tempfile.TemporaryFile() as stderr:
        start = time.perf_counter_ns()
        child = subprocess.Popen(argv, cwd=ROOT, env=ENV, stdout=stdout, stderr=stderr)
        _, status, usage = os.wait4(child.pid, 0)
        elapsed = (time.perf_counter_ns() - start) / 1e6
        child.returncode = os.waitstatus_to_exitcode(status)
        stdout.seek(0)
        stderr.seek(0)
        output = stdout.read().decode(errors='replace')
        errors = stderr.read().decode(errors='replace')
    sample = dict(phase=phase, case=case, argv=argv, elapsed_ms=elapsed,
                  max_rss_kib=usage.ru_maxrss, user_s=usage.ru_utime,
                  system_s=usage.ru_stime, returncode=child.returncode,
                  stdout=output, stderr=errors)
    if measured:
        SAMPLES.append(sample)
        with (OUT / 'samples.jsonl').open('a') as log:
            log.write(json.dumps(sample) + '\n')
    if child.returncode:
        raise RuntimeError(f'{case} failed ({child.returncode}): {errors}')
    return output.strip()

def repeated(command, phase, case, count=None, expected=None):
    if count is None:
        count = COMPILE_REPETITIONS
    for index in range(count + 1):
        result = invoke(command, phase, case, measured=index > 0)
        if expected is not None and result != str(expected):
            raise AssertionError(f'{case}: {result!r} != {expected}')
    print(f'{phase}: {case}', flush=True)

def runtime_rounds(items, n):
    cases = list(items)
    if n == SMALL:
        for kind in ['source', 'saved']:
            cases.append((f'ziran VM ({kind})', [BIN/'zi2zx','run',OUT/f'{kind}.zx']))
    oracle = str(expected(n))
    for name, command in cases:
        result = invoke([*command, *([] if name.startswith('ziran VM') else [str(n)])],
                        f'run n={n}', name, measured=False)
        if result != oracle:
            raise AssertionError(f'{name}: {result!r} != {oracle}')
    rng = random.Random(20260927 + n)
    for _ in range(RUN_REPETITIONS):
        rng.shuffle(cases)
        for name, command in cases:
            result = invoke([*command, *([] if name.startswith('ziran VM') else [str(n)])],
                            f'run n={n}', name)
            if result != oracle:
                raise AssertionError(f'{name}: {result!r} != {oracle}')
    print(f'run n={n}: {len(cases)} implementations x {RUN_REPETITIONS} samples', flush=True)

ZI = write('smoke.zi', '''#program_export
Kernel :: (n: s64) -> s64 {
    x: s64 = 1
    total: s64 = 0
    i: s64 = 0
    while i < n {
        x = (x * 48271) % 2147483647
        total = (total + x) % 2147483647
        i += 1
    }
    return total
}
#program_export
Answer :: () -> s64 { return Kernel(20000) }
'''.replace('Kernel(20000)', f'Kernel({SMALL})'))
C_KERNEL = '''#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
static int64_t kernel(int64_t n) {
    int64_t x=1, total=0, i=0;
    while(i<n) { x=(x*48271)%2147483647; total=(total+x)%2147483647; i+=1; }
    return total;
}
int main(int argc, char **argv) {
    if(argc != 2) return 2;
    printf("%lld\\n", (long long)kernel(strtoll(argv[1], 0, 10)));
    return 0;
}
'''
C = write('hand.c', C_KERNEL)
CPP = write('hand.cpp', C_KERNEL)
GO = write('hand.go', '''package main
import ("fmt"; "os"; "strconv")
func kernel(n int64) int64 {
    var x, total, i int64 = 1, 0, 0
    for i<n { x=(x*48271)%2147483647; total=(total+x)%2147483647; i+=1 }
    return total
}
func main() { n,e:=strconv.ParseInt(os.Args[1],10,64); if e!=nil {panic(e)}; fmt.Println(kernel(n)) }
''')
RUST = write('hand.rs', '''fn kernel(n: i64) -> i64 {
    let (mut x, mut total, mut i): (i64,i64,i64) = (1,0,0);
    while i<n { x=(x*48271)%2147483647; total=(total+x)%2147483647; i+=1; }
    total
}
fn main() { let n: i64=std::env::args().nth(1).unwrap().parse().unwrap(); println!("{}",kernel(n)); }
''')
PYTHON = write('hand.py', '''import sys
def kernel(n):
    x, total, i = 1, 0, 0
    while i < n:
        x = (x * 48271) % 2147483647
        total = (total + x) % 2147483647
        i += 1
    return total
print(kernel(int(sys.argv[1])))
''')
JS = write('hand.js', '''function kernel(n) {
    let x=1, total=0, i=0;
    while(i<n) { x=(x*48271)%2147483647; total=(total+x)%2147483647; i+=1; }
    return total;
}
console.log(kernel(Number(process.argv[2])));
''')
JAVA = write('Smoke.java', '''class Smoke {
    static long kernel(long n) {
        long x=1, total=0, i=0;
        while(i<n) { x=(x*48271)%2147483647; total=(total+x)%2147483647; i+=1; }
        return total;
    }
    public static void main(String[] args) { System.out.println(kernel(Long.parseLong(args[0]))); }
}
''')

def expected(n):
    # Independent geometric-series oracle in the field modulo the prime.
    p, a = 2147483647, 48271
    return (a * (pow(a, n, p) - 1) * pow(a - 1, -1, p)) % p

def source_manifest():
    paths = sorted(list(ROOT.glob('cmd/**/*.c')) + list(ROOT.glob('cmd/**/*.h')) +
                   list(ROOT.glob('cmd/**/*.zi')) + list(ROOT.glob('std/**/*.zi')) +
                   list(ROOT.glob('include/*.h')) + [ROOT / 'Makefile', Path(__file__).resolve()])
    return {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}

def metadata():
    versions = {}
    for name, command in {
        'gcc':['gcc','--version'], 'g++':['g++','--version'], 'go':['go','version'],
        'rustc':['rustc','--version'], 'python':['python3','--version'],
        'node':['node','--version'], 'java':['java','-version'], 'javac':['javac','-version'],
    }.items():
        if not shutil.which(command[0]):
            versions[name] = None
            continue
        result = subprocess.run(command, cwd=ROOT, env=ENV, capture_output=True, text=True)
        versions[name] = (result.stdout + result.stderr).strip()
    return dict(created_utc=time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()),
                head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
                status=subprocess.check_output(['git','status','--short'],cwd=ROOT,text=True),
                platform=platform.platform(), cpu=Path('/proc/cpuinfo').read_text().split('model name')[1].split('\n')[0],
                cpu_affinity=sorted(os.sched_getaffinity(0)), versions=versions,
                source_hashes=source_manifest(),
                toolchain_hashes={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in BIN.glob('*') if p.is_file()},
                sizes=[SMALL,LARGE], expected={str(n):expected(n) for n in [SMALL,LARGE]},
                repetitions={'compile':COMPILE_REPETITIONS,'run':RUN_REPETITIONS,'discarded_warmups':1},
                commands='Every argv, result, and elapsed time is preserved in samples.jsonl.',
                unsupported=['VM large run: instruction budget prevents the large workload'],
                caveats=[
                    'Smoke baseline of one integer modular recurrence, not a language ranking.',
                    'Every run is a fresh process; timings include startup, loading, printing and shutdown.',
                    'Java and Node are not steady-state warmed JIT measurements; only filesystem/toolchain caches are warm.',
                    'C/C++ use -O2 without LTO or march=native; Rust uses opt-level=2; Go uses defaults.',
                    'Python arbitrary precision and JavaScript exact integer-valued Number arithmetic differ in representation.',
                    'All products stay below 2^53 and signed 64-bit range, so results are exactly comparable.',
                    'Zi check/emit are whole invocations, not disaggregated internal compiler phases.',
                    'Saved IR is checked again; source-to-IR cost is separate, not included in saved-IR emit.',
                    'Go uses a dedicated warmed build cache, including the unchanged program itself: these are cached go build invocations, not forced recompiles comparable to GCC.',
                    'VM small input only: 1,000,000-step execution limit prevents the large workload.',
                    'No CPU pinning/frequency isolation; background system activity may affect samples.',
                    'Raw wait4 RSS can include the inherited Python launcher high-water floor; it is launch accounting, not comparable executable peak memory. RSS is omitted from the displayed table.',
                    'Bend, Zig, Clang, and GPU execution are outside this smoke matrix; none are approximated.',
                ])

def main():
    required = {'zi2zir':BIN/'zi2zir','zi2c':BIN/'zi2c','zi2go':BIN/'zi2go',
                'zi2zx':BIN/'zi2zx'}
    missing = [str(path) for path in required.values() if not path.is_file()]
    missing += [tool for tool in ['gcc','g++','go'] if not shutil.which(tool)]
    if missing:
        raise RuntimeError('build Ziran and install required C/C++/Go compilers: '+', '.join(missing))
    (OUT / 'samples.jsonl').write_text('')
    meta = metadata()
    (OUT / 'metadata.json').write_text(json.dumps(meta, indent=2) + '\n')
    root_args = ['--root', FIX]
    repeated([BIN/'zi2zir','--check-only',*root_args,ZI], 'ziran', 'source check')
    IR = OUT / 'ir'
    repeated([BIN/'zi2zir',*root_args,'-o',IR,ZI], 'ziran', 'source to saved IR')
    SAVED = IR / 'smoke.zir'
    repeated([BIN/'zi2zir','--check-only','--root',IR,SAVED], 'ziran', 'saved IR check')
    runtime = {}
    for target, ext, tool, compiler, standard in [
        ('c','c','zi2c','gcc','-std=c99'),
        ('go','go','zi2go','go',None),
    ]:
        for kind, root, source in [('source',FIX,ZI),('saved',IR,SAVED)]:
            output = OUT / f'{target}-{kind}'
            args = [BIN/tool,'--root',root,'--entry','smoke:Answer','-o',output]
            if target == 'go': args += ['--pkg','main']
            repeated([*args,source], 'ziran', f'{kind} to {target}')
            if target == 'go':
                wrapper = output/'main.go'
                wrapper.write_text('package main\nimport ("fmt"; "os"; "strconv")\nfunc main(){n,e:=strconv.ParseInt(os.Args[1],10,64);if e!=nil{panic(e)};fmt.Println(Smoke_Kernel(n))}\n')
                command = ['go','build','-o',output/'app',output/'smoke.go',wrapper]
            else:
                wrapper = output/f'main.{ext}'
                wrapper.write_text('#include "smoke.h"\n#include <stdio.h>\n#include <stdlib.h>\nint main(int argc,char **argv){{if(argc!=2)return 2;printf("%lld\\n",(long long)Kernel(strtoll(argv[1],0,10)));return 0;}}\n')
                command = [compiler,standard,'-O2','-I',ROOT/'include','-I',output,
                           output/f'smoke.{ext}',wrapper,'-o',output/'app','-lm']
            phase = 'cached go build' if target == 'go' else 'downstream compile'
            repeated(command, phase, f'ziran {target} ({kind})')
            runtime[f'ziran {target} ({kind})'] = [output/'app']
        source_bytes = (OUT/f'{target}-source'/f'smoke.{ext}').read_bytes()
        saved_bytes = (OUT/f'{target}-saved'/f'smoke.{ext}').read_bytes()
        meta.setdefault('generated_source_equality',{})[target] = source_bytes == saved_bytes
        if source_bytes != saved_bytes:
            raise AssertionError(f'{target} generated source differs between source and saved IR')
    for kind,root,source in [('source',FIX,ZI),('saved',IR,SAVED)]:
        bundle = OUT/f'{kind}.zx'
        repeated([BIN/'zi2zx','bundle','--root',root,'--entry','smoke:Answer','-o',bundle,source],
                 'ziran', f'{kind} to zx')
    assert (OUT/'source.zx').read_bytes() == (OUT/'saved.zx').read_bytes()
    meta['source_saved_bundle_identical'] = True
    builds = {
        'hand C':['gcc','-O2','-std=c99',C,'-o',FIX/'hand-c'],
        'hand C++':['g++','-O2','-std=c++17',CPP,'-o',FIX/'hand-cpp'],
        'hand Go':['go','build','-o',FIX/'hand-go',GO],
    }
    if shutil.which('rustc'):
        builds['hand Rust'] = ['rustc','-C','opt-level=2',RUST,'-o',FIX/'hand-rust']
    else:
        meta['unsupported'].append('Rust comparison: rustc unavailable')
    if shutil.which('javac') and shutil.which('java'):
        builds['hand Java'] = ['javac','-d',FIX,JAVA]
    else:
        meta['unsupported'].append('Java comparison: javac/java unavailable')
    for name,command in builds.items():
        repeated(command,'cached go build' if name == 'hand Go' else 'comparison compile',name)
    runtime.update({'hand C':[FIX/'hand-c'],'hand C++':[FIX/'hand-cpp'],
                    'hand Go':[FIX/'hand-go'],'Python':['python3',PYTHON]})
    if 'hand Rust' in builds: runtime['hand Rust'] = [FIX/'hand-rust']
    if shutil.which('node'): runtime['JavaScript'] = ['node',JS]
    else: meta['unsupported'].append('JavaScript comparison: node unavailable')
    if 'hand Java' in builds: runtime['Java'] = ['java','-cp',FIX,'Smoke']
    for n in [SMALL,LARGE]:
        runtime_rounds(runtime.items(), n)
    after = source_manifest()
    meta['source_hashes_unchanged_during_measurement'] = after == meta['source_hashes']
    meta['head_after'] = subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip()
    meta['fixture_hashes'] = {str(p.relative_to(OUT)):hashlib.sha256(p.read_bytes()).hexdigest()
                              for p in FIX.iterdir() if p.suffix in ['.zi','.c','.cpp','.go','.rs','.py','.js','.java']}
    artifact_paths = [OUT/'source.zx',OUT/'saved.zx']
    artifact_paths += [OUT/f'{target}-{kind}'/'app' for target in ['c','go'] for kind in ['source','saved']]
    artifact_paths += [FIX/name for name in ['hand-c','hand-cpp','hand-go']]
    if 'hand Rust' in builds: artifact_paths.append(FIX/'hand-rust')
    if 'hand Java' in builds: artifact_paths.append(FIX/'Smoke.class')
    meta['artifact_bytes'] = {str(p.relative_to(OUT)):p.stat().st_size for p in artifact_paths}
    groups = {}
    for sample in SAMPLES: groups.setdefault((sample['phase'],sample['case']),[]).append(sample)
    rows = []
    for (phase,case),samples in groups.items():
        times = [s['elapsed_ms'] for s in samples]
        rows.append(dict(phase=phase,case=case,median_ms=statistics.median(times),min_ms=min(times),
                         max_ms=max(times),median_rss_kib=statistics.median(s['max_rss_kib'] for s in samples),n=len(samples)))
    rows.sort(key=lambda row: (row['phase'], row['case']))
    (OUT/'metadata.json').write_text(json.dumps(meta,indent=2)+'\n')
    (OUT/'summary.json').write_text(json.dumps(rows,indent=2)+'\n')
    table = ['# Integer-kernel smoke baseline','', 'Process wall time includes startup. Warm-cache medians; not a language ranking.','',
             '| Phase | Case | Median ms | Range ms | Samples |',
             '|---|---|---:|---:|---:|']
    for r in rows:
        table.append(f'| {r["phase"]} | {r["case"]} | {r["median_ms"]:.3f} | {r["min_ms"]:.3f}–{r["max_ms"]:.3f} | {r["n"]} |')
    table += ['', 'Checksums: '+json.dumps(meta['expected']), '', 'Generated source identity: '+json.dumps(meta['generated_source_equality']),
              '', 'Source/saved bundles are byte-identical. All runtime samples matched the independent modular-series oracle.',
              '', *('- '+note for note in meta['caveats'])]
    (OUT/'benchmark.md').write_text('\n'.join(table)+'\n')
    print('DONE: '+str(OUT/'benchmark.md'),flush=True)

if __name__ == '__main__': main()
