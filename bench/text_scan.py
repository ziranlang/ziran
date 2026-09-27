#!/usr/bin/env python3
"""Validated UTF-8 scan benchmark across source, saved IR, and backends."""

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
parser.add_argument('--large', type=int, default=2000000)
args = parser.parse_args()
if (min(args.compile_repetitions, args.run_repetitions, args.small) < 1 or
        args.large <= args.small or args.large > 50000000):
    parser.error('repetitions and small must be positive; large must exceed small and fit s64')
OUT = (args.out or ROOT / 'build/benchmarks' /
       ('text-scan-' + time.strftime('%Y%m%dT%H%M%SZ', time.gmtime()))).resolve()
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
    return 148921 * rounds


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
                  system_s=usage.ru_stime,
                  returncode=os.waitstatus_to_exitcode(status),
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


def source_hashes():
    paths = sorted(list(ROOT.glob('cmd/**/*')) + list(ROOT.glob('std/**/*')) +
                   list(ROOT.glob('include/**/*')) +
                   [ROOT / 'Makefile', Path(__file__).resolve()])
    return {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in paths if path.is_file()}


def versions():
    tools = {'gcc': ['gcc', '--version'], 'g++': ['g++', '--version'],
             'go': ['go', 'version'],
             'rustc': ['rustc', '--version'], 'javac': ['javac', '-version'],
             'java': ['java', '-version'], 'node': ['node', '--version'],
             'python': ['python3', '--version'], '9c': ['9c']}
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


