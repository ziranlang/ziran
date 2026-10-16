#!/usr/bin/env python3
"""Measure native Rust assembly against checked fallbacks and native controls."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import random
import resource
import shutil
import statistics
import subprocess
import time

from cpu_features import ROOT, SEED, oracle, source


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--bin-dir', type=Path, default=ROOT / 'build/bin')
    parser.add_argument('--baseline-bin-dir', type=Path)
    parser.add_argument('--generic', action='store_true', help='instantiate assembly kernels through type parameters')
    parser.add_argument('--alternatives', action='store_true', help='use paired GNU register-constraint alternatives')
    parser.add_argument('--small', type=int, default=1000)
    parser.add_argument('--large', type=int, default=20000000)
    parser.add_argument('--repetitions', type=int, default=9)
    args = parser.parse_args()
    if args.generic and args.baseline_bin_dir:
        parser.error('use separate concrete and generic runs; older compilers can reject generic assembly')
    if platform.machine() not in ('x86_64', 'aarch64'):
        parser.error('requires an x86_64 or aarch64 host')
    if not 0 < args.small < args.large or args.repetitions < 1:
        parser.error('requires positive repetitions and 0 < small < large')
    out = args.out.resolve()
    if out.exists(): parser.error('output exists; preserve receipts before reusing inactive outputs')
    out.mkdir(parents=True)
    env = dict(os.environ)
    for key in ('DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'DBUS_SESSION_BUS_ADDRESS', 'RUSTFLAGS'):
        env.pop(key, None)
    native_flags = ['-Copt-level=3']
    c_flags = ['-O3']
    if platform.machine() == 'x86_64':
        native_flags += ['-Ctarget-cpu=x86-64', '-Ctarget-feature=-popcnt,-avx,-bmi2']
        c_flags += ['-march=x86-64', '-mno-popcnt', '-mno-avx', '-mno-bmi2']
    samples, builds, artifacts, lowering = [], [], [], []
    timer = []
    if executable := shutil.which('time'):
        version = subprocess.run([executable, '--version'], capture_output=True, text=True)
        if version.returncode == 0 and 'GNU' in version.stdout + version.stderr:
            timer = [executable]
    if not timer and (executable := shutil.which('busybox')):
        help_result = subprocess.run([executable, 'time', '--help'], capture_output=True, text=True)
        if '-f FMT' in help_result.stdout + help_result.stderr and '-o FILE' in help_result.stdout + help_result.stderr:
            timer = [executable, 'time']
    if not timer: parser.error('requires GNU or BusyBox time for child peak RSS')

    def invoke(command, selected=env, expected=None, measured=False):
        started = time.perf_counter_ns()
        before = resource.getrusage(resource.RUSAGE_CHILDREN)
        argv = list(map(str, command))
        rss = out / 'rss.txt'
        result = subprocess.run(([*timer, '-f', '%M', '-o', str(rss)] if measured else []) + argv,
                                env=selected, capture_output=True, text=True, check=True, timeout=180)
        elapsed = (time.perf_counter_ns() - started) / 1e6
        after = resource.getrusage(resource.RUSAGE_CHILDREN)
        if expected is not None: assert result.stdout.strip() == str(expected), (argv, result.stdout, expected)
        return dict(argv=argv, wall_ms=elapsed,
                    user_s=after.ru_utime-before.ru_utime,
                    system_s=after.ru_stime-before.ru_stime,
                    peak_rss_kib=int(rss.read_text()) if measured else None)

    def build(command, name, selected=env):
        record = invoke(command, selected, measured=True)
        builds.append(dict(case=name, **record))

    rust_host = '''use ziran_generated::Kernel;
fn main() { let n=std::env::args().nth(1).unwrap().parse::<i64>().unwrap(); println!("{}",Kernel(n,SEED)); }
'''.replace('SEED', str(SEED))
    c_host = '''#include <stdio.h>
#include <stdlib.h>
#include "kernel.h"
int main(int argc,char **argv){(void)argc;printf("%llu\\n",(unsigned long long)Kernel(strtoll(argv[1],0,10),SEEDULL));}
'''.replace('SEED', str(SEED))
    rust_control = '''fn main(){let n=std::env::args().nth(1).unwrap().parse::<i64>().unwrap();let mut value=SEEDu64;let mut total=0u64;for _ in 0..n{value^=value<<13;value^=value>>7;value^=value<<17;total=total.wrapping_add(value.count_ones() as u64);}println!("{}",total);}
'''.replace('SEED', str(SEED))
    c_control = '''#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
int main(int argc,char **argv){(void)argc;int64_t n=strtoll(argv[1],0,10);uint64_t value=SEEDULL,total=0;for(int64_t i=0;i<n;i++){value^=value<<13;value^=value>>7;value^=value<<17;total+=__builtin_popcountll(value);}printf("%llu\\n",(unsigned long long)total);}
'''.replace('SEED', str(SEED))
    cases = []
    for form in ('loop', 'calls'):
        work = out / form; work.mkdir()
        program = source(args.small)
        if form == 'calls':
            start = program.index(' MODIFIERS') if ' MODIFIERS' in program else program.index(' #asm(', program.index('Kernel ::'))
            end = program.index(' {', start)
            program = program[:start] + program[end:]
            modifiers = ' '.join('#asm(' + ', '.join(json.dumps(part) for part in parts) + ')' for parts in [
                ['x86_64+popcnt', 'popcnt %[value], %[out]', '=r', 'r', 'cc', ''],
                ['aarch64+simd', 'fmov d0, %[value]\n\tcnt v0.8b, v0.8b\n\taddv b0, v0.8b\n\tumov %w[out], v0.b[0]', '=r', 'r', 'v0', '']])
            program = program.replace('Count :: (value: u64) -> u64 {', 'Count :: (value: u64) -> u64 ' + modifiers + ' {')
        if args.generic:
            program = program.replace('Count :: (value: u64) -> u64', 'Count :: (value: $T) -> T')
            program = program.replace('#program_export\nKernel :: (rounds: s64, seed: u64) -> u64',
                                      'GenericKernel :: (rounds: $N, seed: $T) -> T')
            program = program.replace('total: u64', 'total: T').replace('i: s64', 'i: N')
            program += '\n#program_export\nKernel :: (rounds: s64, seed: u64) -> u64 { return GenericKernel(rounds, seed) }\n'
        if args.alternatives:
            # Both arms permit the same registers. This isolates the effect of
            # translating alternatives from instruction and allocation changes.
            previous = program
            program = program.replace(', "=&r", "r;r",', ', "=&r,&r", "r,r;r,r",')
            program = program.replace(', "=r", "r",', ', "=r,r", "r,r",')
            assert program != previous, 'assembly constraint shape changed'
        entry = work / 'kernel.zi'; entry.write_text(program)
        for compiler, bin_dir in [('current', args.bin_dir), ('before', args.baseline_bin_dir)]:
            if bin_dir is None: continue
            project = work / compiler
            build([bin_dir.resolve() / 'ziran', 'build', '--target=rust', '--root', work, '-o', project, entry], form+'-'+compiler+'-emit')
            (project / 'src/main.rs').write_text(rust_host)
            native = (project / 'src/lib.rs').read_text()
            lowering.append(dict(form=form, compiler=compiler,
                                 assembly_blocks=native.count('core::arch::asm!'),
                                 portable_variant_notices=native.count('portable body for this variant')))
            if compiler == 'current':
                assert 'core::arch::asm!' in native and 'portable body for this variant' not in native
            for mode in (['native', 'fallback'] if compiler == 'current' else ['baseline']):
                selected = dict(env, CARGO_TARGET_DIR=str(work/'cargo-target'), RUSTFLAGS=' '.join(native_flags + (['--cfg ziran_no_asm'] if mode == 'fallback' else [])))
                name = form+'-rust-'+compiler+'-'+mode
                build(['cargo','build','--release','--quiet','--offline','--manifest-path',project/'Cargo.toml'],name,selected)
                binary = work/name; shutil.copy2(work/'cargo-target/release/ziran_generated',binary)
                cases.append((name,binary))
        c_out = work/'c'
        build([args.bin_dir.resolve()/'ziran','build','--target=c','--root',work,'-o',c_out,entry],form+'-c-emit')
        (c_out/'host.c').write_text(c_host)
        for mode in ('native','fallback'):
            name=form+'-c-'+mode; binary=work/name
            build(['cc',*c_flags,*(['-DZIRAN_NO_ASM'] if mode=='fallback' else []),'-I'+str(ROOT/'include'),'-I'+str(c_out),c_out/'kernel.c',c_out/'host.c','-o',binary],name)
            cases.append((name,binary))
        bundle = work / 'portable.zib'
        build([args.bin_dir.resolve()/'ziran','bundle','--root',work,'--entry','kernel:Answer','-o',bundle,entry],form+'-vm-bundle')
        invoke([args.bin_dir.resolve()/'ziran','run',bundle],expected=oracle(args.small))
    for language, text in [('rust',rust_control),('c',c_control)]:
        entry=out/('control.rs' if language=='rust' else 'control.c');entry.write_text(text)
        binary=out/(language+'-control')
        build((['rustc',*native_flags,entry,'-o',binary] if language=='rust' else ['cc',*c_flags,entry,'-o',binary]),language+'-control')
        cases.append((language+'-control',binary))
    expected={n:oracle(n) for n in (args.small,args.large)}
    for name,binary in cases:
        for n in expected: invoke([binary,n],expected=expected[n])
        artifacts.append(dict(case=name,bytes=binary.stat().st_size,sha256=hashlib.sha256(binary.read_bytes()).hexdigest()))
    trials=[(name,binary,n) for name,binary in cases for n in expected for _ in range(args.repetitions)]
    random.Random(42).shuffle(trials)
    for name,binary,n in trials:
        samples.append(dict(case=name,rounds=n,**invoke([binary,n],expected=expected[n],measured=True)))
    summary=[]
    for name,_ in cases:
        for n in expected:
            selected=[s for s in samples if s['case']==name and s['rounds']==n]
            summary.append(dict(case=name,rounds=n,
                                median_ms=statistics.median(s['wall_ms'] for s in selected),
                                median_cpu_ms=statistics.median(1000*(s['user_s']+s['system_s']) for s in selected),
                                median_peak_rss_kib=statistics.median(s['peak_rss_kib'] for s in selected)))
    compilers = {}
    for name, folder in [('current', args.bin_dir), ('before', args.baseline_bin_dir)]:
        if folder is not None:
            compilers[name] = {tool: hashlib.sha256((folder / tool).read_bytes()).hexdigest()
                               for tool in ('ziran', 'zi2rust')}
    metadata = dict(
        platform=platform.platform(), machine=platform.machine(), expected=expected, generic=args.generic,
        alternatives=args.alternatives,
        lowering=lowering,
        rustc=subprocess.check_output(['rustc', '--version'], text=True).strip(),
        cargo=subprocess.check_output(['cargo', '--version'], text=True).strip(),
        cc=subprocess.check_output(['cc', '--version'], text=True).splitlines()[0],
        compilers=compilers,
        benchmark_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        oracle_sha256=hashlib.sha256(Path(__file__).with_name('cpu_features.py').read_bytes()).hexdigest(),
        c_flags=c_flags, rust_flags=native_flags, repetitions=args.repetitions,
        discarded_warmups_per_case_and_size=1, timer=timer,
        caveats=[
            'One xorshift/popcount workload on this host, without CPU isolation.',
            'Whole-process wall/CPU time includes startup and the measurement launcher; peak RSS is not live allocated memory.',
            'Builds are single observations with warmed toolchains; they are not a compiler-speed ranking.',
            'The before compiler uses its supported lowering; recorded assembly counts identify native/fallback differences. Native asm can lose to optimized portable code, especially with per-call dispatch.',
            'Rust includes its standard library in each executable; C controls link libc dynamically. Artifact sizes are not pure kernel code sizes.'],
        sources={str(p.relative_to(out)): hashlib.sha256(p.read_bytes()).hexdigest()
                 for p in out.rglob('*') if p.is_file() and p.suffix in ('.zi', '.rs', '.c') and 'cargo-target' not in p.parts})
    (out/'results.json').write_text(json.dumps(dict(metadata=metadata,summary=summary,samples=samples,builds=builds,artifacts=artifacts),indent=2)+'\n')
    print('Rust assembly benchmark completed; every sample matched the independent integer oracle.')


if __name__ == '__main__': main()
