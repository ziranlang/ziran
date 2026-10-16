"""Link options retain native object reuse while real link inputs still change."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ziran, work = (Path(x).resolve() for x in sys.argv[1:])
if not sys.platform.startswith('linux'):
    print('ELF linker-option checks require a Linux host')
    raise SystemExit(0)
work.mkdir()
compiler = shutil.which(os.environ.get('CC', 'cc'))
integrated_assembler = 'clang' in subprocess.check_output([compiler, '--version'], text=True).lower()
assembler = subprocess.check_output([compiler, '-print-prog-name=as'], text=True).strip()
assembler = shutil.which(assembler)
assert assembler
source = work/'flags.zi'
source.write_text('lib :: #system_library "host";\nHost :: () -> s32 #foreign lib "host";\n'
                  '#program_export\nAnswer :: () -> s32 { return 20 + Host() }\n')
out = work/'output'
out.mkdir()
(out/'host.c').write_text('int host(void) { return 20; }\nint __wrap_host(void) { return 30; }\n')
profile = work/'profile.jsonl'
env = dict(os.environ, CC=compiler, LDFLAGS='', LDLIBS='', ZIRAN_PROFILE=str(profile))
command = [ziran, 'build', '--target=c', '--exe', '--entry', 'flags:Answer',
           '--diagnostics=json', '--root', work, '-o', out, source]


def build(flags, expected, *, reuse=False, fail=False):
    result = subprocess.run(list(map(str, command)), env=dict(env, CFLAGS=flags),
                            capture_output=True, text=True, timeout=30)
    assert (result.returncode != 0) == fail, result.stderr
    assert subprocess.run([str(out/'flags')]).returncode == expected
    records = [json.loads(x) for x in profile.read_text().splitlines()]
    counters = records[-1]['counters']
    object_count = len(list((out/'.objects').rglob('*.o')))
    assert object_count >= 2
    if reuse:
        assert counters.get('native_object_compile', 0) == 0, counters
        assert counters['native_object_reuse'] == object_count, counters
    elif not fail:
        assert counters['native_object_compile'] == object_count, counters
    return counters


for flag in ['-Wl,--wrap=host', '-Xlinker --wrap=host', '-fuse-ld=bfd -Wl,--wrap=host']:
    flags = '-O2 '+flag
    build(flags, 50)
    build(flags, 50, reuse=True)

# -B may select an assembler as well as a linker. Changing that tool must
# invalidate objects even when its old mtime is retained.
tools = work/'tools'
tools.mkdir()
proxy = work/'assembler.c'
proxy.write_text('#include <unistd.h>\nint main(int argc, char **argv) {\n'
                 'volatile int revision = REVISION; (void)revision; (void)argc;\n'
                 'execv(REAL_AS, argv); return 127; }\n')


def replace_assembler(revision):
    subprocess.run([compiler, '-DREVISION='+str(revision), '-DREAL_AS='+json.dumps(assembler),
                    str(proxy), '-o', str(tools/'as')], check=True)


replace_assembler(0)
flags = '-O2 -B'+str(tools)+' -Wl,--wrap=host'
build(flags, 50)
build(flags, 50, reuse=True)
saved = (tools/'as').stat()
replace_assembler(1)
os.utime(tools/'as', ns=(saved.st_atime_ns, saved.st_mtime_ns))
build(flags, 50, reuse=integrated_assembler)
build(flags, 50, reuse=True)
# Separate option values must retain the same checks.
flags = '-O2 -B '+str(tools)+' -Xlinker --wrap=host'
build(flags, 50)
build(flags, 50, reuse=True)

# Reusing objects must still run the linker and regenerate requested maps.
link_map = work/'link.map'
flags = '-O2 -Xlinker -Map -Xlinker '+str(link_map)
build(flags, 40)
link_map.unlink()
build(flags, 40, reuse=True)
assert link_map.stat().st_size > 0

# A linker-only library disappears after objects were cached. Failure must
# preserve the last executable rather than reusing that output as a success.
library = work/'libextra.a'
(work/'extra.c').write_text('int extra(void) { return 1; }\n')
subprocess.run([compiler, '-c', str(work/'extra.c'), '-o', str(work/'extra.o')], check=True)
subprocess.run(['ar', 'rcs', str(library), str(work/'extra.o')], check=True)
flags = '-O2 -Wl,'+str(library)
build(flags, 40)
build(flags, 40, reuse=True)
library.unlink()
build(flags, 40, reuse=True, fail=True)

# The same driver policy applies to saved checked IR, and ordinary flags
# keep their previous cache behavior.
subprocess.run([str(ziran), 'ir', '--root', str(work), '-o', str(work/'ir'), str(source)], check=True)
command = [ziran, 'build', '--target=c', '--exe', '--entry', 'flags:Answer',
           '--diagnostics=json', '--root', work/'ir', '-o', out, work/'ir/flags.zir']
flags = '-O2 -fuse-ld=bfd -Xlinker --wrap=host'
build(flags, 50)
build(flags, 50, reuse=True)
build('-O2', 40)
build('-O2', 40, reuse=True)
print('Linker flags reuse objects, track changed prefix tools, regenerate maps and preserve executables on missing libraries')
