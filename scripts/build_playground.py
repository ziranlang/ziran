#!/usr/bin/env python3
"""Build the website's real Ziran checker and VM with Emscripten.

Uses the ordinary Makefile library sources and module grouping. Generated
assets stay out of Git; the website workflow builds them before publishing.
"""
import argparse
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import re

repo = Path(__file__).resolve().parent.parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--emcc', default=shutil.which('emcc'))
args = parser.parse_args()
if not args.emcc:
    parser.error('emcc is required; activate the Emscripten SDK first')
emcc = Path(args.emcc).resolve()
emar = emcc.with_name('emar')
build = repo / 'build' / 'playground64'
assets = repo / 'site' / 'assets'
environment = dict(os.environ)
environment.pop('DISPLAY', None)
environment.pop('WAYLAND_DISPLAY', None)
environment.setdefault('EM_CACHE', str(repo / 'build' / 'emscripten-cache'))
environment.pop('EM_FROZEN_CACHE', None)

def run(command):
    subprocess.run(command, cwd=repo, env=environment, check=True)

C_KEYWORDS = {'char', 'const', 'double', 'float', 'int', 'long', 'short',
              'signed', 'sizeof', 'struct', 'union', 'unsigned', 'void'}
private = build / 'private'
private.mkdir(parents=True, exist_ok=True)
for group in ('parse', 'check', 'emit', 'vm'):
    header = (repo / 'cmd' / 'zir' / f'zir_{group}_internal.h').read_text()
    block = header.split('#pragma GCC visibility push(hidden)', 1)[1].split('#pragma GCC visibility pop', 1)[0]
    block = re.sub(r'/\*.*?\*/|//[^\n]*', '', block, flags=re.S)
    # Declared names only: `char (*lines)[N]` declares a parameter, and C
    # type keywords must never become macros.
    names = sorted(set(re.findall(r'\b([A-Za-z_]\w*)\s*\((?!\s*\*)', block)) - C_KEYWORDS)
    content = ''.join(f'#define {name} private_{group}_{name}\n' for name in names)
    path = private / f'{group}.h'
    if not path.exists() or path.read_text() != content:
        path.write_text(content)

# Preserve the compiler's 64-bit long values, then lower memory addressing to
# ordinary wasm32 for browsers that do not support WebAssembly memory64.
run(['make', f'BUILD_DIR={build.relative_to(repo)}',
     f'CC={shlex.quote(str(emcc))} -sMEMORY64=2', f'AR={shlex.quote(str(emar))}',
     'OBJCOPY=true', f'WASM_PRIVATE_HEADERS={private.relative_to(repo)}',
     'CFLAGS=-O2', 'FRAMEFLAGS=-Wframe-larger-than=16384',
     str((build / 'libziran.a').relative_to(repo))])
run([str(emcc), '-O2', '-std=c11', '-D_GNU_SOURCE', '-Iinclude', '-Icmd/zir',
     'web/playground.c', str(build / 'libziran.a'), '-lm',
     '-sMEMORY64=2', '-sMODULARIZE=1', '-sEXPORT_NAME=createPlayground',
     '-sEXPORTED_FUNCTIONS=_RunSource', '-sEXPORTED_RUNTIME_METHODS=FS,ccall',
     '-sENVIRONMENT=worker,node', '-sALLOW_MEMORY_GROWTH=1',
     '-sMAXIMUM_MEMORY=536870912', '-sSTACK_SIZE=8388608',
     '--embed-file', 'std@/std', '-o', str(assets / 'playground-runtime.js')])