ZI = write('text_scan.zi', f'''#import "utf8"
TEXT: string = "aé中🙂";

#program_export
Kernel :: (rounds: s64) -> s64 {{
    total: s64 = 0
    round: s64 = 0
    while round < rounds {{
        at: s32 = 0
        count: s32 = 0
        while at < cast(s32)TEXT.count {{
            decoded: Decoded = Decode(TEXT, at)
            if !decoded.valid {{ return -1 }}
            total += cast(s64)decoded.value
            count += 1
            at = decoded.after
        }}
        if count != 4 {{ return -2 }}
        round += 1
    }}
    return total
}}

#program_export
Answer :: () -> s64 {{ return Kernel({args.small}) }}
''')
C = write('hand.c', '''#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static const unsigned char text[] = {0x61,0xc3,0xa9,0xe4,0xb8,0xad,0xf0,0x9f,0x99,0x82};

static int32_t advance(int32_t at, uint32_t *code) {
    unsigned char first = text[at];
    if (first < 0x80) { *code = first; return at + 1; }
    int count = 0;
    uint32_t minimum = 0, value = 0;
    if (first >= 0xc2 && first <= 0xdf) { count = 1; minimum = 0x80; value = first & 31; }
    else if (first >= 0xe0 && first <= 0xef) { count = 2; minimum = 0x800; value = first & 15; }
    else if (first >= 0xf0 && first <= 0xf4) { count = 3; minimum = 0x10000; value = first & 7; }
    if (!count || at + count > 10) { *code = 0xfffd; return at + 1; }
    for (int i = 1; i <= count; i++) {
        if ((text[at + i] & 0xc0) != 0x80) { *code = 0xfffd; return at + 1; }
        value = (value << 6) | (unsigned)(text[at + i] & 63);
    }
    if (value < minimum || value > 0x10ffff ||
        (value >= 0xd800 && value <= 0xdfff)) { *code = 0xfffd; return at + 1; }
    *code = value;
    return at + count + 1;
}

int main(int argc, char **argv) {
    if (argc != 2) return 2;
    int64_t rounds = strtoll(argv[1], 0, 10), total = 0;
    for (int64_t round = 0; round < rounds; round++) {
        int32_t at = 0;
        int32_t count = 0;
        while (at < 10) {
            uint32_t code = 0;
            at = advance(at, &code);
            total += (int64_t)code;
            count++;
        }
        if (count != 4) return 3;
    }
    printf("%lld\\n", (long long)total);
    return 0;
}
''')
CPP = write('hand.cpp', '''#include <cstdint>
#include <cstdio>
#include <cstdlib>

static const unsigned char text[] = {0x61,0xc3,0xa9,0xe4,0xb8,0xad,0xf0,0x9f,0x99,0x82};

static int32_t advance(int32_t at, uint32_t &code) {
    unsigned first = text[at];
    if (first < 0x80) { code = first; return at + 1; }
    int count = 0;
    uint32_t minimum = 0, value = 0;
    if (first >= 0xc2 && first <= 0xdf) { count = 1; minimum = 0x80; value = first & 31; }
    else if (first >= 0xe0 && first <= 0xef) { count = 2; minimum = 0x800; value = first & 15; }
    else if (first >= 0xf0 && first <= 0xf4) { count = 3; minimum = 0x10000; value = first & 7; }
    if (!count || at + count > 10) { code = 0xfffd; return at + 1; }
    for (int i = 1; i <= count; i++) {
        if ((text[at + i] & 0xc0) != 0x80) { code = 0xfffd; return at + 1; }
        value = (value << 6) | (text[at + i] & 63);
    }
    if (value < minimum || value > 0x10ffff ||
        (value >= 0xd800 && value <= 0xdfff)) { code = 0xfffd; return at + 1; }
    code = value;
    return at + count + 1;
}

int main(int argc, char **argv) {
    if (argc != 2) return 2;
    int64_t rounds = std::strtoll(argv[1], nullptr, 10), total = 0;
    for (int64_t round = 0; round < rounds; round++) {
        int32_t at = 0, count = 0;
        while (at < 10) {
            uint32_t code = 0;
            at = advance(at, code);
            total += static_cast<int64_t>(code);
            ++count;
        }
        if (count != 4) return 3;
    }
    std::printf("%lld\\n", static_cast<long long>(total));
    return 0;
}
''')
GO = write('hand.go', '''package main
import ("fmt";"os";"strconv")
func main() {
    rounds,e := strconv.ParseInt(os.Args[1],10,64); if e != nil { panic(e) }
    const text = "aé中🙂"
    var total int64
    for round := int64(0); round < rounds; round++ {
        count := 0
        for _, code := range text { total += int64(code); count++ }
        if count != 4 { panic("codepoint count") }
    }
    fmt.Println(total)
}
''')
RUST = write('hand.rs', '''fn main() {
    let rounds: i64 = std::env::args().nth(1).unwrap().parse().unwrap();
    let text = "aé中🙂";
    let mut total: i64 = 0;
    for _ in 0..rounds {
        let mut count = 0;
        for code in text.chars() { total += code as i64; count += 1; }
        if count != 4 { panic!("codepoint count"); }
    }
    println!("{}", total);
}
''')
JAVA = write('TextScan.java', '''class TextScan {
    public static void main(String[] args) {
        long rounds = Long.parseLong(args[0]);
        String text = "aé中🙂";
        long total = 0;
        for (long round = 0; round < rounds; round++) {
            int at = 0, count = 0;
            while (at < text.length()) {
                int code = text.codePointAt(at);
                at += Character.charCount(code);
                total += code;
                count++;
            }
            if (count != 4) throw new IllegalStateException("codepoint count");
        }
        System.out.println(total);
    }
}
''')
JS = write('hand.js', '''const rounds = Number(process.argv[2]);
const text = "aé中🙂";
let total = 0;
for (let round = 0; round < rounds; round++) {
    let count = 0;
    for (const character of text) { total += character.codePointAt(0); count++; }
    if (count !== 4) throw new Error("codepoint count");
}
console.log(total);
''')
PYTHON = write('hand.py', '''import sys
rounds = int(sys.argv[1])
text = "aé中🙂"
total = 0
for _ in range(rounds):
    count = 0
    for character in text:
        total += ord(character)
        count += 1
    if count != 4:
        raise RuntimeError("codepoint count")
print(total)
''')


def output_hashes(directory):
    return {str(path.relative_to(directory)): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in sorted(directory.glob('*.[ch]')) if path.is_file()}


def write_plan9_compat(directory, entry):
    directory.mkdir()
    (directory / 'u.h').write_text("""#ifndef FAKE_U_H
#define FAKE_U_H
typedef signed char schar;
typedef unsigned char uchar;
typedef short ushort;
typedef unsigned int uint;
typedef long long vlong;
typedef unsigned long long uvlong;
typedef unsigned long usize;
#endif
""")
    (directory / 'libc.h').write_text("""#ifndef FAKE_LIBC_H
#define FAKE_LIBC_H
extern void *realloc(void *, unsigned long);
extern void free(void *);
extern void *memmove(void *, const void *, unsigned long);
extern void *memcpy(void *, const void *, unsigned long);
extern void *memset(void *, int, unsigned long);
extern int memcmp(const void *, const void *, unsigned long);
extern int fprint(int, const char *, ...);
extern void abort(void);
#endif
""")
    (directory / 'runner.c').write_text(f"""#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
long long {entry}(long long);
int fprint(int fd, const char *format, ...) {{
    char buffer[512];
    va_list args;
    va_start(args, format);
    int count = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    return count < 0 ? count :
        (int)write(fd, buffer, (unsigned long)count);
}}
int main(int argc, char **argv) {{
    if (argc != 2) return 2;
    long long value = strtoll(argv[1], NULL, 10);
    printf("%lld\\n", (long long){entry}(value));
    return 0;
}}
""")


