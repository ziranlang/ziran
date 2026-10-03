#!/bin/sh
set -eu
ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
bin=$(CDPATH= cd -- "$(dirname -- "$ziran")" && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

python3 - "$repo" "$bin" "$work" <<'PY'
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

repo, binary, work = map(Path, sys.argv[1:])
ziran = binary / 'ziran'

def diagnostics(command, code=None, *, env=None, success=False):
    result = subprocess.run([os.fspath(part) for part in command], env=env,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=30)
    assert (result.returncode == 0) == success, (command, result.returncode, result.stderr)
    items = [json.loads(line) for line in result.stderr.decode('utf-8').splitlines()]
    assert items, (command, result.stderr)
    for item in items:
        assert item['schema_version'] == 1, item
        assert item['severity'] in ('error', 'warning'), item
        assert isinstance(item['code'], str) and isinstance(item['message'], str), item
        assert all(key in item for key in ('path', 'line', 'column', 'end_line', 'end_column')), item
    if code:
        assert any(item['code'] == code for item in items), (command, items)
    return items

# Invalid arguments honor a later JSON flag, including in the formatter.
for tool in ('zi2c', 'zi2cpp', 'zi2go', 'zi2rust', 'zi2py', 'zi2zir',
             'zi2zib', 'zi-api', 'zi-inspect', 'zi-fmt'):
    diagnostics([binary / tool, '--unknown', '--diagnostics=json'], 'command.arguments')

diagnostics([ziran, 'build', '--target=unknown', '--diagnostics=json'], 'package.command')
diagnostics([ziran, 'check', '--diagnostics=json', '--root', work, work / 'missing.zi'], 'module.input')
diagnostics([ziran, 'inspect', '--diagnostics=json', work / 'missing.zir'], 'zir.input')
diagnostics([ziran, 'run', '--diagnostics=json', work / 'missing.zib'], 'zib.input')
diagnostics([ziran, 'fmt', '--diagnostics=json', work / 'missing.zi'], 'format.input')

# A native byte path and a valid multilingual path must both remain valid JSON.
for name in (b'\xff.zir', '文🙂.zir'.encode()):
    path = os.fsencode(work) + b'/' + name
    items = diagnostics([os.fsencode(ziran), b'inspect', b'--diagnostics=json', path], 'zir.input')
    if name.startswith(b'\xff'):
        assert items[0]['path'].endswith('ÿ.zir'), items
    else:
        assert items[0]['path'].endswith('文🙂.zir'), items

unformatted = work / 'format.zi'
unformatted.write_text('Answer::()->s32{return 42}\n')
diagnostics([ziran, 'fmt', '--check', '--diagnostics=json', unformatted], 'format.changed')

source = work / 'native.zi'
source.write_text('#program_export\nAnswer :: () -> s32 { return 42 }\n')
string_entry = work / 'text_entry.zi'
string_entry.write_text('#program_export\nAnswer :: () -> string { return "text" }\n')
diagnostics([ziran, 'build', '--target=c', '--exe', '--entry', 'text_entry:Answer',
             '--diagnostics=json', '--root', work, '-o', work / 'string', string_entry], 'zir_c.entry')

fake = work / 'cc'
fake.write_text('#!/usr/bin/env python3\nimport os,sys\nos.write(2, "文🙂\\n".encode() + b"\\xff\\xed\\xa0\\x80\\x00" + b"x"*524288)\nsys.exit(3)\n')
fake.chmod(0o700)
child_env = os.environ | {'CC': str(fake)}
command = [ziran, 'build', '--target=c', '--exe', '--entry', 'native:Answer',
           '--diagnostics=json', '--root', work, '-o', work / 'c', source]
items = diagnostics(command, 'zir_c.toolchain', env=child_env)
assert any('文🙂' in item['message'] and 'ÿ' in item['message'] and
           'output truncated' in item['message'] for item in items), items
diagnostics(command, 'zir_c.toolchain', env=os.environ | {'CC': str(work / 'missing-cc')})

# The launcher's uncaptured exec failure must not leak perror text into JSON.
launcher = work / 'launcher'
shutil.copy2(ziran, launcher)
diagnostics([launcher, 'check', '--diagnostics=json', '--root', work, source], 'package.process')

probe = work / 'diagnostic.c'
probe.write_text(r'''
#include "zir_diagnostic.h"
#include <assert.h>
#include <stdlib.h>
static int prohibit_allocation;
void *AllocateOrExit(size_t size) { assert(!prohibit_allocation); return malloc(size); }
const char *SpanPath(ZirSourceSpan span) { (void)span; return ""; }
int main(void) {
    SetDiagnosticFormat("json");
    Diagnostic((ZirSourceSpan){0}, "command.arguments", "%s", "\xf0\x9f\x99\x82\xff\xc2\xe0\x80\x80\xed\xa0\x80\xf4\x90\x80\x80");
    prohibit_allocation = 1;
    DiagnosticOutOfMemory();
    return 1;
}
''')
subprocess.run(['cc', '-std=c11', '-I' + str(repo / 'cmd/zir'), str(probe),
                str(repo / 'cmd/zir/zir_diagnostic.c'), '-o', str(work / 'probe')], check=True)
items = diagnostics([work / 'probe'], 'compiler.memory')
assert items[0]['message'].startswith('🙂ÿ'), items
assert len(items) == 2, items
PY
