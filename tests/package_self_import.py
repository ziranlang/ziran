#!/usr/bin/env python3
"""Public self qualification uses manifest names, not consumer aliases."""
from pathlib import Path
import json
import os
import sys
import tempfile
from packages import call, init, write


def check_self_imports(ziran: str, root: Path, compiler: Path, env: dict) -> None:
    def compile_native(output: Path, source: Path, project: bool) -> None:
        args = [ziran, 'build', '--target=c', '--exe', '--entry', 'app:main', '-o', str(output)]
        if project:
            args += ['--project', '--offline']
        else:
            args += ['--root', str(source.parent)]
        call(*args, str(source), cwd=app, env=env)
        call(str(output / 'app'), cwd=app, env=env)

    fixture = root / 'self-imports'
    fixture.mkdir()
    leaf, provider, app = [fixture / n for n in ('leaf', 'provider', 'app')]
    for repo in (leaf, provider):
        init(repo, env)
    app.mkdir()
    write(leaf / 'ziran.toml', '''[package]
name = "Library"
module_roots = ["src"]
[exports]
Public = "src/value.zi"
Entry = "src/entry.zi"
local = "src/value.zi"
PublicLocal = "src/value.zi"
''')
    write(leaf / 'src/value.zi', 'Value :: struct { number: s32 }\n')
    write(leaf / 'src/private.zi', 'Secret :: 7;\n')
    entry = '''using Qualified :: #import "Library/Public";
using Short :: #import "value";
Echo :: (value: Qualified.Value) -> Short.Value { return value }
'''
    write(leaf / 'src/entry.zi', entry)
    write(provider / 'ziran.toml', f'''[package]
name = "Provider"
module_roots = ["src"]
[dependencies.inner]
git = "{leaf.as_uri()}"
[exports]
Wrapper = "src/wrapper.zi"
''')
    write(provider / 'src/wrapper.zi', '''using E :: #import "inner/Entry";
using V :: #import "inner/Public";
WrappedEcho :: (value: V.Value) -> V.Value { return E.Echo(value) }
RoundTrip :: (number: s32) -> s32 {
    value: V.Value; value.number = number
    return E.Echo(value).number
}
''')
    write(app / 'ziran.toml', f'''[package]
name = "Application"
entry = "src/app.zi"
module_roots = ["src"]
[toolchain]
git = "https://github.com/ziranlang/ziran.git"
[dependencies.outer]
git = "{provider.as_uri()}"
[dependencies.renamed]
git = "{leaf.as_uri()}"
[exports]
PublicLocal = "src/local.zi"
OnlyOwn = "src/local.zi"
''')
    write(app / 'ziran.local.toml', '\n'.join([
        '[overrides]', *[n + ' = ' + json.dumps(str(p)) for n, p in
                        (('ziran', compiler), ('outer', provider), ('renamed', leaf), ('Library', leaf), ('inner', leaf), ('Application', leaf))], '']))
    write(app / 'src/local.zi', 'LocalNumber :: 3;\n')
    write(app / 'src/private.zi', 'Secret :: 9;\n')
    source = '''using Own :: #import "Application/PublicLocal";
using Short :: #import "local";
using P :: #import "outer/Wrapper";
using V :: #import "renamed/Public";
main :: () -> s32 {
    value: V.Value; value.number = Own.LocalNumber
    return P.WrappedEcho(value).number - Short.LocalNumber
}
'''
    manifest = (app / 'ziran.toml').read_text()
    direct = f'[dependencies.renamed]\ngit = "{leaf.as_uri()}"\n'
    assert direct in manifest
    write(app / 'ziran.toml', manifest.replace(direct, ''))
    assert '[dependencies.renamed]' not in (app / 'ziran.toml').read_text()
    transitive = source.replace('using V :: #import "renamed/Public";\n', '').replace(
        '    value: V.Value; value.number = Own.LocalNumber\n'
        '    return P.WrappedEcho(value).number - Short.LocalNumber',
        '    return P.RoundTrip(Own.LocalNumber) - Short.LocalNumber')
    assert 'renamed/Public' not in transitive
    write(app / 'src/app.zi', transitive)
    call(ziran, 'lock', '--offline', cwd=app, env=env)
    # Old launchers reject our own public export on this first assertion.
    # Library is reachable only through outer -> inner in this graph.
    call(ziran, 'check', '--project', '--offline', 'src/app.zi', cwd=app, env=env)
    write(app / 'ziran.toml', manifest)
    write(app / 'src/app.zi', source)
    call(ziran, 'lock', '--offline', cwd=app, env=env)
    compile_native(app / 'build/native', app / 'src/app.zi', True)
    ir = app / 'build/ir'
    call(ziran, 'ir', '--project', '--offline', '--entry', 'app:main', '-o', str(ir),
         'src/app.zi', cwd=app, env=env)
    compile_native(app / 'build/saved', ir / 'app.zir', False)

    # Public self exports do not expose private modules or transitive aliases.
    for path, good, spelling in (
            (app / 'src/app.zi', source, 'Application/private'),
            (leaf / 'src/entry.zi', entry, 'Library/private'),
            (app / 'src/app.zi', source, 'inner/Public'),
            (app / 'src/app.zi', source, 'Library/Public')):
        write(path, '#import "' + spelling + '"\n' + good)
        refused = call(ziran, 'check', '--project', '--offline', 'src/app.zi',
                       cwd=app, env=env, succeed=False)
        assert 'module ' + spelling + ' is not exported' in refused, refused
        write(path, good)
    # A dependency alias matching the manifest name retains its namespace.
    write(app / 'ziran.toml', manifest + f'\n[dependencies.Application]\ngit = "{leaf.as_uri()}"\n')
    collision = '\n'.join([
        'using D :: #import "Application/PublicLocal";',
        'using Short :: #import "local";',
        'main :: () -> s32 { value: D.Value; value.number = Short.LocalNumber; return value.number - 3 }', ''])
    write(app / 'src/app.zi', collision)
    call(ziran, 'lock', '--offline', cwd=app, env=env)
    compile_native(app / 'build/collision', app / 'src/app.zi', True)
    write(app / 'src/app.zi', '#import "Application/OnlyOwn"\n' + collision)
    refused = call(ziran, 'check', '--project', '--offline', 'src/app.zi',
                   cwd=app, env=env, succeed=False)
    assert 'module Application/OnlyOwn is not exported' in refused, refused
    print('PASS: public self imports, aliases, transitive identity, visibility, prefix precedence and native/saved IR', flush=True)


def main() -> None:
    compiler = Path(__file__).resolve().parents[1]
    scratch = compiler / 'build/scratch'
    scratch.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='package-self-import-', dir=scratch) as directory:
        root = Path(directory)
        env = {k: v for k, v in os.environ.items() if k not in ('DISPLAY', 'WAYLAND_DISPLAY')}
        env['XDG_CACHE_HOME'] = str(root / 'cache')
        env['ZIRAN_TEST_ALLOW_LOCAL_GIT'] = '1'
        check_self_imports(str(Path(sys.argv[1]).resolve()), root, compiler, env)


if __name__ == '__main__':
    main()
