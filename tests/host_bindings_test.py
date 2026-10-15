from pathlib import Path
import subprocess
import sys

ziran, work = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve()
declarations = '\n'.join(f'H{i:03} :: (value: s64) -> s64 #foreign host_api;' for i in range(64))
calls = '\n'.join(f'value = H{i:03}(value)' for i in range(64))
host = f'host_api :: #system_library "host_api";\n{declarations}\n'
(work / 'provider.zi').write_text(host + 'NeverCalled :: () -> s64 #foreign host_api;\n'
    'Identity :: (value: $T) -> T {\nreturn value\n}\n'
    '#program_export\nNext :: (input: s64) -> s64 {\nvalue := input\n' +
    calls.replace('value = H000(value)', 'value = H000(Identity(value))') + '\nreturn value\n}\n'
    '#program_export\nmain :: () -> s64 {\nreturn Next(7)\n}\n')
(work / 'app.zi').write_text('#import "provider"\n' + host + 'seed: s64 = H000(7);\n'
    '#program_export\nAnswer :: () -> s64 {\nvalue := Next(seed)\n' + calls + '\nreturn value\n}\n')
expected = (7 * 48271 + 1001) & ((1 << 63) - 1)
for group in range(2):
    for i in range(64): expected = (expected * 48271 + i + 1 + group * 1000) & ((1 << 63) - 1)

def run(*args):
    result = subprocess.run(list(map(str, args)), capture_output=True, text=True)
    if result.returncode: raise RuntimeError(f'{args}:\n{result.stdout}{result.stderr}')
    return result

run(ziran, 'ir', '--root', work, '-o', work / 'ir', work / 'app.zi')
library = work / 'provider.zib'
run(ziran, 'bundle', '--root', work, '--entry', 'provider:main', '-o', library, work / 'provider.zi')
bridge = work / 'bridge.zib'
(work / 'bridge.zi').write_text('#import "provider"\n#program_export\n'
    'main :: () -> s64 {\nreturn Next(7)\n}\n')
run(ziran, 'bundle', '--root', work, '--entry', 'bridge:main', '--library', library,
    '-o', bridge, work / 'bridge.zi')
for mode, options in [('standalone', []), ('thin', ['--library', library]),
                      ('included', ['--include-library', library])]:
    for form, root, source in [('source', work, work / 'app.zi'), ('saved', work / 'ir', work / 'ir/app.zir')]:
        bundle = work / f'{mode}-{form}.zib'
        run(ziran, 'bundle', '--root', root, '--entry', 'app:Answer', *options, '-o', bundle, source)
        run(work / 'test', bundle, expected, *([library] if mode == 'thin' else []))
        if mode == 'thin': run(work / 'test', bundle, expected, bridge, library)
    assert (work / f'{mode}-source.zib').read_bytes() == (work / f'{mode}-saved.zib').read_bytes()
solo = work / 'solo.zi'
solo.write_text('host_api :: #system_library "host_api";\n'
    'Only :: (value: s64) -> s64 #foreign host_api;\nseed: s64 = Only(7);\n'
    '#program_export\nAnswer :: () -> s64 {\nreturn Only(seed)\n}\n')
run(ziran, 'ir', '--root', work, '-o', work / 'single-ir', solo)
for form, root, source in [('source', work, solo), ('saved', work / 'single-ir', work / 'single-ir/solo.zir')]:
    bundle = work / f'single-{form}.zib'
    run(ziran, 'bundle', '--root', root, '--entry', 'solo:Answer', '-o', bundle, source)
    run(work / 'test', bundle)
assert (work / 'single-source.zib').read_bytes() == (work / 'single-saved.zib').read_bytes()
print('128 host bindings passed in source/saved standalone, thin, included and nested retained libraries')