def main():
    required = [BIN / name for name in ['ziran', 'zi2zir', 'zi2c',
                                          'zi2cpp', 'zi2go', 'zi2zib']]
    missing = [str(path) for path in required if not path.is_file()]
    missing += [name for name in ['gcc', 'g++', 'go'] if not shutil.which(name)]
    if missing:
        raise RuntimeError('missing toolchain: ' + ', '.join(missing))
    (OUT / 'samples.jsonl').write_text('')
    before = source_hashes()
    meta = dict(created_utc=time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()),
                head=subprocess.check_output(['git', 'rev-parse', 'HEAD'],
                                             cwd=ROOT, text=True).strip(),
                status=subprocess.check_output(['git', 'status', '--short'],
                                               cwd=ROOT, text=True),
                platform=platform.platform(), cpu=cpu_name(),
                cpu_affinity=sorted(os.sched_getaffinity(0)),
                versions=versions(), source_hashes=before,
                toolchain_hashes={p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                                  for p in BIN.iterdir() if p.is_file()},
                sizes=[args.small, args.large],
                expected={str(size): expected(size) for size in [args.small, args.large]},
                repetitions={'compile': args.compile_repetitions,
                             'run': args.run_repetitions, 'discarded_warmups': 1},
                caveats=['Fresh processes include startup, loading, printing, and shutdown.',
                         'Go build uses a dedicated warm cache; it is not a forced recompilation.',
                         'Ziran, C, and C++ use explicit UTF-8 decoders; Go, Rust, Java, JavaScript, and Python use their native codepoint iteration.',
                         'No CPU isolation; medians are not regression thresholds.',
                         'VM large run omitted because its instruction budget is too small.',
                         'RSS from wait4 includes launcher accounting and is omitted from the table.',
                         'Plan 9 C emission includes checking, target lowering, and the post-pass in one process; compare it with direct C emission rather than interpreting the difference as an isolated post-pass cost.',
                         'Plan 9 dialect execution is compiled by host GCC against a minimal fake Plan 9 libc when no Plan 9 compiler is installed; it validates output and dialect shape but is not Plan 9 hardware performance.'],
                unsupported=[])
    root_args = ['--root', FIX, '--module-path', ROOT / 'std']
    repeated([BIN / 'zi2zir', '--check-only', *root_args, ZI],
             'ziran', 'source check')
    IR = OUT / 'ir'
    repeated([BIN / 'zi2zir', *root_args, '-o', IR, ZI],
             'ziran', 'source to saved IR')
    SAVED = IR / 'text_scan.zir'
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
                       '--entry', 'text_scan:Kernel', '-o', output]
            if target == 'go':
                command += ['--pkg', 'main']
            repeated([*command, source], 'ziran', f'{kind} to {target}')
            generated[kind] = {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                               for p in output.glob(f'*.{extension}')}
            if target == 'go':
                (output / 'main.go').write_text(
                    'package main\nimport ("fmt";"os";"strconv")\n'
                    'func main(){n,e:=strconv.ParseInt(os.Args[1],10,64);'
                    'if e!=nil{panic(e)};fmt.Println(TextScan_Kernel(n))}\n')
                build = ['go', 'build', '-o', output / 'app',
                         *sorted(output.glob('*.go'))]
            else:
                header = 'text_scan.h' if target == 'c' else 'text_scan.hpp'
                (output / f'main.{extension}').write_text(
                    f'#include "{header}"\n#include <stdio.h>\n#include <stdlib.h>\n'
                    'int main(int argc,char **argv){if(argc!=2)return 2;'
                    'printf("%lld\\n",(long long)Kernel(strtoll(argv[1],0,10)));return 0;}\n')
                build = [compiler, standard, '-O2', '-I', ROOT / 'include',
                         '-I', output, *sorted(output.glob(f'*.{extension}')),
                         '-o', output / 'app', '-lm']
            repeated(build, 'cached go build' if target == 'go' else 'downstream compile',
                     f'ziran {target} ({kind})')
            runtime[f'ziran {target} ({kind})'] = [output / 'app']
        equality[target] = generated['source'] == generated['saved']
        if not equality[target]:
            raise AssertionError(f'{target} source and saved IR emission differ')
    plan9_generated = {}
    plan9_direct_equal = {}
    plan9_compat = OUT / 'plan9-include'
    write_plan9_compat(plan9_compat, 'Kernel')
    for kind, root, source in [('source', FIX, ZI), ('saved', IR, SAVED)]:
        output = OUT / f'plan9-{kind}'
        repeated([BIN / 'ziran', 'build', '--target=plan9-c',
                  '--root', root, '--module-path', ROOT / 'std',
                  '--entry', 'text_scan:Kernel', '-o', output, source],
                 'ziran', f'{kind} to plan9-c (dispatcher)')
        direct = OUT / f'plan9-direct-{kind}'
        repeated([BIN / 'zi2c', '--target=plan9-c',
                  '--root', root, '--module-path', ROOT / 'std',
                  '--entry', 'text_scan:Kernel', '-o', direct, source],
                 'ziran', f'{kind} to plan9-c (direct)')
        plan9_generated[kind] = output_hashes(output)
        plan9_direct_equal[kind] = output_hashes(output) == output_hashes(direct)
        if not plan9_direct_equal[kind]:
            raise AssertionError(f'plan9-c {kind} dispatcher and direct output differ')
        build = ['gcc', '-std=c11', '-O2', '-I', plan9_compat, '-I', output,
                 *sorted(output.glob('*.c')), plan9_compat / 'runner.c',
                 '-o', output / 'app', '-lm']
        repeated(build, 'fake Plan 9 compile',
                 f'ziran plan9-c ({kind})')
        runtime[f'ziran plan9-c dialect ({kind})'] = [output / 'app']
    equality['plan9-c'] = plan9_generated['source'] == plan9_generated['saved']
    if not equality['plan9-c']:
        raise AssertionError('plan9-c source and saved IR emission differ')
    if not shutil.which('9c'):
        meta['unsupported'].append('Plan 9 native execution: 9c unavailable')
    for kind, root, source in [('source', FIX, ZI), ('saved', IR, SAVED)]:
        repeated([BIN / 'zi2zib', 'bundle', '--root', root,
                  '--module-path', ROOT / 'std', '--entry', 'text_scan:Answer',
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
        runtime['hand Java'] = ['java', '-cp', FIX, 'TextScan']
    if shutil.which('node'):
        runtime['JavaScript'] = ['node', JS]
    else:
        meta['unsupported'].append('JavaScript comparison: node unavailable')
    for size in [args.small, args.large]:
        cases = list(runtime.items())
        if size == args.small:
            for kind in ['source', 'saved']:
                cases.append((f'ziran VM ({kind})', [BIN / 'zi2zib', 'run',
                                                     OUT / f'{kind}.zib']))
        oracle = str(expected(size))
        for name, command in cases:
            arguments = [] if name.startswith('ziran VM') else [str(size)]
            result = invoke([*command, *arguments], f'run rounds={size}', name,
                            measured=False)
            if result != oracle:
                raise AssertionError(f'{name}: {result!r} != {oracle}')
        rng = random.Random(20260927 + size)
        for _ in range(args.run_repetitions):
            rng.shuffle(cases)
            for name, command in cases:
                arguments = [] if name.startswith('ziran VM') else [str(size)]
                result = invoke([*command, *arguments], f'run rounds={size}', name)
                if result != oracle:
                    raise AssertionError(f'{name}: {result!r} != {oracle}')
        print(f'run rounds={size}: {len(cases)} implementations x '
              f'{args.run_repetitions}', flush=True)
    meta['generated_source_equality'] = equality
    meta['plan9_dispatcher_direct_equality'] = plan9_direct_equal
    meta['plan9_post_pass_measurement'] = {
        'method': 'compare direct C and direct plan9-c whole-process emission',
        'isolated': False,
    }
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
    for prefix in ['plan9', 'plan9-direct']:
        for kind in ['source', 'saved']:
            directory = OUT / f'{prefix}-{kind}'
            products.extend(directory.glob('*.c'))
            products.extend(directory.glob('*.h'))
            if (directory / 'app').is_file():
                products.append(directory / 'app')
    products += [FIX / 'hand-c', FIX / 'hand-cpp', FIX / 'hand-go']
    if 'hand Rust' in comparisons:
        products.append(FIX / 'hand-rust')
    if 'hand Java' in comparisons:
        products.append(FIX / 'TextScan.class')
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
    table = ['# UTF-8 scan benchmark', '',
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
