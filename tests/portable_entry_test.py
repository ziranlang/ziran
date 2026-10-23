import os
from pathlib import Path
import subprocess
import sys

ziran, work = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve()
cases = []
for width in [8, 16, 32, 64]:
    for prefix in ['s', 'u']:
        kind = f'{prefix}{width}'
        values = [0, (1 << (width - (prefix == 's'))) - 1]
        if prefix == 's': values.append(-(1 << (width - 1)))
        for index, value in enumerate(values):
            cases.append((f'{kind}_{index}', kind, f'return {value}', value))
cases.extend([
    ('u64_arithmetic', 'u64', 'return (cast(u64)1 << 63) | cast(u64)123', (1 << 63) | 123),
    ('u8_narrow', 'u8', 'return cast(u8)511', 255),
    ('s8_narrow', 's8', 'return cast(s8)255', -1),
    ('true', 'bool', 'return true', 1),
    ('false', 'bool', 'return false', 0),
    ('void', 'void', 'return', None),
    ('void_fallthrough', 'void', '', None),
])
functions = '\n'.join(f'#program_export\n{name} :: () -> {kind} {{\n{body}\n}}\n'
                      for name, kind, body, _ in cases)
source = work / 'entries.zi'
source.write_text(functions)

def run(*args, **kwargs):
    return subprocess.run(list(map(str, args)), check=True, capture_output=True, text=True, **kwargs)

run(ziran, 'ir', '--root', work, '-o', work / 'ir', source)
for name, _, _, expected in cases:
    output = '' if expected is None else f'{expected}\n'
    for form, entry, root in [('source', source, work), ('saved', work / 'ir/entries.zir', work / 'ir')]:
        bundle = work / f'{name}-{form}.zx'
        run(ziran, 'bundle', '--root', root, '--entry', f'entries:{name}', '-o', bundle, entry)
        actual = run(ziran, 'run', bundle).stdout
        assert actual == output, (name, form, repr(actual), repr(output))
        run(work / 'host', bundle, (expected or 0) % (1 << 64), int(expected is not None))
    assert (work / f'{name}-source.zx').read_bytes() == (work / f'{name}-saved.zx').read_bytes()

for name, signature, body in [
    ('real', '() -> float64', 'return 1.5'),
    ('text', '() -> string', 'return "unsupported entry"'),
    ('parameter', '(value: s32) -> s32', 'return value'),
    ('sized', '() -> usize', 'return 1'),
]:
    invalid = work / f'{name}.zi'
    invalid.write_text(f'#program_export\nAnswer :: {signature} {{\n{body}\n}}\n')
    result = subprocess.run(list(map(str, [ziran, 'bundle', '--root', work, '--entry', f'{name}:Answer',
                            '-o', work / f'{name}.zx', invalid])), capture_output=True, text=True,
                            env=dict(os.environ, ZIRAN_DIAGNOSTICS='json'))
    assert result.returncode != 0 and 'zx.entry' in result.stderr, (name, result.stderr)
print(f'{len(cases)} portable scalar entries passed from source and saved IR through CLI and host APIs')
