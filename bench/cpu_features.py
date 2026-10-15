#!/usr/bin/env python3
"""Validate and measure CPU-selected assembly against Ziran and C fallbacks."""
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
MASK = (1 << 64) - 1
SEED = 0x123456789ABCDEF
X86 = '''xorq %[out], %[out]
testq %[rounds], %[rounds]
jle .Ldone%=
movq %[seed], %%rdx
xorq %%rcx, %%rcx
.Lloop%=:
movq %%rdx, %%r8
shlq $13, %%r8
xorq %%r8, %%rdx
movq %%rdx, %%r8
shrq $7, %%r8
xorq %%r8, %%rdx
movq %%rdx, %%r8
shlq $17, %%r8
xorq %%r8, %%rdx
popcnt %%rdx, %%r8
addq %%r8, %[out]
incq %%rcx
cmpq %[rounds], %%rcx
jl .Lloop%=
.Ldone%=:
'''
ARM = '''mov %[out], xzr
cmp %[rounds], #0
b.le .Ldone%=
mov x9, %[seed]
mov x10, xzr
.Lloop%=:
eor x9, x9, x9, lsl #13
eor x9, x9, x9, lsr #7
eor x9, x9, x9, lsl #17
fmov d0, x9
cnt v0.8b, v0.8b
addv b0, v0.8b
umov w11, v0.b[0]
add %[out], %[out], x11
add x10, x10, #1
cmp x10, %[rounds]
b.lt .Lloop%=
.Ldone%=:
'''


def oracle(n):
    value, total = SEED, 0
    for _ in range(n):
        value ^= (value << 13) & MASK
        value ^= value >> 7
        value ^= (value << 17) & MASK
        total += value.bit_count()
    return total & MASK


