#!/usr/bin/env python3
"""Compile the programs `zi2zx native-c` wrote into objects a host links.

Every program defines C names other programs define too, and so may the
host's own compiled modules (Kryon's widgets, a program main). Each
program's external symbols therefore get a prefix of their own: a quick
first compile lists them, and the real compile renames them through a
forced header. The objects print one per line; the host links them all,
and their constructors register each program under its bundle identity.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import json
from pathlib import Path
import shlex
import subprocess
import sys


def compile_all(jobs, commands):
    with ThreadPoolExecutor(max_workers=jobs) as pool:
        for result in pool.map(lambda command: subprocess.run(command, capture_output=True, text=True), commands):
            if result.returncode != 0:
                sys.stderr.write(result.stdout + result.stderr)
                raise SystemExit(f'cannot compile linked code: {shlex.join(result.args)}')


def program_objects(args, index, identity, name):
    program = (args.directory / index).resolve()
    manifest = json.loads((program / 'zx_native.json').read_text())
    if manifest['identity'] != identity:
        raise SystemExit(f'{program} does not hold the program programs.txt names')
    output = (args.output / index).resolve()
    (output / 'scan').mkdir(parents=True, exist_ok=True)
    prefix = output / 'zx_prefix.h'

    def command(source, folder, flags, renamed):
        target = output / folder / (source['path'].replace('/', '__')[:-2] + '.o')
        return target, [args.cc, *flags, '-w', '-nostdinc', '-isystem', str(program / manifest['system_include']),
                        '-I', str(program), *(['-include', str(prefix)] if renamed else []),
                        *[item for header in manifest['include'] for item in ('-include', header)],
                        *['-D' + define for define in source['defines']],
                        '-c', str(program / source['path']), '-o', str(target)]

    sources = manifest['sources']
    scans = [command(source, 'scan', ['-O0'], False) for source in sources]
    compile_all(args.jobs, [item for _, item in scans])
    listed = subprocess.run([args.nm, '--defined-only', '--extern-only', '--just-symbol-name',
                             *[str(target) for target, _ in scans]], check=True, capture_output=True, text=True)
    names = {line.strip() for line in listed.stdout.splitlines()
             if line.strip() and not line.strip().endswith(':')}
    # Clang names a C main by its parameters on WebAssembly.
    if names & {'__main_argc_argv', '__main_void'}:
        names.add('main')
    names -= {'__main_argc_argv', '__main_void'}
    tag = f'zx{identity[:12]}_'
    prefix.write_text(''.join(f'#define {symbol} {tag}{symbol}\n' for symbol in sorted(names)))
    built = [command(source, '', args.cflags, True) for source in sources]
    compile_all(args.jobs, [item for _, item in built])
    print(f'{name}: {len(sources)} sources, {len(names)} symbols as {tag}*', file=sys.stderr)
    return [target for target, _ in built]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path, help='the folder zi2zx native-c wrote')
    parser.add_argument('--output', type=Path, required=True, help='folder for the objects')
    parser.add_argument('--cc', default='emcc')
    parser.add_argument('--nm', default='llvm-nm')
    parser.add_argument('--cflags', default='-O2', help='compile flags, as one shell word list')
    parser.add_argument('--jobs', type=int, default=2)
    args = parser.parse_args()
    args.cflags = shlex.split(args.cflags)
    objects = []
    for line in (args.directory / 'programs.txt').read_text().splitlines():
        index, identity, name = line.split(' ', 2)
        objects += program_objects(args, index, identity, name)
    print('\n'.join(map(str, objects)))


if __name__ == '__main__':
    main()
