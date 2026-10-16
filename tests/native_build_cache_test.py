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

def build(answer, changed=None, fail=False, cwd=None):
    before = objects()
    result = subprocess.run(command, env=env, cwd=cwd, capture_output=True, text=True, timeout=30)
    assert (result.returncode != 0) == fail, result.stderr
    if result.stderr:
        for line in result.stderr.splitlines(): assert json.loads(line)['code'] == 'zir_c.toolchain', line
    if answer is not None:
        actual = subprocess.run([str(out/'app')]).returncode
        assert actual == answer, (actual, answer)
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
int main(int argc, char **argv) {
    volatile int revision = REVISION;
    (void)revision;
    int gated = 0;
    for(int i=1;i<argc;i++) if(strstr(argv[i], "/host.c")) gated=1;
    char option[64];
    char **command = calloc((size_t)argc + 2, sizeof(*command));
    if(!command) return 1;
    int count = 0, piped = 0, compile = 0, null_input = 0;
    command[count++] = REAL_CC;
    if(getenv("CACHE_CONFIG")) {
        FILE *file = fopen(getenv("CACHE_CONFIG"), "r");
        int value;
        if(!file || fscanf(file, "%d", &value) != 1 || fclose(file) != 0) return 1;
        snprintf(option, sizeof(option), "-DCACHE_ADDEND=%d", value);
        command[count++] = option;
    }
    for(int i=1;i<argc;i++) {
        command[count++] = argv[i];
        piped += !strcmp(argv[i], "-pipe");
        compile |= !strcmp(argv[i], "-c");
        null_input |= !strcmp(argv[i], "/dev/null");
    }
    if(null_input && getenv("CACHE_PROBE_LOG")) {
        FILE *file = fopen(getenv("CACHE_PROBE_LOG"), "a");
        if(!file) return 1;
        fprintf(file, "%s\n", piped ? "pipe" : compile ? "compile" : "other");
        if(fclose(file) != 0) return 1;
    }
    if(compile && !null_input && getenv("CACHE_COMPILE_LOG")) {
        FILE *file = fopen(getenv("CACHE_COMPILE_LOG"), "a");
        if(!file) return 1;
        fprintf(file, "%d\n", piped);
        if(fclose(file) != 0) return 1;
    }
    if(null_input && getenv("CACHE_FAIL_PROBE")) return 1;
    if(null_input && getenv("CACHE_PROBE_TEXT")) {
        FILE *file = fopen(getenv("CACHE_PROBE_TEXT"), "r");
        if(!file) return 1;
        char bytes[4096]; size_t length;
        while((length=fread(bytes, 1, sizeof(bytes), file)))
            if(fwrite(bytes, 1, length, stderr) != length) return 1;
        return ferror(file) || fclose(file) != 0;
    }
    if(null_input && getenv("CACHE_EMPTY_PROBE")) return 0;
    if(null_input && getenv("CACHE_BINARY_PROBE")) fputc(0, stderr);
    if(null_input && getenv("CACHE_LARGE_PROBE"))
        for(int i=0;i<70000;i++) fputc('x', stderr);
    if(null_input && getenv("CACHE_AMBIGUOUS_PROBE"))
        fputs("#include <...> search starts here:\nambiguous interleaved output\nEnd of search list.\n", stderr);
    if(null_input && getenv("CACHE_VERSION_CONFIG")) {
        FILE *file = fopen(getenv("CACHE_VERSION_CONFIG"), "r");
        int value;
        if(!file || fscanf(file, "%d", &value) != 1 || fclose(file) != 0) return 1;
        fprintf(stderr, "Test math library version %d\n", value);
    }
    if(!gated || !getenv("CACHE_GATE")) { execv(REAL_CC, command); return 127; }
    pid_t child=fork();
    if(child==0) { execv(REAL_CC, command); _exit(127); }
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

# Piped GCC may print its assembler command before or after cc1's include
# search list. The complete option records are the same; independent tool
# discovery must not change the identity when only that scheduling changes.
probe = subprocess.run([compiler, *env['CFLAGS'].split(), '-I', str(out), '-pipe',
                        '-v', '-x', 'c', '-c', '/dev/null', '-o', '/dev/null'],
                       env=env, capture_output=True, text=True, check=True).stderr
lines = probe.splitlines(keepends=True)
assembler = next((line for line in lines if line.startswith(' ') and line.split() and
                  Path(line.split()[0]).name.endswith('as')), None)
if assembler and 'End of search list.\n' in lines:
    lines.remove(assembler)
    early = next(i for i, line in enumerate(lines) if line.startswith(('ignoring nonexistent directory', '#include ')))
    late = lines.index('End of search list.\n') + 1
    probe_text = work/'compiler-probe.txt'
    env['CACHE_PROBE_TEXT'] = str(probe_text)
    probe_text.write_text(''.join([*lines[:early], assembler, *lines[early:]]))
    build(51)
    probe_text.write_text(''.join([*lines[:late], assembler, *lines[late:]]))
    build(51, set())
    probe_text.write_text(''.join([*lines[:early], assembler, *lines[early:]]))
    build(51, set())

    # cc1 emits its compiler prefix separately from its math-library versions.
    # The assembler's whole banner may arrive between those writes, within
    # the compiler line. Retain every version byte and every derived option.
    prefix = '\tcompiled by GNU C version 99.0.0, '
    libraries = 'GMP version 6.3.0, MPFR version 4.2.1, MPC version 1.3.1, isl version 0.27\n'
    banner = 'GNU assembler version 2.44 (target) using BFD version 2.44\n'
    probe_text.write_text(probe + prefix + libraries + banner)
    build(51)
    probe_text.write_text(probe + prefix + banner + libraries)
    build(51, set())
    probe_text.write_text(probe + prefix + libraries + banner)
    build(51, set())
    probe_text.write_text(probe + prefix + libraries.replace('6.3.0', '7.0.0') + banner)
    assert len(build(51)) == len(objects())
    build(51, set())
    env.pop('CACHE_PROBE_TEXT')

# A compiler's configuration can change its derived options without changing
# its executable, the requested flags or the environment. Model an installed
# specs file using this test's own native driver and retain the old mtime.
config = work/'compiler-config'
config.write_text('0\n')
env['CACHE_CONFIG'] = str(config)
host.write_text('#include "config #$ space.h"\nint host(void) { return CONFIG + 24 + CACHE_ADDEND; }\n')
build(51)
build(51, set())
saved = config.stat()
config.write_text('1\n')
os.utime(config, ns=(saved.st_atime_ns, saved.st_mtime_ns))
assert len(build(52)) == len(objects())
build(52, set())

# A native compiler copied or hardlinked as cc needs the same identity checks.
alias = work/'cc'
shutil.copy2(proxy, alias)
env['CC'] = str(alias)
config.write_text('0\n')
build(51)
build(51, set())
config.write_text('1\n')
assert len(build(52)) == len(objects())
build(52, set())
env['CC'] = str(proxy)

# The successful GCC probe enables the same piped object compile. Observe the
# real native driver's arguments, including explicit flags and probe failure.
compile_log = work/'compile-flags.log'
env['CACHE_COMPILE_LOG'] = str(compile_log)
original_flags = env['CFLAGS']
build(52)
assert compile_log.read_text().splitlines() == ['1'] * len(objects())
previous_lines = compile_log.read_text()
build(52, set())
assert compile_log.read_text() == previous_lines
env['CFLAGS'] += ' -pipe'
compile_log.unlink()
build(52)
assert compile_log.read_text().splitlines() == ['1'] * len(objects())
env['CFLAGS'] = original_flags
# A header called -pipe is an option value, not an explicit pipe flag.
(work/'-pipe').write_text('#define PIPE_FLAG_INPUT 1\n')
env['CFLAGS'] += ' -include -pipe'
compile_log.unlink()
build(52, cwd=work)
assert compile_log.read_text().splitlines() == ['2'] * len(objects())
env['CFLAGS'] = original_flags
env['CACHE_FAIL_PROBE'] = '1'
compile_log.unlink()
build(52)
assert compile_log.read_text().splitlines() == ['0'] * len(objects())
del env['CACHE_FAIL_PROBE']
del env['CACHE_COMPILE_LOG']

# The GCC probe uses a pipe while keeping the compiler and assembler version
# checks. Missing or ambiguous discovery disables reuse while ordinary
# compilation still works.
probe_log = work/'probe.log'
env['CACHE_PROBE_LOG'] = str(probe_log)
build(52)
probe_log.write_text('')
build(52, set())
assert probe_log.read_text().splitlines() == ['pipe']
profile = work/'probe-profile.jsonl'
env['ZIRAN_PROFILE'] = str(profile)
build(52)
build(52, set())
counters = json.loads(profile.read_text().splitlines()[-1])['counters']
assert counters['native_probe_pipe'] == 1
assert counters.get('native_probe_compile', 0) == 0
assert counters['native_object_reuse'] == len(objects())
compiled = json.loads(profile.read_text().splitlines()[0])['counters']
assert compiled['native_object_pipe'] == compiled['native_object_compile'] == len(objects())
del env['ZIRAN_PROFILE']
# Retain the version information from the original compile probe, including
# the versions of libraries used by the compiler itself.
version_config = work/'compiler-version'
version_config.write_text('1\n')
env['CACHE_VERSION_CONFIG'] = str(version_config)
build(52)
build(52, set())
old_version = version_config.stat()
version_config.write_text('2\n')
os.utime(version_config, ns=(old_version.st_atime_ns, old_version.st_mtime_ns))
assert len(build(52)) == len(objects())
build(52, set())
del env['CACHE_VERSION_CONFIG']
for option in ('CACHE_FAIL_PROBE', 'CACHE_EMPTY_PROBE', 'CACHE_BINARY_PROBE', 'CACHE_AMBIGUOUS_PROBE', 'CACHE_LARGE_PROBE'):
    env[option] = '1'
    assert len(build(52)) == len(objects())
    assert len(build(52)) == len(objects())
    del env[option]
del env['CACHE_CONFIG']
del env['CACHE_PROBE_LOG']
host.write_text('#include "config #$ space.h"\nint host(void) { return CONFIG + 24; }\n')

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

# Clang keeps its single compile probe: a second driver invocation costs more
# than compiling an empty file on hosts with shared LLVM libraries.
clang = shutil.which('clang') or shutil.which('clang-19')
if clang:
    env['CC'] = clang
    for option in ('CACHE_GATE', 'CACHE_READY', 'CACHE_RELEASE'):
        env.pop(option, None)
    profile = work/'clang-profile.jsonl'
    env['ZIRAN_PROFILE'] = str(profile)
    build(52)
    build(52, set())
    counters = json.loads(profile.read_text().splitlines()[-1])['counters']
    assert counters['native_probe_compile'] == 1
    assert counters.get('native_probe_pipe', 0) == 0
    assert counters['native_object_reuse'] == len(objects())
    compiled = json.loads(profile.read_text().splitlines()[0])['counters']
    assert compiled['native_object_compile'] == len(objects())
    assert compiled.get('native_object_pipe', 0) == 0
print('Native object reuse validates contents, include lookup, compiler options and identity, flags, environment, probe failures and concurrent edits')