def source(small):
    modifiers = []
    for arch, template, clobbers in [('x86_64+popcnt', X86, 'cc;rcx;rdx;r8'),
                                     ('aarch64+simd', ARM, 'cc;x9;x10;x11;v0')]:
        modifiers.append('#asm('+', '.join(json.dumps(s) for s in
                         [arch, template, '=&r', 'r;r', clobbers, ''])+')')
    return '''Count :: (value: u64) -> u64 {
    bits := value - ((value >> 1) & 0x5555555555555555)
    bits = (bits & 0x3333333333333333) + ((bits >> 2) & 0x3333333333333333)
    return (((bits + (bits >> 4)) & 0x0f0f0f0f0f0f0f0f) * 0x0101010101010101) >> 56
}
#program_export
Kernel :: (rounds: s64, seed: u64) -> u64 MODIFIERS {
    value := seed
    total: u64
    i: s64
    while i < rounds {
        value ^= value << 13
        value ^= value >> 7
        value ^= value << 17
        total += Count(value)
        i += 1
    }
    return total
}
#program_export
Answer :: () -> s64 { return cast(s64)Kernel(SMALL, SEED) }
'''.replace('MODIFIERS', ' '.join(modifiers)).replace('SMALL', str(small)).replace('SEED', str(SEED))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--bin-dir', type=Path, default=ROOT/'build/bin')
    parser.add_argument('--compile-repetitions', type=int, default=3)
    parser.add_argument('--run-repetitions', type=int, default=9)
    parser.add_argument('--small', type=int, default=1000)
    parser.add_argument('--large', type=int, default=20000000)
    args = parser.parse_args()
    if min(args.small, args.compile_repetitions, args.run_repetitions) < 1 or not args.small < args.large <= 50000000:
        parser.error('positive repetitions and 0 < small < large <= 50000000 required')
    output = args.out.resolve()
    if output.exists(): parser.error('output exists; preserve receipts and clear inactive outputs before reuse')
    cc = shutil.which(os.environ.get('CC', 'cc'))
    if not cc or not hasattr(os, 'wait4'): parser.error('requires a C compiler and POSIX wait4')
    output.mkdir(parents=True)
    binaries = args.bin_dir.resolve()
    env = dict(os.environ)
    for key in ['DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'DBUS_SESSION_BUS_ADDRESS',
                'ZIRAN_PROFILE', 'ZIRAN_VM_PROFILE']:
        env.pop(key, None)
    launcher = []
    if tool := shutil.which('time'):
        version = subprocess.run([tool, '--version'], capture_output=True, text=True)
        if version.returncode == 0 and 'GNU' in version.stdout + version.stderr: launcher = [tool]
    if not launcher and (tool := shutil.which('busybox')):
        help_result = subprocess.run([tool, 'time', '--help'], capture_output=True, text=True)
        if '-f FMT' in help_result.stdout + help_result.stderr: launcher = [tool, 'time']
    digest = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
    git = lambda *argv: subprocess.check_output(['git', *argv], cwd=ROOT, text=True).strip()
    metadata = dict(schema_version=1, head=git('rev-parse', 'HEAD'), status=git('status', '--short'),
        platform=platform.platform(), machine=platform.machine(),
        created_utc=time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()),
        compiler=subprocess.check_output([cc, '--version'], text=True), measurement_launcher=launcher,
        input=dict(seed=SEED, small=args.small, large=args.large), discarded_warmups=1,
        compile_repetitions=args.compile_repetitions, run_repetitions=args.run_repetitions,
        tools={name: digest(binaries/name) for name in ['zi2zir', 'zi2c', 'zi2zib']},
        source_hashes={str(p.relative_to(ROOT)): digest(p) for directory in ['cmd', 'std', 'include']
                       for p in (ROOT/directory).rglob('*') if p.is_file() and p.suffix in ['.c', '.h', '.zi']},
        caveats=['One dependent xorshift/popcount workload; not a language ranking.',
                 'Assembly loops and compiler-optimized loops may differ; every result uses a separate Python oracle.',
                 'Native timings include process startup and the measurement launcher when available.',
                 'Peak RSS is whole-process memory, not live allocations; without a launcher it can include inherited Python memory.',
                 'No CPU/frequency isolation; other host activity can affect samples.',
                 'The CPU-selected path may fall back when the host lacks the requested features.',
                 'The VM executes the small portable case only; other hardware remains unmeasured.'])
    metadata['source_hashes']['bench/cpu_features.py'] = digest(Path(__file__))
    rows = []

    def invoke(command, phase, case, warmup=False, expected=None):
        argv = [str(p) for p in command]
        rss_file = output/'resource-rss.txt'
        measured = [*launcher, '-f', '%M', '-o', str(rss_file), *argv] if launcher else argv
        with tempfile.TemporaryFile() as stdout, tempfile.TemporaryFile() as stderr:
            begin = time.perf_counter_ns()
            child = subprocess.Popen(measured, cwd=ROOT, env=env, stdout=stdout, stderr=stderr)
            _, status, usage = os.wait4(child.pid, 0)
            child.returncode = os.waitstatus_to_exitcode(status)
            elapsed = (time.perf_counter_ns() - begin) / 1e6
            stdout.seek(0); stderr.seek(0)
            text = stdout.read().decode(errors='replace').strip()
            errors = stderr.read().decode(errors='replace')
        rss = usage.ru_maxrss / 1024 if platform.system() == 'Darwin' else usage.ru_maxrss
        row = dict(phase=phase, case=case, warmup=warmup, argv=argv, elapsed_ms=elapsed,
                   user_ms=usage.ru_utime * 1000, system_ms=usage.ru_stime * 1000,
                   max_rss_kib=int(rss_file.read_text().strip()) if launcher and child.returncode == 0 else rss,
                   returncode=child.returncode, stdout=text, stderr=errors)
        with (output/'samples.jsonl').open('a') as log: log.write(json.dumps(row)+'\n')
        if child.returncode: raise RuntimeError(f'{case}: {errors}')
        if expected is not None and text != str(expected): raise AssertionError(f'{case}: {text!r} != {expected}')
        if not warmup: rows.append(row)

    def repeated(command, case):
        for repetition in range(args.compile_repetitions + 1):
            invoke(command, 'compile', case, warmup=repetition == 0)

    entry = output/'counts.zi'; entry.write_text(source(args.small))
    repeated([binaries/'zi2zir', '--check-only', '--root', output, entry], 'Ziran check')
    repeated([binaries/'zi2zir', '--root', output, '-o', output/'ir', entry], 'Ziran save IR')
    native = []
    for form, root, file in [('source', output, entry), ('saved', output/'ir', output/'ir/counts.zir')]:
        generated = output/form
        repeated([binaries/'zi2c', '--root', root, '-o', generated, file], f'{form} emit C')
        repeated([binaries/'zi2zib', 'bundle', '--root', root, '--entry', 'counts:Answer',
                  '-o', output/f'{form}.zib', file], f'{form} bundle')
        host = generated/'host.c'
        host.write_text('#include "counts.h"\n#include <stdio.h>\n#include <stdlib.h>\n'
            f'int main(int argc,char **argv){{if(argc!=2)return 1;printf("%llu\\n",'
            f'(unsigned long long)Kernel(strtoll(argv[1],0,10),{SEED}ULL));return 0;}}\n')
        for mode, flags in [('selected', []), ('fallback', ['-DZIRAN_NO_ASM'])]:
            binary = generated/mode
            repeated([cc, '-O2', '-std=c11', *flags, '-I', generated, '-I', ROOT/'include',
                      generated/'counts.c', host, '-o', binary], f'{form} C compile {mode}')
            native.append((f'Ziran {form} {mode}', binary))
    for name in ['counts.c', 'counts.h']:
        assert (output/'source'/name).read_bytes() == (output/'saved'/name).read_bytes()
    assert (output/'source.zib').read_bytes() == (output/'saved.zib').read_bytes()
    reference = output/'reference.c'
    reference.write_text('''#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(__aarch64__) && defined(__linux__)
#include <sys/auxv.h>
#include <asm/hwcap.h>
#endif
static int available(void) {
#if defined(__x86_64__) || defined(__i386__)
    __builtin_cpu_init(); return __builtin_cpu_supports("popcnt") != 0;
#elif defined(__aarch64__) && defined(__linux__)
    return (getauxval(AT_HWCAP) & HWCAP_ASIMD) != 0;
#else
    return 0;
#endif
}
static uint64_t portable(int64_t n,uint64_t value) {
    uint64_t total=0;
    for(int64_t i=0;i<n;i++) {
        value^=value<<13;value^=value>>7;value^=value<<17;
        uint64_t bits=value-((value>>1)&UINT64_C(0x5555555555555555));
        bits=(bits&UINT64_C(0x3333333333333333))+((bits>>2)&UINT64_C(0x3333333333333333));
        total+=(((bits+(bits>>4))&UINT64_C(0x0f0f0f0f0f0f0f0f))*UINT64_C(0x0101010101010101))>>56;
    }
    return total;
}
#if defined(__x86_64__) || defined(__i386__)
__attribute__((noinline,target("popcnt")))
#elif defined(__aarch64__) && defined(__linux__)
__attribute__((noinline,target("+simd")))
#endif
static uint64_t selected(int64_t n,uint64_t value) {
    uint64_t total=0;
    for(int64_t i=0;i<n;i++) {
        value^=value<<13;value^=value>>7;value^=value<<17;
        total+=(uint64_t)__builtin_popcountll(value);
    }
    return total;
}
int main(int argc,char **argv) {
    if(argc!=2)return 1;
    if(!strcmp(argv[1],"--features")){printf("%d\\n",available());return 0;}
    int64_t n=strtoll(argv[1],0,10);
    printf("%llu\\n",(unsigned long long)(FORCE_FALLBACK || !available() ? portable(n,SEED) : selected(n,SEED)));
    return 0;
}
'''.replace('SEED', str(SEED)+'ULL'))
    for mode, flag in [('selected', '0'), ('fallback', '1')]:
        binary = output/f'c-{mode}'
        repeated([cc, '-O2', '-std=c11', '-DFORCE_FALLBACK='+flag, reference, '-o', binary], f'C compile {mode}')
        native.append((f'handwritten C {mode}', binary))
    metadata['host_optional_feature_available'] = subprocess.check_output([output/'c-selected', '--features'], text=True).strip() == '1'
    rng = random.Random(20261015)
    for n in [-1, 0, 1, args.small, args.large]:
        expected = oracle(n)
        commands = [(name, [binary, str(n)], expected) for name, binary in native]
        if n == args.small:
            commands += [(f'Ziran {form} VM', [binaries/'zi2zib', 'run', output/f'{form}.zib'], expected)
                         for form in ['source', 'saved']]
        for repetition in range(args.run_repetitions + 1):
            rng.shuffle(commands)
            for name, command, answer in commands:
                invoke(command, f'run n={n}', name, warmup=repetition == 0, expected=answer)
        print(f'validated n={n}', flush=True)
    groups = {}
    for row in rows: groups.setdefault((row['phase'], row['case']), []).append(row)
    summary = [dict(phase=phase, case=case, samples=len(samples),
                    **{f'median_{metric}': statistics.median(r[metric] for r in samples)
                       for metric in ['elapsed_ms', 'user_ms', 'system_ms', 'max_rss_kib']},
                    min_elapsed_ms=min(r['elapsed_ms'] for r in samples),
                    max_elapsed_ms=max(r['elapsed_ms'] for r in samples))
               for (phase, case), samples in groups.items()]
    metadata['artifacts'] = {str(p.relative_to(output)): digest(p) for p in output.rglob('*')
                              if p.is_file() and p.suffix not in ['.json', '.jsonl']}
    metadata.update(head_after=git('rev-parse', 'HEAD'), status_after=git('status', '--short'))
    (output/'metadata.json').write_text(json.dumps(metadata, indent=2)+'\n')
    (output/'summary.json').write_text(json.dumps(summary, indent=2)+'\n')
    print(f'{len(rows)} validated measured samples: {output}', flush=True)


if __name__ == '__main__': main()
