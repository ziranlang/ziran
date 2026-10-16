#!/usr/bin/env python3
"""Finish and execute C modules across many stdio output-buffer boundaries."""
import hashlib
import os
from pathlib import Path
import subprocess
import sys


ziran, work = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve()
work.mkdir()
source = work/'large.zi'
count = 512
parts = []
expected = 0
for index in range(count):
    parts.append(f'''Step{index} :: (input: s64) -> s64 {{
    value := input + {index}
    value = value * 3
    if value % 2 == 0 {{ value += 5 }} else {{ value -= 7 }}
    return value ^ 17
}}
''')
    value = (17 + index) * 3
    expected += (value + (5 if value % 2 == 0 else -7)) ^ 17
parts.append('#program_export\nmain :: () -> s32 {\nresult: s64 = 0\n')
parts.extend(f'result += Step{index}(17)\n' for index in range(count))
parts.append('print("%\\n", result)\nreturn 0\n}\n')
source.write_text(''.join(parts))
env = dict(os.environ, CFLAGS='-O0 -std=c11')
for key in ('DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'DBUS_SESSION_BUS_ADDRESS'):
    env.pop(key, None)


def run(*argv):
    result = subprocess.run(list(map(str, argv)), env=env, capture_output=True,
                            text=True, timeout=120)
    assert result.returncode == 0, (argv, result.stdout, result.stderr)
    return result.stdout


run(ziran, 'ir', '--root', work, '-o', work/'ir', source)
for form, root, entry in [('source', work, source), ('saved', work/'ir', work/'ir/large.zir')]:
    output = work/form
    command = [ziran, 'build', '--target=c', '--exe', '--entry', 'large:main',
               '--root', root, '-o', output, entry]
    run(*command)
    generated = output/'large.c'
    assert generated.stat().st_size > 65536, 'fixture must cross many output-buffer boundaries'
    assert run(output/'large') == f'{expected}\n'
    paths = sorted(p for p in output.rglob('*') if p.suffix in ('.c', '.h'))
    original = {p: (hashlib.sha256(p.read_bytes()).digest(), p.stat().st_mtime_ns) for p in paths}
    run(*command)
    assert run(output/'large') == f'{expected}\n'
    assert original == {p: (hashlib.sha256(p.read_bytes()).digest(), p.stat().st_mtime_ns) for p in paths}
print('Large source/saved C modules finish, execute every generated function and retain unchanged output timestamps')
