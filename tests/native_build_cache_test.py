import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time

ziran, work = (Path(arg).resolve() for arg in sys.argv[1:])
compiler = shutil.which(os.environ.get('CC', 'cc'))
out = work/'out with spaces'
out.mkdir()
headers = work/'headers'
headers.mkdir()
header = headers/'config #$ space.h'
header.write_text('#define CONFIG 11\n')
(work/'shared.zi').write_text('Increment :: 7;\n')
(work/'leaf.zi').write_text('Shared :: #import "shared";\nRead :: () -> s32 { return Shared.Increment + 1 }\n')
(work/'stable.zi').write_text('Read :: () -> s32 { return 3 }\n')
(work/'app.zi').write_text('Leaf :: #import "leaf";\nStable :: #import "stable";\n'
                          'lib :: #system_library "host";\nHost :: () -> s32 #foreign lib "host";\n'
                          '#program_export\nAnswer :: () -> s32 { return Leaf.Read() + Stable.Read() + Host() }\n')
host = out/'host.c'
host.write_text('#include "config #$ space.h"\n#ifndef BUMP\n#define BUMP 0\n#endif\n'
                'extern int base(void); int host(void) { return CONFIG + base() + BUMP; }\n')

def library(value):
    (work/'base.c').write_text(f'int base(void) {{ return {value}; }}\n')
    subprocess.run([compiler, '-c', str(work/'base.c'), '-o', str(work/'base.o')], check=True)
    subprocess.run(['ar', 'rcs', str(work/'libbase.a'), str(work/'base.o')], check=True)

library(20)
env = {**os.environ, 'CC': compiler, 'CFLAGS': '-O2 -isystem '+str(headers),
       'LDFLAGS': '-L'+str(work), 'LDLIBS': '-lbase'}
command = [str(ziran), 'build', '--target=c', '--exe', '--entry', 'app:Answer',
           '--diagnostics=json', '--root', str(work), '-o', str(out), str(work/'app.zi')]

def objects():
    return {str(path.relative_to(out/'.objects')): (path.stat().st_mtime_ns, path.stat().st_ino)
            for path in (out/'.objects').rglob('*.o')}

def build(answer, changed=None, fail=False):
    before = objects()
    result = subprocess.run(command, env=env, capture_output=True, text=True, timeout=30)
    assert (result.returncode != 0) == fail, result.stderr
    if result.stderr:
        for line in result.stderr.splitlines(): assert json.loads(line)['code'] == 'zir_c.toolchain', line
    if answer is not None: assert subprocess.run([str(out/'app')]).returncode == answer
    after = objects()
    updates = {name for name, value in after.items() if before.get(name) != value}
    if changed is not None: assert updates == changed, (updates, changed, result.stderr)
    return updates

build(42)
assert list(out.rglob('*.inputs')), 'compiler was not eligible for object reuse'
build(42, set())
(work/'leaf.zi').write_text('Shared :: #import "shared";\nRead :: () -> s32 { return Shared.Increment + 2 }\n')
build(43, {'leaf.c.o'})
(work/'shared.zi').write_text('Increment :: 10;\n')
updates = build(46)
assert 'leaf.c.o' in updates and 'stable.c.o' not in updates, updates

# Content checks cannot be replaced with size/mtime-only checks.
saved = header.stat()
header.write_text('#define CONFIG 12\n')
os.utime(header, ns=(saved.st_atime_ns, saved.st_mtime_ns))
build(47, {'host.c.o'})
library(21)
build(48, set())  # The archive changed: reuse objects, but always relink.
env['CFLAGS'] += ' -DBUMP=1'
assert len(build(49)) == len(objects())
env['ZIRAN_CACHE_TEST_ENV'] = 'changed'
assert len(build(49)) == len(objects())
build(49, set())

# Cache corruption and missing outputs must be misses, never silent reuse.
stable = out/'.objects/stable.c.o'
stable.with_name(stable.name+'.inputs').write_bytes(b'truncated')
build(49, {'stable.c.o'})
stable.write_bytes(b'not an object')
build(49, {'stable.c.o'})
stable.unlink()
build(49, {'stable.c.o'})
header.unlink()
build(49, fail=True)
header.write_text('#define CONFIG 12\n')
assert 'host.c.o' in build(49)

