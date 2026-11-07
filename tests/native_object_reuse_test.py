"""A one-module edit rebuilds only that module's native object.

Unchanged objects stay reused across shells and build tickets and when
another generated C file or header appears.
"""
import json
import os
from pathlib import Path
import subprocess
import sys
from native_test_compiler import direct_compiler

ziran, work = (Path(arg).resolve() for arg in sys.argv[1:])
compiler = direct_compiler()
src = work/'src'
src.mkdir()
out = work/'out'
modules = ['alpha', 'beta', 'gamma', 'delta']
# A file-scope helper keeps each Read out of line, so an edit to it changes
# that module's C alone and no header another module includes.
body = ('#scope_file\nStep :: (x: s32) -> s32 {{ if x > {0} {{ return x - {0} }} return x + {0} }}\n'
        '#scope_export\nRead :: (x: s32) -> s32 {{ return Step(x) }}\n')
for i, name in enumerate(modules): (src/f'{name}.zi').write_text(body.format(i + 1))
(src/'app.zi').write_text(''.join(f'{n.capitalize()} :: #import "{n}";\n' for n in modules) +
                          '#program_export\nAnswer :: () -> s32 { return ' +
                          ' + '.join(f'{n.capitalize()}.Read(0)' for n in modules) + ' }\n')
command = [str(ziran), 'build', '--target=c', '--exe', '--entry', 'app:Answer', '--diagnostics=json',
           '--root', str(src), '-o', str(out), str(src/'app.zi')]
base = {key: value for key, value in os.environ.items() if key != 'ZIRAN_PROFILE'}
base.update({'CC': compiler, 'CFLAGS': '-O2'})
profile = work/'profile.json'


def objects():
    return {str(path.relative_to(out/'.objects')): (path.stat().st_mtime_ns, path.stat().st_ino)
            for path in (out/'.objects').rglob('*.o')} if (out/'.objects').exists() else {}


def build(answer, env=None, changed=None):
    """Build, run, and return the objects this build wrote."""
    before = objects()
    profile.unlink(missing_ok=True)
    result = subprocess.run(command, env={**base, **(env or {}), 'ZIRAN_PROFILE': str(profile)},
                            capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stderr
    assert subprocess.run([str(out/'app')]).returncode == answer
    counters = json.loads(profile.read_text().splitlines()[-1])['counters']
    after = objects()
    updates = {name for name, value in after.items() if before.get(name) != value}
    assert counters.get('native_object_compile', 0) == len(updates), (counters, updates)
    if changed is not None: assert updates == changed, (updates, changed)
    return updates


# ziran_exe_main.c is the generated process entry for --exe.
every = {f'{name}.c.o' for name in modules + ['app', 'ziran_exe_main']}
build(10, changed=every)

# Another shell, session or build-queue ticket differs in unrelated variables
# and its private TMPDIR; none of them changes an object.
tickets = work/'ticket'
tickets.mkdir()
build(10, {'ZIRAN_REUSE_TEST_TICKET': '1234:5678', 'TMPDIR': str(tickets), 'OLDPWD': '/'}, set())

# A one-line edit in one module recompiles that module's object alone.
(src/'beta.zi').write_text(body.format(2).replace('return x + 2', 'return x + 5'))
build(13, changed={'beta.c.o'})
(src/'beta.zi').write_text(body.format(2))
build(10, changed={'beta.c.o'})

# A generated C file appearing or leaving (a module split into units, a new
# module) compiles itself and keeps every other object.
extra = out/'extra_unit.c'
extra.write_text('int extra_unit(void) { return 1; }\n')
build(10, changed={'extra_unit.c.o'})
extra.unlink()
build(10, changed=set())
# So does an unrelated header.
(out/'extra_unit.h').write_text('#define EXTRA_UNIT 1\n')
build(10, changed=set())
(out/'extra_unit.h').unlink()
build(10, changed=set())
# A new header that an existing include now reaches first rebuilds its
# includers: the output directory precedes the system include roots.
(out/'stdint.h').write_text('#include_next <stdint.h>\n')
assert 'beta.c.o' in build(10)
build(10, changed=set())
(out/'stdint.h').unlink()
assert 'beta.c.o' in build(10)

# Variables the compiler reads still form part of the identity.
build(10, {'SOURCE_DATE_EPOCH': '1'}, every)
build(10, {'SOURCE_DATE_EPOCH': '1'}, set())
print('native object reuse: ok')
