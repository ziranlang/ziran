#!/usr/bin/env python3
"""Validate and measure assembly, portable fallback, C, Go, and the .zx VM."""
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
MULTIPLIER = 6364136223846793005
INCREMENT = 1442695040888963407


def oracle(n):
    value = SEED
    for i in range(n):
        value = int.from_bytes((value ^ i).to_bytes(8, 'little'), 'big')
        value = (value * MULTIPLIER + INCREMENT) & MASK
    return value


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--bin-dir', type=Path, default=ROOT / 'build/bin')
    parser.add_argument('--compile-repetitions', type=int, default=3)
    parser.add_argument('--run-repetitions', type=int, default=9)
    parser.add_argument('--small', type=int, default=1000)
    parser.add_argument('--large', type=int, default=2000000)
    args = parser.parse_args()
    if min(args.small, args.compile_repetitions, args.run_repetitions) < 1 or args.large <= args.small:
        parser.error('positive repetitions and 0 < small < large required')
    output = args.out.resolve()
    if output.exists():
        parser.error('output exists; archive needed receipts and remove inactive outputs before reusing it')
    cc = shutil.which(os.environ.get('CC', 'cc'))
    if not cc or not hasattr(os, 'wait4'):
        parser.error('requires a C compiler and POSIX wait4 resource accounting')
    output.mkdir(parents=True)
    binaries = args.bin_dir.resolve()
    env = dict(os.environ)
    for key in ['DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'DBUS_SESSION_BUS_ADDRESS', 'ZIRAN_PROFILE']:
        env.pop(key, None)
    env['GOCACHE'] = str(output / 'go-cache')
    env['GO111MODULE'] = 'off'
    samples = []
    time_launcher = []
    time_tool = shutil.which('time')
    if time_tool:
        version = subprocess.run([time_tool, '--version'], capture_output=True, text=True)
        if version.returncode == 0 and 'GNU' in version.stdout + version.stderr:
            time_launcher = [time_tool]
    if not time_launcher and (busybox := shutil.which('busybox')):
        help_result = subprocess.run([busybox, 'time', '--help'], capture_output=True, text=True)
        help_text = help_result.stdout + help_result.stderr
        if '-f FMT' in help_text and '-o FILE' in help_text:
            time_launcher = [busybox, 'time']

    def digest(path):
        return hashlib.sha256(path.read_bytes()).hexdigest()

    def git(*argv):
        return subprocess.check_output(['git', *argv], cwd=ROOT, text=True, env=env).strip()

    metadata = dict(schema_version=1, head=git('rev-parse', 'HEAD'), status=git('status', '--short'),
                    created_utc=time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()),
                    platform=platform.platform(), machine=platform.machine(),
                    input=dict(small=args.small, large=args.large, seed=SEED),
                    compile_repetitions=args.compile_repetitions, run_repetitions=args.run_repetitions,
                    discarded_warmups=1, cc=subprocess.check_output([cc, '--version'], text=True),
                    tools={name: digest(binaries / name) for name in ['zi2zir', 'zi2c', 'zi2zx']},
                    source_hashes={str(path.relative_to(ROOT)): digest(path)
                                   for folder in ['cmd', 'std', 'include']
                                   for path in (ROOT / folder).rglob('*')
                                   if path.is_file() and path.suffix in ['.c', '.h', '.zi']},
                    rss_measurement='time child peak RSS' if time_launcher else 'wait4 peak RSS including inherited high-water marks',
                    measurement_launcher=time_launcher,
                    caveats=['One byte-swap recurrence on this host; not a language ranking.',
                             'Whole-process times include startup and, when available, a GNU/BusyBox time launcher for reliable child RSS.',
                             'User/system CPU includes the measurement launcher; peak RSS is not live allocated bytes.',
                             'Go compilation uses a warm standard-library build cache after a discarded warmup.',
                             'Source-to-native total times below sum separately measured phases, not a single build.',
                             'No CPU isolation; concurrent host work can affect samples.',
                             'An assembly instruction appearing in the binary is checked only for x86_64 hosts.',
                             'The VM uses the small input; its portable body executes no machine assembly.'])
    metadata['source_hashes']['bench/assembly.py'] = digest(Path(__file__))
    (output / 'metadata.json').write_text(json.dumps(metadata, indent=2) + '\n')

    def invoke(command, phase, case, warmup=False, expected=None):
        argv = [str(part) for part in command]
        measured_argv = argv
        resources = output / 'resource-rss.txt'
        if time_launcher:
            measured_argv = [*time_launcher, '-f', '%M', '-o', str(resources), *argv]
        with tempfile.TemporaryFile() as stdout, tempfile.TemporaryFile() as stderr:
            started = time.perf_counter_ns()
            child = subprocess.Popen(measured_argv, cwd=ROOT, env=env, stdout=stdout, stderr=stderr)
            _, status, usage = os.wait4(child.pid, 0)
            child.returncode = os.waitstatus_to_exitcode(status)
            elapsed = (time.perf_counter_ns() - started) / 1e6
            stdout.seek(0); stderr.seek(0)
            result = stdout.read().decode(errors='replace').strip()
            errors = stderr.read().decode(errors='replace')
        rss = usage.ru_maxrss / 1024 if platform.system() == 'Darwin' else usage.ru_maxrss
        if time_launcher and child.returncode == 0:
            rss = int(resources.read_text().strip())
        record = dict(phase=phase, case=case, argv=argv, warmup=warmup,
                      elapsed_ms=elapsed, user_s=usage.ru_utime, system_s=usage.ru_stime,
                      max_rss_kib=rss, stdout=result, stderr=errors, returncode=child.returncode)
        with (output / 'samples.jsonl').open('a') as log:
            log.write(json.dumps(record) + '\n')
        if child.returncode:
            raise RuntimeError(f'{case}: {errors}')
        if expected is not None and result != str(expected):
            raise AssertionError(f'{case}: expected {expected}, got {result!r}')
        if not warmup:
            samples.append(record)

    def repeated(command, phase, case):
        for repetition in range(args.compile_repetitions + 1):
            invoke(command, phase, case, warmup=repetition == 0)

    source = output / 'swap.zi'
    source.write_text('''Swap :: (value: u64) -> u64 #asm("x86_64", "bswap %[out]", "=r", "0", "", "") #asm("aarch64", "rev %[out], %[value]", "=r", "r", "", "") {
    return ((value & 255) << 56) | ((value & 65280) << 40) |
           ((value & 16711680) << 24) | ((value & 4278190080) << 8) |
           ((value >> 8) & 4278190080) | ((value >> 24) & 16711680) |
           ((value >> 40) & 65280) | ((value >> 56) & 255)
}
#program_export
Kernel :: (n: s64, seed: u64) -> u64 {
    value := seed
    i: s64 = 0
    while i < n {
        value = Swap(value ^ cast(u64)i) * 6364136223846793005 + 1442695040888963407
        i += 1
    }
    return value
}
#program_export
Answer :: () -> s64 { return cast(s64)(Kernel(SMALL, SEED) & 9223372036854775807) }
'''.replace('SMALL', str(args.small)).replace('SEED', str(SEED)))
    repeated([binaries/'zi2zir', '--check-only', '--root', output, source], 'compile', 'Ziran check')
    repeated([binaries/'zi2zir', '--root', output, '-o', output/'ir', source], 'compile', 'Ziran save IR')
    native = []
    for form, root, entry in [('source', output, source), ('saved', output/'ir', output/'ir/swap.zir')]:
        directory = output / form
        repeated([binaries/'zi2c', '--root', root, '-o', directory, entry], 'compile', f'Ziran {form} emit C')
        repeated([binaries/'zi2zx', 'bundle', '--root', root, '--entry', 'swap:Answer', '-o', output/f'{form}.zx', entry],
                 'compile', f'Ziran {form} bundle')
        host = directory/'host.c'
        host.write_text('#include "swap.h"\n#include <stdio.h>\n#include <stdlib.h>\n'
                        f'int main(int argc, char **argv) {{ if(argc != 2) return 1; '
                        f'printf("%llu\\n", (unsigned long long)Kernel(strtoll(argv[1], 0, 10), {SEED}ULL)); return 0; }}\n')
        for mode, defines in [('assembly', []), ('fallback', ['-DZIRAN_NO_ASM'])]:
            binary = directory / mode
            repeated([cc, '-O2', '-std=c11', *defines, '-I', directory, '-I', ROOT/'include',
                      directory/'swap.c', host, '-o', binary], 'compile', f'Ziran {form} C compile ({mode})')
            native.append((f'Ziran {form} {mode}', binary))
    for name in ['swap.c', 'swap.h']:
        if (output/'source'/name).read_bytes() != (output/'saved'/name).read_bytes():
            raise AssertionError('source and saved IR output differ')
    if (output/'source.zx').read_bytes() != (output/'saved.zx').read_bytes():
        raise AssertionError('source and saved IR bundles differ')
    reference = output/'reference.c'
    reference.write_text('#include <stdint.h>\n#include <stdio.h>\n#include <stdlib.h>\n'
                         f'int main(int argc, char **argv) {{ if(argc != 2) return 1; uint64_t value={SEED}ULL; '
                         f'int64_t n=strtoll(argv[1], 0, 10); for(int64_t i=0; i<n; i++) '
                         f'value=__builtin_bswap64(value^(uint64_t)i)*{MULTIPLIER}ULL+{INCREMENT}ULL; '
                         'printf("%llu\\n", (unsigned long long)value); return 0; }\n')
    repeated([cc, '-O2', '-std=c11', reference, '-o', output/'reference-c'], 'compile', 'handwritten C')
    native.append(('handwritten C', output/'reference-c'))
    go = shutil.which('go')
    if go:
        reference_go = output/'reference.go'
        reference_go.write_text('package main\nimport("fmt";"math/bits";"os";"strconv")\n'
                                f'func main(){{ n,err:=strconv.ParseInt(os.Args[1],10,64);if err!=nil{{panic(err)}};'
                                f'value:=uint64({SEED});for i:=int64(0);i<n;i++{{'
                                f'value=bits.ReverseBytes64(value^uint64(i))*{MULTIPLIER}+{INCREMENT}'
                                '};fmt.Println(value)}\n')
        repeated([go, 'build', '-o', output/'reference-go', reference_go], 'compile', 'handwritten Go (warm cache)')
        native.append(('handwritten Go', output/'reference-go'))
        metadata['go_version'] = subprocess.check_output([go, 'version'], text=True)
    else:
        metadata['go_version'] = 'unavailable'
    objdump = shutil.which('objdump')
    if objdump and platform.machine() == 'x86_64':
        assembly = subprocess.check_output([objdump, '-d', output/'source/assembly'], text=True)
        metadata['bswap_in_native_binary'] = 'bswap' in assembly
        if not metadata['bswap_in_native_binary']:
            raise AssertionError('native binary contains no bswap instruction')
    rng = random.Random(20261015)
    for n in [args.small, args.large]:
        expected = oracle(n)
        commands = [(name, [binary, str(n)], expected) for name, binary in native]
        if n == args.small:
            for form in ['source', 'saved']:
                commands.append((f'Ziran {form} VM fallback', [binaries/'zi2zx', 'run', output/f'{form}.zx'],
                                 expected & ((1 << 63) - 1)))
        for repetition in range(args.run_repetitions + 1):
            rng.shuffle(commands)
            for name, command, expected_result in commands:
                invoke(command, f'run n={n}', name, warmup=repetition == 0, expected=expected_result)
        print(f'validated n={n}', flush=True)
    groups = {}
    for record in samples:
        groups.setdefault((record['phase'], record['case']), []).append(record)
    summary = [dict(phase=phase, case=case, samples=len(rows),
                    **{f'median_{metric}': statistics.median(row[metric] for row in rows)
                       for metric in ['elapsed_ms', 'user_s', 'system_s', 'max_rss_kib']},
                    min_elapsed_ms=min(row['elapsed_ms'] for row in rows),
                    max_elapsed_ms=max(row['elapsed_ms'] for row in rows))
               for (phase, case), rows in groups.items()]
    metadata['artifacts'] = {str(path.relative_to(output)): digest(path)
                             for path in output.rglob('*') if path.is_file()
                             and 'go-cache' not in path.parts and path.suffix not in ['.json', '.jsonl']}
    metadata.update(head_after=git('rev-parse', 'HEAD'), status_after=git('status', '--short'))
    (output/'metadata.json').write_text(json.dumps(metadata, indent=2) + '\n')
    (output/'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    print(f'{len(samples)} validated measured samples: {output}', flush=True)


if __name__ == '__main__':
    main()