# Dependency hashes are SHA-256, including empty and multi-block headers.
empty = headers/'empty.h'
empty.write_bytes(b'')
large = headers/'large.h'
large.write_text('/*'+'x'*17000+'*/\n')
host.write_text('#include "empty.h"\n#include "large.h"\nint host(void) { return 34; }\n')
build(49)
stamp = (out/'.objects/host.c.o.inputs').read_bytes()
assert hashlib.sha256(b'').digest() in stamp
assert hashlib.sha256(large.read_bytes()).digest() in stamp
assert hashlib.sha256((out/'.objects/host.c.o').read_bytes()).digest() == stamp[48:80]
build(49, set())

# An existing include directory can gain a higher-priority nested header.
early, late = work/'early', work/'late'
for directory in [early, late]: (directory/'nested').mkdir(parents=True)
(late/'nested/value.h').write_text('#define VALUE 34\n')
host.write_text('#include "nested/value.h"\nint host(void) { return VALUE; }\n')
env['CFLAGS'] += ' -I'+str(early)+' -I'+str(late)
build(49)
build(49, set())
(early/'nested/value.h').write_text('#define VALUE 35\n')
build(50, {'host.c.o'})

# Flat literal probes track include names; nonliteral probes and clocks
# remain uncached.
host.write_text('#if __has_include("optional.h")\n#include "optional.h"\n#else\n#define VALUE 35\n#endif\n'
                'int host(void) { return VALUE; }\n')
build(50)
build(50, set())
(headers/'optional.h').write_text('#define VALUE 36\n')
assert 'host.c.o' in build(51)
host.write_text('#define HEADER "optional.h"\n#if __has_include(HEADER)\n#include HEADER\n#endif\n'
                'int host(void) { return VALUE; }\n')
build(51)
build(51, {'host.c.o'})
host.write_text('int host(void) { return 36 + (__TIME__[0] == 0); }\n')
build(51)
build(51, {'host.c.o'})

# A real executable shim lets the test replace only its own compiler file.
# Its changed ctime must invalidate the cache even with the old mtime.
proxy = work/'gcc'
proxy_source = work/'proxy.c'
proxy_source.write_text(r'''
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
int main(int argc, char **argv) {
    volatile int revision = REVISION;
    (void)revision;
    int gated = 0;
    for(int i=1;i<argc;i++) if(strstr(argv[i], "/host.c")) gated=1;
    argv[0] = REAL_CC;
    if(!gated || !getenv("CACHE_GATE")) { execv(REAL_CC, argv); return 127; }
    pid_t child=fork();
    if(child==0) { execv(REAL_CC, argv); _exit(127); }
    int status;
    if(child<0 || waitpid(child,&status,0)!=child || !WIFEXITED(status) || WEXITSTATUS(status)) return 1;
    close(open(getenv("CACHE_READY"), O_WRONLY|O_CREAT, 0600));
    for(int i=0;i<5000;i++) { if(access(getenv("CACHE_RELEASE"), F_OK)==0) return 0; usleep(1000); }
    return 1;
}
''')

def proxy_build(revision):
    subprocess.run([compiler, '-D_GNU_SOURCE', '-DREVISION='+str(revision),
                    '-DREAL_CC='+json.dumps(compiler), str(proxy_source), '-o', str(proxy)], check=True)

proxy_build(0)
env['CC'] = str(proxy)
host.write_text('#include "config #$ space.h"\nint host(void) { return CONFIG + 24; }\n')
build(51)
build(51, set())
saved = proxy.stat()
proxy_build(1)
os.utime(proxy, ns=(saved.st_atime_ns, saved.st_mtime_ns))
assert len(build(51)) == len(objects())
build(51, set())

# A header changed after the compiler read it must not stamp the new bytes
# as inputs to the old object. Retain the old program, then rebuild next time.
env.update(CACHE_GATE='1', CACHE_READY=str(work/'ready'), CACHE_RELEASE=str(work/'release'))
with (work/'gated.log').open('wb') as log:
    child = subprocess.Popen(command, env=env, stdout=log, stderr=log)
    try:
        deadline = time.monotonic()+30
        while not (work/'ready').exists():
            assert child.poll() is None and time.monotonic() < deadline, (work/'gated.log').read_text()
            time.sleep(.01)
        header.write_text('#define CONFIG 13\n')
        (work/'release').touch()
        assert child.wait(timeout=30) == 0, (work/'gated.log').read_text()
    finally:
        if child.poll() is None: child.terminate();child.wait(timeout=10)
assert subprocess.run([str(out/'app')]).returncode == 51
assert not (out/'.objects/host.c.o.inputs').exists()
build(52, {'host.c.o'})
build(52, set())
print('Native object reuse validates contents, include lookup, compiler identity, flags, environment and concurrent edits')
