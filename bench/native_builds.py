#!/usr/bin/env python3
"""Compare complete native builds of equivalent modular Ziran, C and Go programs."""
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
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
MASK = (1 << 64) - 1
SEED = 0x123456789ABCDEF
INCREMENT = 1442695040888963407


def oracle(modules, rounds, leaf=0, shared=0):
    value = SEED
    for _ in range(rounds):
        for index in range(modules):
            value ^= value >> (index % 7 + 1)
            value = (value * (48271 + index * 2) + INCREMENT + shared +
                     (leaf if index == modules - 1 else 0)) & MASK
    return value


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--bin-dir', type=Path, default=ROOT/'build/bin')
    parser.add_argument('--before-bin-dir', type=Path,
                        help='also measure a preserved Ziran launcher/compiler on the same sources')
    parser.add_argument('--sizes', type=int, nargs='+', default=[1, 16, 64])
    parser.add_argument('--build-repetitions', type=int, default=5)
    parser.add_argument('--run-repetitions', type=int, default=9)
    parser.add_argument('--work', type=int, default=20000000, help='approximately this many module calls per large run')
    parser.add_argument('--jobs', type=int, default=4, help='C make/Go concurrency cap; Ziran uses its own driver behavior')
    parser.add_argument('--cflags', default='-O2 -std=c11', help='same C compiler flags for Ziran and C controls')
    args = parser.parse_args()
    if (not args.sizes or len(set(args.sizes)) != len(args.sizes) or
            any(n < 1 or n > 128 for n in args.sizes) or
            min(args.build_repetitions, args.run_repetitions, args.work, args.jobs) < 1):
        parser.error('unique sizes 1..128 and positive work/repetitions/jobs required')
    output = args.out.resolve()
    if output.exists(): parser.error('archive needed receipts and remove inactive outputs before reusing --out')
    tools = {name: shutil.which(name) for name in ['cc', 'go', 'make']}
    if not all(tools.values()) or not hasattr(os, 'wait4'):
        parser.error('requires cc, go, make and POSIX wait4')
    binary = args.bin_dir.resolve()
    if not (binary/'ziran').is_file() or not (binary/'zi2c').is_file():
        parser.error('build the native Ziran tools first')
    before_binary = args.before_bin_dir.resolve() if args.before_bin_dir else None
    if before_binary and any(not (before_binary/name).is_file() for name in ('ziran','zi2c')):
        parser.error('preserve both ziran and zi2c for the before variant')
    output.mkdir(parents=True)
    env = dict(os.environ)
    for name in ['DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'DBUS_SESSION_BUS_ADDRESS',
                 'ZIRAN_PROFILE', 'ZIRAN_VM_PROFILE', 'MAKEFLAGS', 'MFLAGS']:
        env.pop(name, None)
    env.update(CC=tools['cc'], CFLAGS=args.cflags, LDFLAGS='', LDLIBS='',
               GOCACHE=str(output/'go-cache'), GOTOOLCHAIN='local', GOPROXY='off',
               GOSUMDB='off', GOWORK='off', GOFLAGS='', CGO_ENABLED='0', GOMAXPROCS=str(args.jobs))
    launcher = []
    if tool := shutil.which('time'):
        version = subprocess.run([tool, '--version'], capture_output=True, text=True)
        if version.returncode == 0 and 'GNU' in version.stdout + version.stderr: launcher = [tool]
    if not launcher and (tool := shutil.which('busybox')):
        result = subprocess.run([tool, 'time', '--help'], capture_output=True, text=True)
        if '-f FMT' in result.stdout + result.stderr: launcher = [tool, 'time']
    digest = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
    git = lambda *a: subprocess.check_output(['git', *a], cwd=ROOT, text=True).strip()
    metadata = dict(schema_version=1, created_utc=time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()),
                    head=git('rev-parse', 'HEAD'), status=git('status', '--short'),
                    platform=platform.platform(), machine=platform.machine(), jobs=args.jobs,
                    cflags=args.cflags,
                    cpu_affinity=sorted(os.sched_getaffinity(0)) if hasattr(os, 'sched_getaffinity') else None,
                    versions={n: subprocess.check_output([p, '--version' if n != 'go' else 'version'], text=True)
                              for n, p in tools.items()},
                    binaries={name: digest(binary/name) for name in ['ziran', 'zi2c']},
                    source_hashes={str(p.relative_to(ROOT)): digest(p) for folder in ['cmd', 'include', 'std']
                                   for p in (ROOT/folder).rglob('*') if p.is_file() and p.suffix in ['.c', '.h', '.zi']},
                    generator_sha256=digest(Path(__file__)), measurement_launcher=launcher,
                    sizes=args.sizes, work=args.work, build_repetitions=args.build_repetitions,
                    run_repetitions=args.run_repetitions, cases=[],
                    caveats=['One host with warm filesystem caches; no CPU/frequency isolation.',
                             'Fresh builds remove fixture outputs and change comments in every application source to miss Go application caches; toolchain/standard-library caches remain warm.',
                             'Unchanged builds preserve all inputs/outputs. Leaf/shared edits use unique values, preventing reuse of an earlier identical Go application build.',
                             'C uses make dependency files and -j; Go uses -p and GOMAXPROCS; Ziran uses its native --exe driver without added parallelism.',
                             'Peak RSS is the maximum reported child/process RSS, not the sum of concurrent compiler memory.',
                             'C uses dynamic system libraries; Go uses its normal static runtime. Binary sizes are not equivalent packaging costs.',
                             'Every built binary runs an independently calculated small oracle. Runtime samples include startup.'])
    if before_binary:
        metadata['before_binaries'] = {name:digest(before_binary/name) for name in ('ziran','zi2c')}
        metadata['caveats'].append('Ziran before/after share source files and flags, use separate output directories, and are interleaved with C and Go. Output identity is recorded, not required for general compiler comparisons.')
    (output/'metadata.json').write_text(json.dumps(metadata, indent=2)+'\n')
    samples = []

    def invoke(command, case, language, phase, repetition, cwd, expected=None):
        argv = [str(v) for v in command]
        resources = output/'resources.txt'
        measured = [*launcher, '-f', '%M', '-o', str(resources), *argv] if launcher else argv
        with tempfile.TemporaryFile() as stdout, tempfile.TemporaryFile() as stderr:
            started = time.perf_counter_ns()
            child = subprocess.Popen(measured, cwd=cwd, env=env, stdout=stdout, stderr=stderr)
            _, status, usage = os.wait4(child.pid, 0)
            child.returncode = os.waitstatus_to_exitcode(status)
            elapsed = (time.perf_counter_ns() - started) / 1e6
            stdout.seek(0); stderr.seek(0)
            text = stdout.read().decode(errors='replace').strip()
            errors = stderr.read().decode(errors='replace')
        rss = int(resources.read_text().splitlines()[-1]) if launcher else usage.ru_maxrss
        row = dict(case=case, language=language, phase=phase, repetition=repetition,
                   warmup=repetition==0, measured=repetition>0,
                   argv=argv, cwd=str(cwd), elapsed_ms=elapsed, user_ms=usage.ru_utime*1000,
                   system_ms=usage.ru_stime*1000, max_rss_kib=rss, stdout=text, stderr=errors,
                   returncode=child.returncode)
        with (output/'samples.jsonl').open('a') as log: log.write(json.dumps(row)+'\n')
        if child.returncode: raise RuntimeError(f'{case}/{language}/{phase}: {errors}')
        if expected is not None and text != str(expected):
            raise AssertionError(f'{case}/{language}/{phase}: expected {expected}, got {text!r}')
        if repetition > 0: samples.append(row)

    rng = random.Random(20261015)
    for size in args.sizes:
        folder = output/f'modules-{size}'
        roots = {name: folder/name for name in ['ziran', 'c', 'go']}
        for p in roots.values(): p.mkdir(parents=True)
        if before_binary: roots['ziran-before'] = roots['ziran']
        build_dirs = {name: path/('build-before' if name=='ziran-before' else 'build') for name,path in roots.items()}
        go_root = roots['go']
        (go_root/'go.mod').write_text('module example.invalid/nativebench\ngo 1.20\n')
        (go_root/'settings').mkdir()
        for index in range(size): (go_root/f'm{index}').mkdir()
        commands = {
            'ziran': [binary/'ziran', 'build', '--target=c', '--exe', '--entry', 'app:main',
                      '--root', roots['ziran'], '-o', roots['ziran']/'build', roots['ziran']/'app.zi'],
            'c': [tools['make'], '--no-print-directory', '-s', f'-j{args.jobs}'],
            'go': [tools['go'], 'build', f'-p={args.jobs}', '-o', 'build/app', '.'],
        }
        if before_binary:
            commands['ziran-before'] = [before_binary/'ziran', *commands['ziran'][1:]]
            commands['ziran-before'][commands['ziran-before'].index('-o')+1] = build_dirs['ziran-before']
        executables = {name: path/'app' for name,path in build_dirs.items()}
        c_calls = ''.join(f'value=step_{i}(value);' for i in range(size))
        zi_calls = '\n'.join(f'value = M{i}.Step(value)' for i in range(size))
        go_calls = ''.join(f'value=m{i}.Step(value);' for i in range(size))

        def write_inputs(leaf=0, shared=0, nonce='base'):
            texts = {n: {} for n in roots}
            texts['ziran']['settings.zi'] = f'Increment :: {INCREMENT+shared};\n'
            texts['c']['settings.h'] = f'#define INCREMENT UINT64_C({INCREMENT+shared})\n'
            texts['c']['steps.h'] = '#include <stdint.h>\n'+''.join(f'uint64_t step_{i}(uint64_t);\n' for i in range(size))
            texts['go']['settings/settings.go'] = f'package settings\nconst Increment uint64 = {INCREMENT+shared}\n'
            for i in range(size):
                delta = leaf if i == size-1 else 0
                shift, multiplier = i%7+1, 48271+i*2
                texts['ziran'][f'm{i}.zi'] = (f'Settings :: #import "settings";\nStep :: (value: u64) -> u64 {{\n'
                                             f'return (value ^ (value >> {shift})) * {multiplier} + Settings.Increment + {delta}\n}}\n')
                texts['c'][f'm{i}.c'] = (f'#include "steps.h"\n#include "settings.h"\nuint64_t step_{i}(uint64_t value) {{'
                                        f'return (value^(value>>{shift}))*UINT64_C({multiplier})+INCREMENT+UINT64_C({delta});}}\n')
                texts['go'][f'm{i}/step.go'] = (f'package m{i}\nimport "example.invalid/nativebench/settings"\n'
                                              f'func Step(value uint64) uint64 {{return (value^(value>>{shift}))*{multiplier}+settings.Increment+{delta}}}\n')
            imports = '\n'.join(f'M{i} :: #import "m{i}";' for i in range(size))
            texts['ziran']['app.zi'] = (f'{imports}\nlibc :: #system_library "libc";\n'
                                      'Parse :: (text: *u8) -> s32 #foreign libc "atoi";\n'
                                      f'Kernel :: (n: s64) -> u64 {{\nvalue: u64 = {SEED}\nfor 0..n-1 {{\n{zi_calls}\n}}\nreturn value\n}}\n'
                                      '#program_export\nmain :: (argc: s32, argv: **u8) -> s32 {\nif argc!=2 {return 2}\nprint("%\\n",Kernel(cast(s64)(Parse(argv[1]))))\nreturn 0\n}\n')
            texts['c']['app.c'] = ('#include "steps.h"\n#include <stdio.h>\n#include <stdlib.h>\n'
                                  f'int main(int argc,char **argv){{if(argc!=2)return 2;uint64_t value=UINT64_C({SEED});'
                                  f'long n=strtol(argv[1],0,10);for(long i=0;i<n;i++){{{c_calls}}}'
                                  'printf("%llu\\n",(unsigned long long)value);return 0;}\n')
            go_imports = ';'.join(f'"example.invalid/nativebench/m{i}"' for i in range(size))
            texts['go']['main.go'] = ('package main\nimport("fmt";"os";"strconv";'+go_imports+')\n'
                                    f'func main(){{n,e:=strconv.Atoi(os.Args[1]);if e!=nil{{panic(e)}};value:=uint64({SEED});'
                                    f'for i:=0;i<n;i++{{{go_calls}}};fmt.Println(value)}}\n')
            for language, files in texts.items():
                for name, text in files.items():
                    path = roots[language]/name
                    content = text+f'// build input modules-{size}-{nonce}\n'
                    if not path.exists() or path.read_text() != content: path.write_text(content)

        objects = ' '.join(f'build/m{i}.o' for i in range(size))+' build/app.o'
        (roots['c']/'Makefile').write_text(f'objects := {objects}\nall: build/app\nbuild/app: $(objects)\n\t$(CC) $(CFLAGS) $(objects) -o $@\n'
                                            'build/%.o: %.c\n\t@mkdir -p build\n\t$(CC) $(CFLAGS) -MMD -MP -c $< -o $@\n-include $(objects:.o=.d)\n')
        small, large = 3, max(1, args.work//size)
        expected_large = oracle(size, large)
        write_inputs()
        languages = list(roots)
        for language in languages:
            invoke(commands[language], size, language, 'toolchain-warmup', -1, roots[language])
            invoke([executables[language], small], size, language, 'validate', -1, roots[language], oracle(size, small))
        for phase in ['fresh', 'unchanged', 'leaf-edit', 'shared-edit']:
            write_inputs(nonce=phase)
            for language in languages: invoke(commands[language], size, language, 'phase-setup', -1, roots[language])
            for repetition in range(args.build_repetitions+1):
                leaf = repetition+1 if phase=='leaf-edit' else 0
                shared = repetition+1 if phase=='shared-edit' else 0
                write_inputs(leaf, shared, f'{phase}-{repetition}' if phase=='fresh' else phase)
                rng.shuffle(languages)
                expected = oracle(size, small, leaf, shared)
                for language in languages:
                    if phase=='fresh' and build_dirs[language].exists(): shutil.rmtree(build_dirs[language])
                    invoke(commands[language], size, language, phase, repetition, roots[language])
                    invoke([executables[language], small], size, language, 'validate', -1, roots[language], expected)
            print(f'validated modules{size}/{phase}', flush=True)
        write_inputs()
        for language in languages: invoke(commands[language], size, language, 'runtime-setup', -1, roots[language])
        for repetition in range(args.run_repetitions+1):
            rng.shuffle(languages)
            for language in languages:
                invoke([executables[language], large], size, language, 'run', repetition, roots[language], expected_large)
        identity = {}
        if before_binary:
            generated = lambda directory: {str(p.relative_to(directory)):p.read_bytes() for p in directory.rglob('*') if p.suffix in ('.c','.h')}
            identity = dict(generated_c=generated(build_dirs['ziran-before'])==generated(build_dirs['ziran']),
                            executable=executables['ziran-before'].read_bytes()==executables['ziran'].read_bytes())
        metadata['cases'].append(dict(modules=size,small=small,large=large,expected_large=expected_large,
                                      binary_bytes={n:p.stat().st_size for n,p in executables.items()},
                                      ziran_output_identity=identity,
                                      fixture_hashes={str(p.relative_to(folder)):digest(p) for r in dict.fromkeys(roots.values()) for p in r.rglob('*')
                                                      if p.is_file() and not {'build','build-before'} & set(p.relative_to(r).parts)}))
    groups = {}
    for row in samples: groups.setdefault((row['case'],row['language'],row['phase']),[]).append(row)
    summary = [dict(modules=key[0],language=key[1],phase=key[2],samples=len(rows),
                    **{f'median_{name}':statistics.median(r[name] for r in rows)
                       for name in ['elapsed_ms','user_ms','system_ms','max_rss_kib']}) for key,rows in groups.items()]
    metadata.update(head_after=git('rev-parse','HEAD'),status_after=git('status','--short'))
    (output/'metadata.json').write_text(json.dumps(metadata,indent=2)+'\n')
    (output/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(f'{len(samples)} validated measured samples: {output}',flush=True)


if __name__ == '__main__': main()
