#!/bin/sh
set -eu
exec python3 - "$1" <<'PY'
import base64
import hashlib
import http.server
import json
import os
from pathlib import Path
import shutil
import ssl
import struct
import subprocess
import sys
import tempfile
import threading


def library_sections(image):
    magic, version, count, size = struct.unpack_from('<4sIII', image)
    assert magic == b'ZXLB' and version in (1, 2) and size == len(image)
    position = 16
    sections = []
    for _ in range(count):
        name_length, length, sha = struct.unpack_from('<II32s', image, position)
        position += 40
        name = image[position:position + name_length]
        position += name_length
        contents = image[position:position + length]
        position += length
        assert len(contents) == length and hashlib.sha256(contents).digest() == sha
        sections.append((name, contents))
    assert position == size and [name for name, _ in sections] == sorted({name for name, _ in sections})
    return sections


def make_library(sections, version=1):
    body = b''.join(struct.pack('<II32s', len(name), len(data), hashlib.sha256(data).digest()) + name + data
                    for name, data in sections)
    return struct.pack('<4sIII', b'ZXLB', version, len(sections), 16 + len(body)) + body


class Registry(http.server.BaseHTTPRequestHandler):
    def log_message(self, *_):
        pass

    def response(self, status, body=b''):
        self.send_response(status)
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_PUT(self):
        self.server.requests.append(('PUT', self.path))
        length = int(self.headers['Content-Length'])
        body = self.rfile.read(length)
        if self.headers.get('Authorization') != 'Basic ' + base64.b64encode(b'fixture:fixture-token').decode():
            self.response(401)
        elif self.path in self.server.images:
            self.response(409)
        else:
            self.server.images[self.path] = body
            self.response(201)

    def do_GET(self):
        self.server.requests.append(('GET', self.path))
        self.server.read_authorization.append(self.headers.get('Authorization'))
        body = self.server.images.get(self.path)
        if body is None:
            self.response(404)
        else:
            self.response(200, body)


ziran = Path(sys.argv[1]).resolve()
zi2zx = ziran.with_name('zi2zx')
with tempfile.TemporaryDirectory(prefix='package-zx-test-', dir=ziran.parent.parent) as directory:
    root = Path(directory)
    env = os.environ.copy()
    env.update(XDG_CACHE_HOME=str(root / 'cache'), NO_PROXY='localhost', no_proxy='localhost')
    for key in ('DISPLAY', 'WAYLAND_DISPLAY', 'CURL_SSL_BACKEND', 'ZIRAN_DIAGNOSTICS', 'ZIRAN_PACKAGE_NETRC'):
        env.pop(key, None)

    def command(*arguments, failure=None, cwd=None):
        result = subprocess.run(arguments, cwd=cwd or root, env=env, capture_output=True, text=True, timeout=30)
        if failure is None:
            assert result.returncode == 0, f'{arguments}: {result.stdout}{result.stderr}'
            return result.stdout.strip()
        assert result.returncode != 0 and failure in result.stderr, f'{arguments}: {result.stdout}{result.stderr}'

    # Two pinned revisions of one package coexist. Generated C gives a
    # #program_export procedure one symbol, so when both revisions define one
    # that the program calls, the C build names the clash and the fix instead
    # of failing in the C compiler (Kryon's LightPalette from two Kryon pins).
    graph = root / 'two-revisions'
    repo = Path(command('git', 'rev-parse', '--show-toplevel', cwd=ziran.parent))
    graph_env = dict(env, XDG_CACHE_HOME=str(graph / 'cache'), ZIRAN_TEST_ALLOW_LOCAL_GIT='1',
                     GIT_CONFIG_COUNT='1', GIT_CONFIG_KEY_0='protocol.file.allow', GIT_CONFIG_VALUE_0='always')
    fixture_author = ('-c', 'user.name=Package fixture', '-c', 'user.email=fixture@example.org',
                      '-c', 'commit.gpgsign=false')

    def graph_run(*arguments, cwd):
        return subprocess.run(arguments, cwd=cwd, env=graph_env, capture_output=True, text=True, timeout=120)

    def graph_package(name, manifest, files):
        directory = graph / name
        directory.mkdir(parents=True)
        (directory / 'ziran.toml').write_text(manifest)
        for path, text in files.items():
            (directory / path).parent.mkdir(parents=True, exist_ok=True)
            (directory / path).write_text(text)
        return directory

    def graph_commit(directory):
        for arguments in (('init', '-q', '-b', 'master'), ('add', '.'), (*fixture_author, 'commit', '-qm', 'fixture')):
            assert graph_run('git', *arguments, cwd=directory).returncode == 0
        return graph_run('git', 'rev-parse', 'HEAD', cwd=directory).stdout.strip()

    def graph_dependency(alias, ref):
        return f'[dependencies.{alias}]\ngit = "file://{graph / alias}"\nref = "{ref}"\n'

    kit = graph_package('kit', '[package]\nname = "kit"\nmodule_roots = ["."]\n[exports]\nKit = "kit.zi"\n',
                        {'kit.zi': '#program_export\nLightPalette :: () -> s32 { return 1 }\n'})
    kit_old = graph_commit(kit)
    (kit / 'kit.zi').write_text('#program_export\nLightPalette :: () -> s64 { return 2 }\n')
    assert graph_run('git', *fixture_author, 'commit', '-qam', 'new', cwd=kit).returncode == 0
    kit_new = graph_run('git', 'rev-parse', 'HEAD', cwd=kit).stdout.strip()
    for name, ref in (('css', kit_new), ('theme', kit_old)):
        graph_commit(graph_package(name, f'[package]\nname = "{name}"\nmodule_roots = ["."]\n[exports]\n'
                                   f'Api = "api.zi"\n' + graph_dependency('kit', ref), {
                                       'api.zi': f'#import "kit/Kit"\n{name}Light :: () -> s64 '
                                                 '{ return cast(s64)LightPalette() }\n'}))
    both = graph_package('both', '[package]\nname = "app"\nentry = "src/app.zi"\nmodule_roots = ["src"]\n'
                         '[toolchain]\ngit = "https://github.com/ziranlang/ziran.git"\nref = "master"\n' +
                         graph_dependency('css', 'master') + graph_dependency('theme', 'master'), {
                             'ziran.local.toml': f'[overrides]\nziran = "{repo}"\n',
                             'src/app.zi': '#import "css/Api"\n#import "theme/Api"\n'
                                           'main :: () -> s32 { return cast(s32)(cssLight() + themeLight()) }\n'})
    assert graph_run(ziran, 'lock', cwd=both).returncode == 0
    revisions = [item['commit'] for item in json.loads((both / 'ziran.lock').read_text())['packages']
                 if item['name'] == 'kit']
    assert sorted(revisions) == sorted([kit_old, kit_new]), revisions
    checked = graph_run(ziran, 'check', '--project', cwd=both)
    assert checked.returncode != 0 and (
        'the dependency name kit is pinned twice' in checked.stderr) and (
        'give one dependency an explicit alias' in checked.stderr), checked.stderr
    refused = graph_run(ziran, 'build', cwd=both)
    assert refused.returncode != 0 and 'the dependency name kit is pinned twice' in refused.stderr, refused.stderr
    theme = graph / 'theme'
    (theme / 'ziran.toml').write_text((theme / 'ziran.toml').read_text().replace(kit_old, kit_new))
    assert graph_run('git', *fixture_author, 'commit', '-qam', 'agree', cwd=theme).returncode == 0
    assert graph_run(ziran, 'update', cwd=both).returncode == 0
    built = graph_run(ziran, 'build', cwd=both)
    assert built.returncode == 0, built.stderr
    assert graph_run(both / 'build/app', cwd=both).returncode == 4
    # `ziran api PACKAGE/MODULE` in a project prints that module as an import
    # names it, options may follow the file, and a name that selects nothing
    # is an error that lists the modules.
    api = graph_run(ziran, 'api', 'css/Api', cwd=both)
    assert api.returncode == 0 and 'cssLight :: () -> s64' in api.stdout and 'themeLight' not in api.stdout, api.stdout + api.stderr
    api = graph_run(ziran, 'api', 'kit/Kit', cwd=both)
    assert api.returncode == 0 and 'LightPalette :: () -> s64' in api.stdout and 'cssLight' not in api.stdout, api.stdout + api.stderr
    api = graph_run(ziran, 'api', 'src/app.zi', '--module', 'app', cwd=both)
    assert api.returncode == 0 and 'main :: () -> s32' in api.stdout and 'cssLight ::' not in api.stdout, api.stdout + api.stderr
    api = graph_run(ziran, 'api', 'css/Nothing', cwd=both)
    assert api.returncode != 0 and '--module css/Nothing names no checked module' in api.stderr and 'Modules: app' in api.stderr, api.stderr

    key, cert = root / 'key.pem', root / 'cert.pem'
    command('openssl', 'req', '-new', '-x509', '-newkey', 'rsa:2048', '-nodes', '-days', '1',
            '-subj', '/CN=localhost', '-addext', 'subjectAltName=DNS:localhost', '-keyout', key, '-out', cert)
    env['CURL_CA_BUNDLE'] = str(cert)
    netrc = root / 'netrc'
    netrc.write_text('machine localhost login fixture password fixture-token\n')
    netrc.chmod(0o600)

    provider = root / 'counter.zi'
    provider.write_text('''using Modules :: #import "std/zx_module"
host_api :: #system_library "host_api";
EchoExternal :: (handle: u64, amount: s32) -> s32 #foreign host_api;
Add :: (amount: s32) -> s32 {
    handle := Modules.Load("Echo.v1");
    if handle == 0 { return 1; }
    result := EchoExternal(handle, amount);
    if !Modules.Release(handle) { return 2; }
    return result + 40;
}
''')
    echo = root / 'echo.zi'
    echo.write_text('Echo :: (amount: s32) -> s32 { return amount; }\n')
    image, other, echo_image = (root / f'{name}.zx' for name in ('counter', 'other', 'echo'))
    for name, path in (('demo/counter', image), ('demo/other', other)):
        command(zi2zx, '--library', name, '--require-module', 'Echo.v1=demo/echo',
                '--import', 'counter:EchoExternal=Echo.v1:Echo', '--export', 'counter:Add', '-o', path, provider)
    command(zi2zx, '--library', 'demo/echo', '--export', 'echo:Echo', '-o', echo_image, echo)
    expected = hashlib.sha256(image.read_bytes()).hexdigest()
    echo_digest = hashlib.sha256(echo_image.read_bytes()).hexdigest()
    revision = 'a' * 40

    server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Registry)
    tls = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    tls.load_cert_chain(cert, key)
    server.socket = tls.wrap_socket(server.socket, server_side=True)
    server.requests = []
    server.read_authorization = []
    server.images = {}
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    try:
        repository = f'https://localhost:{server.server_port}/ziranlang/fixture.git'
        artifact = f'/api/packages/ziranlang/generic/fixture-zx/{revision}/counter.zx'
        package = [ziran, 'pkg', 'zx']
        upload = [*package, 'publish', repository, revision, 'counter']
        download = [*package, 'fetch', repository, revision, 'counter']
        command(*upload, image, failure='HTTP 401')
        assert command(*upload, image, '--netrc', netrc) == expected
        assert server.images[artifact] == image.read_bytes()
        assert command(*upload, image, '--netrc', netrc) == expected
        command(*upload, other, '--netrc', netrc, failure='already has different bytes')
        assert command(*package, 'publish', repository, revision, 'echo', echo_image, '--netrc', netrc) == echo_digest
        provider.unlink()
        image.unlink()
        other.unlink()
        echo.unlink()
        echo_image.unlink()

        command(*download, 'f' * 64, failure='failed digest, format or module-name verification')
        command(*download, expected, '--module', 'demo/other', failure='failed digest, format or module-name verification')
        command(*download, expected, '--module', '../counter', failure='invalid portable package option')
        command(*download, expected, '--module', 'demo/counter', '--offline', failure='absent from the offline cache')

        module_root = Path(command(*download, expected, '--module', 'demo/counter'))
        fetched = module_root / 'demo/counter.zx'
        assert fetched.read_bytes() == server.images[artifact] and not provider.exists()
        request_count = len(server.requests)
        assert Path(command(*download, expected, '--module', 'demo/counter', '--offline')) == module_root
        assert len(server.requests) == request_count

        app = root / 'app.zi'
        app.write_text('''using Modules :: #import "std/zx_module"
host_api :: #system_library "host_api";
AddExternal :: (handle: u64, amount: s32) -> s32 #foreign host_api;
main :: () -> s32 {
    handle := Modules.Load("Counter.v1");
    if handle == 0 { return 1; }
    result := AddExternal(handle, 2);
    if !Modules.Release(handle) { return 2; }
    return result;
}
''')
        lock = root / 'ziran.zx.lock'
        pinned = {'format': 1, 'modules': [{'interface': 'Counter.v1', 'name': 'demo/counter',
                  'repository': repository, 'commit': revision, 'profile': 'counter', 'sha256': expected},
                  {'interface': 'Echo.v1', 'name': 'demo/echo', 'repository': repository, 'commit': revision,
                   'profile': 'echo', 'sha256': echo_digest}],
                  'imports': [{'module': 'app', 'function': 'AddExternal', 'interface': 'Counter.v1', 'export': 'Add'}]}
        lock.write_text(json.dumps(pinned))
        build = [ziran, 'bundle', '--zx-lock', lock, '--root', root, '--entry', 'app:main', '-o']
        app_image = root / 'app.zx'
        command(*build, app_image, app)
        run = [ziran, 'run', '--backend', 'engine', '--zx-lock', lock]
        command(*run, '--offline', app_image, failure='absent from the offline cache')
        assert command(*run, app_image) == '42'
        assert len(server.requests) == request_count + 1
        echo_root = Path(command(*package, 'fetch', repository, revision, 'echo', echo_digest, '--module', 'demo/echo', '--offline'))
        echo_fetched = echo_root / 'demo/echo.zx'
        request_count = len(server.requests)
        assert command(*run, '--offline', app_image) == '42'
        assert len(server.requests) == request_count
        command(*run, '--module-root', echo_root, app_image, failure='supplies the complete library image map')
        # A valid transitive image placed in a searched host root cannot
        # satisfy a requirement omitted from the exact lock.
        pinned_echo = pinned['modules'].pop()
        lock.write_text(json.dumps(pinned))
        loose = fetched.parent / 'echo.zx'
        loose.write_bytes(echo_fetched.read_bytes())
        assert command(ziran, 'run', '--backend', 'engine', '--module-root', module_root, app_image) == '42'
        command(*run, '--offline', app_image, failure='ZX module not found: demo/echo')
        loose.unlink()
        pinned['modules'].append(pinned_echo)
        lock.write_text(json.dumps(pinned))
        command(zi2zx, 'run', '--backend', 'engine', '--module-image', f'demo/counter={expected}={fetched}',
                '--module-image', f'demo/echo={"f" * 64}={echo_fetched}', app_image,
                failure='cannot resolve ZX module demo/echo')
        fetched.unlink()
        command(*run, '--offline', app_image, failure='absent from the offline cache')
        assert command(*run, app_image) == '42'
        assert len(server.requests) == request_count + 1

        pinned['imports'][0]['interface'] = 'Unpinned'
        lock.write_text(json.dumps(pinned))
        command(*build, app_image, app, failure='unpinned interface alias')
        pinned['imports'][0]['interface'] = 'Counter.v1'
        lock.write_text(json.dumps(pinned))
        app.write_text(app.read_text().replace('amount: s32', 'amount: s64'))
        command(*build, app_image, app)
        command(*run, '--offline', app_image, failure='signature')

        # No installed host provider may take over a locked run. An ungranted
        # external call must fail in the runtime with the pins still enforced.
        hosts = root / 'hosts'
        hosts.mkdir()
        fake_host = hosts / 'provider'
        marker = hosts / 'invoked'
        fake_host.write_text(f'#!/bin/sh\ntouch "{marker}"\nexit 0\n')
        fake_host.chmod(0o755)
        identity = subprocess.run([ziran, 'version', '--identity'], env=env, check=True,
                                  capture_output=True, text=True).stdout.strip()
        (hosts / 'fake.host').write_text(f'{fake_host}\nziran {identity}\n')
        env['ZIRAN_HOST_PATH'] = str(hosts)
        app.write_text('''host_api :: #system_library "host_api";
External :: () -> s32 #foreign host_api;
main :: () -> s32 { return External(); }
''')
        command(ziran, 'bundle', '--root', root, '--entry', 'app:main', '-o', app_image, app)
        command(*run, '--offline', app_image, failure='host')
        assert not marker.exists()
        command(ziran, 'run', '--backend', 'engine', app_image)
        assert marker.exists()

        broken = server.images[artifact][0:60]
        server.images[artifact] = broken
        command(*download, hashlib.sha256(broken).hexdigest(),
                failure='failed digest, format or module-name verification')
        fetched.write_bytes(broken)
        request_count = len(server.requests)
        command(*download, expected, '--module', 'demo/counter', failure='cached portable image failed')
        assert len(server.requests) == request_count
        assert ('PUT', artifact) in server.requests and ('GET', artifact) in server.requests

        # The package driver can select an archive before checking out its
        # provider, while source imports and mutable types keep their meaning.
        source = root / 'provider'
        source.mkdir()
        (source / 'src').mkdir()
        provider_module = source / 'src/state.zi'
        provider_module.write_text('''Update :: #type (value: s32) -> s32;
State :: struct { value: s32; handler: Update; }
Advance :: (state: *State) { state.value = state.handler(state.value); }
Copy :: (value: $T) -> T { return value; }
''')
        toolchain = Path(command('git', '-C', ziran.parent, 'rev-parse', '--show-toplevel'))
        tool_url = 'https://git.waozi.xyz/ziranlang/ziran.git'
        tool_revision = command('git', '-C', toolchain, 'rev-parse', 'HEAD')
        tool_pin = {'url': tool_url, 'ref': tool_revision, 'commit': tool_revision}
        tool_section = f'[toolchain]\ngit = "{tool_url}"\nref = "{tool_revision}"\n'
        (source / 'ziran.toml').write_text('[package]\nname = "state"\nmodule_roots = ["src"]\n' +
                                           tool_section + '[exports]\nmodel = "src/state.zi"\n')
        command('git', '-C', source, 'init', '-q')
        command('git', '-C', source, 'add', 'src/state.zi', 'ziran.toml')
        command('git', '-C', source, '-c', 'user.name=Package fixture', '-c', 'user.email=fixture@example.org',
                '-c', 'commit.gpgsign=false', 'commit', '-qm', 'Package fixture')
        checked_revision = command('git', '-C', source, 'rev-parse', 'HEAD')

        def project(directory, name, rows, edges):
            (directory / 'ziran.lock').write_text(json.dumps({'format': 1, 'packages': rows,
                'root': {'name': name, 'dependencies': edges}, 'toolchain': tool_pin}))
            (directory / 'ziran.local.toml').write_text(f'[overrides]\nziran = "{toolchain}"\n')

        # Put the source lock and local override in the fixture's ignore list.
        (source / '.git/info/exclude').write_text('build/\nziran.local.toml\nziran.lock\n')
        project(source, 'state', [], {})
        command('git', '-C', source, 'config', 'remote.origin.url', repository)
        archive = root / 'state.zx'
        command(*package, 'pack-ir', repository, checked_revision, 'default', archive, cwd=source)
        library = archive.read_bytes()
        assert struct.unpack_from('<I', library, 4)[0] == 1
        archive_digest = hashlib.sha256(library).hexdigest()
        sections = library_sections(library)
        assert [name for name, _ in sections] == [b'checked-package.tsv', b'src/state.zir', b'ziran.toml']
        inspection = command(*package, 'inspect-ir', archive)
        assert inspection == sections[0][1].decode().strip() + '\t' + archive_digest
        repeated = root / 'repacked.zx'
        command(*package, 'pack-ir', repository, checked_revision, 'default', repeated, cwd=source)
        assert repeated.read_bytes() == library
        malformed = root / 'malformed.zx'
        for invalid in (b'PK\x03\x04' + library[4:],
                        library[:4] + struct.pack('<I', 2) + library[8:],
                        library[:8] + struct.pack('<I', 4099) + library[12:],
                        library[:16] + struct.pack('<I', 0xffffffff) + library[20:],
                        library[:-1], library + b'\0', library[:-1] + bytes([library[-1] ^ 1]),
                        make_library(sections[::-1]), make_library(sections[:1] + sections),
                        make_library([(b'../state.zir', sections[1][1]), *sections])):
            malformed.write_bytes(invalid)
            command(*package, 'inspect-ir', malformed, failure='cannot verify the checked .zx library')
        assert command(*package, 'publish-ir', repository, checked_revision, 'default', archive,
                       '--netrc', netrc) == archive_digest
        assert command(*package, 'publish-ir', repository, checked_revision, 'default', archive,
                       '--netrc', netrc) == archive_digest
        command(*package, 'publish', repository, checked_revision, 'default', archive,
                '--netrc', netrc, failure='cannot verify the portable image')
        command(*package, 'publish-ir', repository, checked_revision, 'other', archive,
                '--netrc', netrc, failure='failed format, revision or profile verification')
        alternate = root / 'alternate.zx'
        command(*package, 'pack-ir', repository, checked_revision, 'other', alternate, cwd=source)
        alternate_digest = command(*package, 'publish-ir', repository, checked_revision, 'other', alternate, '--netrc', netrc)
        developer = root / 'developer.zx'
        dev_pack = [*package, 'pack-dev', repository, checked_revision, 'default', developer, '--offline']
        command(*dev_pack, cwd=source)
        original_dev = command(*package, 'inspect-ir', developer).split('\t')
        provider_module.write_text(provider_module.read_text().replace('state.handler(state.value);',
                                                                      'state.handler(state.value) + 5;'))
        command(*dev_pack, cwd=source)
        current_dev = command(*package, 'inspect-ir', developer).split('\t')
        assert developer.read_bytes()[:4] == b'ZXLD' and current_dev[0] == 'dev'
        assert len(current_dev) == 6 and original_dev[:4] == current_dev[:4]
        assert original_dev[4] != current_dev[4] and len(current_dev[4]) == 64
        assert current_dev[5] == hashlib.sha256(developer.read_bytes()).hexdigest()
        command(*package, 'pack-ir', repository, checked_revision, 'default', root / 'dirty.zx',
                cwd=source, failure='requires the clean declared repository revision')
        command(*dev_pack, '--locked', cwd=source, failure='pack-dev cannot be used with --locked')
        before = len(server.requests)
        command(*package, 'publish-ir', repository, checked_revision, 'default', developer,
                '--netrc', netrc, failure='failed format, revision or profile verification')
        assert len(server.requests) == before
        dev_export = f'/api/packages/ziranlang/generic/fixture-zx-library/{checked_revision}/default.zx'
        server.images[dev_export] = developer.read_bytes()
        command(*package, 'fetch-ir', repository, checked_revision, 'default', current_dev[5],
                failure='failed digest, format or module-name verification')
        server.images[dev_export] = library
        shutil.rmtree(source)
        archive.unlink()
        alternate.unlink()
        consumer = root / 'consumer'
        consumer.mkdir()
        (consumer / 'ziran.toml').write_text('[package]\nname = "consumer"\nmodule_roots = ["."]\n' + tool_section +
                                             f'[dependencies.state_v2]\ngit = "{repository}"\nref = "{checked_revision}"\n')
        identity = 'localhost_ziranlang_fixture_' + checked_revision[:12]
        row = {'id': identity, 'name': 'state', 'url': repository, 'ref': checked_revision,
               'commit': checked_revision, 'dependencies': {}}
        project(consumer, 'consumer', [row], {'state_v2': identity})
        pins = consumer / 'ziran.zx.lock'
        app = consumer / 'app.zi'
        app.write_text('''using Model :: #import "state_v2/model"
Increment :: (value: s32) -> s32 { return value + 2; }
main :: () -> s32 {
    state := Model.State.{value = Model.Copy(cast(s32)40), handler = Increment};
    Model.Advance(*state);
    return state.value;
}
''')
        built = consumer / 'app.zx'
        argv = [ziran, 'bundle', '--project', '--entry', 'app:main', '-o', built, app]
        dev_dir = root / 'dev-libraries'
        dev_dir.mkdir()
        developer.rename(dev_dir / f'{identity}.zx')
        env['ZIRAN_DEV_ZX_DIR'] = str(dev_dir)
        before = len(server.requests)
        command(*argv, '--offline', cwd=consumer)
        assert command(ziran, 'run', '--backend', 'engine', built) == '47'
        assert len(server.requests) == before and not pins.exists()
        command(*argv, '--offline', '--locked', cwd=consumer, failure='ZIRAN_DEV_ZX_DIR cannot be used with --locked')
        env.pop('ZIRAN_DEV_ZX_DIR')
        pins.write_text(json.dumps({'format': 1, 'packages': [{'package': identity,
            'profile': 'default', 'sha256': archive_digest}]}))
        command(*argv, '--offline', failure='absent from the offline cache', cwd=consumer)
        command(*argv, cwd=consumer)
        assert command(ziran, 'run', '--backend', 'engine', built) == '42'
        request_count = len(server.requests)
        command(*argv, '--offline', cwd=consumer)
        fetched_library = Path(command(*package, 'fetch-ir', repository, checked_revision, 'default',
                                       archive_digest, '--offline'))
        assert fetched_library.read_bytes() == library
        assert len(server.requests) == request_count

        # Exercise an exact locked tool pin in an isolated fixture cache with
        # the native tools built for this test. The provider has no overrides.
        web_cache = root / 'web-cache'
        env['XDG_CACHE_HOME'] = str(web_cache)
        fixture_tool = root / 'fixture-tool'
        fixture_tool.mkdir()

        def commit_fixture(directory):
            command('git', 'init', '-q', cwd=directory)
            command('git', 'add', '.', cwd=directory)
            command('git', '-c', 'user.name=Package fixture', '-c', 'user.email=fixture@example.org',
                    '-c', 'commit.gpgsign=false', 'commit', '-qm', 'Package fixture', cwd=directory)
            return command('git', 'rev-parse', 'HEAD', cwd=directory)

        (fixture_tool / '.gitignore').write_text('build/\n')
        (fixture_tool / 'std').mkdir()
        (fixture_tool / 'std/.gitkeep').touch()
        web_tool_revision = commit_fixture(fixture_tool)
        web_tool_id = 'p' + hashlib.sha256(f'{tool_url}\n{web_tool_revision}'.encode()).hexdigest()[:16]
        web_tool = web_cache / 'ziran/sources' / web_tool_id
        web_tool.parent.mkdir(parents=True)
        fixture_tool.rename(web_tool)
        command('git', 'config', 'remote.origin.url', tool_url, cwd=web_tool)
        (web_tool / 'build/bin').mkdir(parents=True)
        for name in ('ziran', 'zi2zir', 'zi2zx'):
            shutil.copy2(ziran.with_name(name), web_tool / 'build/bin' / name)
        web_launcher = web_tool / 'build/bin/ziran'
        web_package = [web_launcher, 'pkg', 'zx']
        web_tool_pin = {'url': tool_url, 'ref': web_tool_revision, 'commit': web_tool_revision}
        web_tool_section = tool_section.replace(tool_revision, web_tool_revision)
        web_provider = root / 'web-provider'
        web_provider.mkdir()
        (web_provider / '.gitignore').write_text('build/\n')
        (web_provider / 'web.zi').write_text('''#if OS == .WEB && CPU == .WASM32 && #defined(FIXTURE_WEB) {
    Answer :: () -> s32 { return 42; }
} else {
    Fallback :: () -> s32 { return 1; }
}
''')
        (web_provider / 'ziran.toml').write_text('[package]\nname = "web"\nmodule_roots = ["."]\n' +
                                                web_tool_section + '[exports]\napi = "web.zi"\n')
        (web_provider / 'ziran.lock').write_text(json.dumps({'format': 1, 'packages': [],
            'root': {'name': 'web', 'dependencies': {}}, 'toolchain': web_tool_pin}))
        web_revision = commit_fixture(web_provider)
        command('git', 'config', 'remote.origin.url', repository, cwd=web_provider)
        web_archive = root / 'web.zx'
        web_pack = [*web_package, 'pack-ir', repository, web_revision, 'web', web_archive, '--offline']
        web_flags = ['--target-os', 'web', '--target-cpu', 'wasm32', '--define', 'FIXTURE_WEB']
        command(*web_pack, *web_flags, cwd=web_provider, failure='target selection requires --locked')
        dirty_marker = web_provider / 'untracked.txt'
        dirty_marker.touch()
        command(*web_pack, '--locked', *web_flags, cwd=web_provider,
                failure='requires the clean declared repository revision')
        dirty_marker.unlink()
        command(*web_pack, '--locked', '--target-os', cwd=web_provider, failure='pack-ir')
        for bad_flags in (['--target-os', 'unknown'], ['--target-cpu', '--help-me'],
                          ['--target-os', 'web', '--target-os', 'linux'],
                          ['--target-cpu', 'wasm32', '--target-cpu', 'x64'], ['--target=c', '--root']):
            command(*web_pack, '--locked', *bad_flags, cwd=web_provider, failure='invalid checked package option')
            assert not web_archive.exists()
        command(*web_pack, '--locked', *web_flags, cwd=web_provider)
        web_bytes = web_archive.read_bytes()
        assert struct.unpack_from('<I', web_bytes, 4)[0] == 2
        web_sections = library_sections(web_bytes)
        assert dict(web_sections)[b'checked-target.tsv'] == b'web\twasm32\n'
        web_digest = hashlib.sha256(web_bytes).hexdigest()
        assert command(*web_package, 'inspect-ir', web_archive).endswith('\tweb\t' + web_digest)
        command(*web_pack, '--locked', '--target-os', 'web', '--define', 'FIXTURE_WEB', cwd=web_provider)
        assert web_archive.read_bytes() == web_bytes
        without_target = [(name, data) for name, data in web_sections if name != b'checked-target.tsv']
        for invalid in (make_library(web_sections), make_library(without_target, version=2),
                        make_library([(name, b'web\tunknown\n' if name == b'checked-target.tsv' else data)
                                      for name, data in web_sections], version=2)):
            malformed.write_bytes(invalid)
            command(*web_package, 'inspect-ir', malformed, failure='cannot verify the checked .zx library')
        assert command(*web_package, 'publish-ir', repository, web_revision, 'web', web_archive,
                       '--netrc', netrc) == web_digest
        web_artifact = f'/api/packages/ziranlang/generic/fixture-zx-library/{web_revision}/web.zx'
        changed_target = make_library([(name, b'linux\tx64\n' if name == b'checked-target.tsv' else data)
                                       for name, data in web_sections], version=2)
        server.images[web_artifact] = changed_target
        command(*web_package, 'fetch-ir', repository, web_revision, 'web', web_digest,
                failure='failed digest, format or module-name verification')
        server.images[web_artifact] = web_bytes
        shutil.rmtree(web_provider)
        web_archive.unlink()

        web_consumer = root / 'web-consumer'
        web_consumer.mkdir()
        (web_consumer / 'ziran.toml').write_text('[package]\nname = "consumer"\nmodule_roots = ["."]\n' + web_tool_section)
        env.update(ZIRAN_ZX_REGISTRY=f'https://localhost:{server.server_port}', ZIRAN_ZX_PROFILE='web')
        before = len(server.requests)
        command(web_launcher, 'add', repository, '--name', 'web', '--ref', web_revision, cwd=web_consumer)
        assert server.requests[before:] == [('GET', web_artifact + '.tsv'), ('GET', web_artifact)]
        assert server.read_authorization[-2:] == [None, None]
        web_app = web_consumer / 'app.zi'
        web_app.write_text('using Web :: #import "web/api"\nmain :: () -> s32 { return Web.Answer(); }\n')
        web_output = web_consumer / 'app.zx'
        command(web_launcher, 'bundle', '--project', '--locked', '--offline', '--entry', 'app:main',
                '-o', web_output, web_app, cwd=web_consumer)
        assert command(ziran, 'run', '--backend', 'engine', web_output) == '42'
        assert len(server.requests) == before + 2 and set(web_tool.parent.iterdir()) == {
            web_tool, web_tool.with_suffix('.lock')}
        env['XDG_CACHE_HOME'] = str(root / 'cache')
        env.pop('ZIRAN_ZX_REGISTRY')
        env.pop('ZIRAN_ZX_PROFILE')

        # Ordinary add discovers the published descriptor and resolves its
        # package graph from the archive before attempting any Git checkout.
        automatic = root / 'automatic'
        automatic.mkdir()
        bare_manifest = '[package]\nname = "consumer"\nmodule_roots = ["."]\nentry = "app.zi"\n' + tool_section
        automatic_manifest = automatic / 'ziran.toml'
        automatic_manifest.write_text(bare_manifest)
        (automatic / 'ziran.local.toml').write_text(f'[overrides]\nziran = "{toolchain}"\n')
        auto_cache = root / 'automatic-cache'
        env.update(XDG_CACHE_HOME=str(auto_cache), ZIRAN_ZX_REGISTRY=f'https://localhost:{server.server_port}')
        checked_artifact = f'/api/packages/ziranlang/generic/fixture-zx-library/{checked_revision}/default.zx'
        descriptor = checked_artifact + '.tsv'
        good_metadata = server.images[descriptor]
        expected_tool = 'waozi_xyz_ziranlang_ziran_' + tool_revision[:12]
        assert good_metadata == f'2\t{identity}\t{expected_tool}\tdefault\t{archive_digest}\n'.encode()
        add = [ziran, 'add', repository, '--name', 'state', '--ref', checked_revision]
        # A library checked under another toolchain leaves the package on its
        # source: no archive is fetched and the add goes on to the Git
        # checkout, which this registry-only fixture server cannot serve.
        server.images[descriptor] = good_metadata.replace(expected_tool.encode(), b'waozi_xyz_ziranlang_ziran_000000000000')
        before = len(server.requests)
        env['XDG_CACHE_HOME'] = str(root / 'other-toolchain-cache')
        command(*add, failure='cannot check out', cwd=automatic)
        env['XDG_CACHE_HOME'] = str(auto_cache)
        assert ('GET', checked_artifact) not in server.requests[before:]
        assert automatic_manifest.read_text() == bare_manifest and not (automatic / 'ziran.lock').exists()
        # A descriptor naming another package is still refused.
        other_package = identity[:-1] + ('1' if identity[-1] == '0' else '0')
        server.images[descriptor] = good_metadata.replace(identity.encode(), other_package.encode())
        command(*add, failure='metadata does not match its repository revision, toolchain or profile', cwd=automatic)
        assert automatic_manifest.read_text() == bare_manifest and not (automatic / 'ziran.lock').exists()
        server.images[descriptor] = good_metadata
        good_archive = server.images.pop(checked_artifact)
        command(*add, failure='cannot fetch the portable image from Gitea (HTTP 404)', cwd=automatic)
        assert automatic_manifest.read_text() == bare_manifest and not (automatic / 'ziran.lock').exists()
        server.images[checked_artifact] = good_archive
        before = len(server.requests)
        assert 'checked package archive' in command(*add, cwd=automatic)
        assert server.requests[before:] == [('GET', descriptor), ('GET', checked_artifact)]
        automatic_lock = automatic / 'ziran.lock'
        locked = json.loads(automatic_lock.read_text())
        assert locked['packages'][0]['checked'] == {
            'profile': 'default', 'sha256': archive_digest, 'toolchain': expected_tool,
        }
        assert not (automatic / 'ziran.zx.lock').exists()

        auto_app = automatic / 'app.zi'
        auto_app.write_text(app.read_text().replace('state_v2/model', 'state/model'))
        auto_output = automatic / 'app.zx'
        auto_bundle = [ziran, 'bundle', '--project', '--offline', '--entry', 'app:main', '-o', auto_output, auto_app]
        before = len(server.requests)
        saved = automatic_lock.read_bytes()
        command(ziran, 'lock', '--offline', cwd=automatic)
        assert automatic_lock.read_bytes() == saved
        command(*auto_bundle, cwd=automatic)
        assert command(ziran, 'run', '--backend', 'engine', auto_output) == '42'
        command(ziran, 'check', '--offline', cwd=automatic)
        native_output = automatic / 'native'
        native_output.mkdir()
        command(ziran, 'build', '--project', '--offline', '--target=c', '--exe', '--entry', 'app:main',
                '-o', native_output, auto_app, cwd=automatic)
        native = subprocess.run([native_output / 'app'], cwd=automatic, check=False, timeout=30)
        assert native.returncode == 42
        command(ziran, 'build', '--offline', cwd=automatic)
        native = subprocess.run([automatic / 'build/consumer'], cwd=automatic, check=False, timeout=30)
        assert native.returncode == 42
        assert len(server.requests) == before and not (auto_cache / 'ziran/sources').exists()

        env['ZIRAN_ZX_PROFILE'] = 'missing'
        command(ziran, 'lock', failure='cannot discover checked package metadata from Gitea (HTTP 404)', cwd=automatic)
        assert automatic_lock.read_bytes() == saved
        env['ZIRAN_ZX_PROFILE'] = 'other'
        command(ziran, 'lock', cwd=automatic)
        assert json.loads(automatic_lock.read_text())['packages'][0]['checked'] == {
            'profile': 'other', 'sha256': alternate_digest, 'toolchain': expected_tool,
        }
        env.pop('ZIRAN_ZX_PROFILE')
        saved = automatic_lock.read_bytes()
        before = len(server.requests)
        command(*auto_bundle, cwd=automatic)
        assert command(ziran, 'run', '--backend', 'engine', auto_output) == '42'

        # A lock kept across a Ziran upgrade: its checked entry belongs to
        # another toolchain build. Builds name the command that fixes it, and
        # `ziran lock` and `ziran update` look the library up again for this
        # toolchain instead of refusing.
        refreshed = json.loads(saved)
        stale = json.loads(saved)
        stale['packages'][0]['checked']['toolchain'] = 'waozi_xyz_ziranlang_ziran_000000000000'
        stale_lock = json.dumps(stale)
        advice = 'for another Ziran toolchain build; run ziran lock to refresh it'
        for refresh in ('lock', 'update'):
            automatic_lock.write_text(stale_lock)
            command(*auto_bundle, failure=advice, cwd=automatic)
            command(*auto_bundle, '--locked', failure=advice, cwd=automatic)
            command(ziran, 'check', '--offline', failure=advice, cwd=automatic)
            command(ziran, refresh, cwd=automatic)
            assert json.loads(automatic_lock.read_text()) == refreshed
            command(*auto_bundle, cwd=automatic)
        # With nothing published for this toolchain the package goes back to
        # its source, which this registry-only server cannot serve; the lock
        # on disk keeps its bytes.
        automatic_lock.write_text(stale_lock)
        other_descriptor = descriptor.replace('default.zx', 'other.zx')
        other_metadata = server.images[other_descriptor]
        server.images[other_descriptor] = other_metadata.replace(expected_tool.encode(), b'p0000000000000000')
        env['XDG_CACHE_HOME'] = str(root / 'stale-toolchain-cache')
        command(ziran, 'lock', failure='cannot check out', cwd=automatic)
        env['XDG_CACHE_HOME'] = str(auto_cache)
        assert automatic_lock.read_text() == stale_lock
        server.images[other_descriptor] = other_metadata
        # A wrong package id or digest in the saved entry is still refused.
        for field, value in (('toolchain', 'not-an-identity'), ('sha256', 'abc')):
            broken = json.loads(saved)
            broken['packages'][0]['checked'][field] = value
            automatic_lock.write_text(json.dumps(broken))
            command(ziran, 'lock', failure='invalid ziran.lock', cwd=automatic)
        automatic_lock.write_bytes(saved)
        before = len(server.requests)

        override = root / 'override'
        override.mkdir()
        (override / 'src').mkdir()
        (override / 'ziran.toml').write_text('[package]\nname = "state"\nmodule_roots = ["src"]\n' +
                                             tool_section + '[exports]\nmodel = "src/state.zi"\n')
        (override / 'src/state.zi').write_text('''Update :: #type (value: s32) -> s32;
State :: struct { value: s32; handler: Update; }
Advance :: (state: *State) { state.value = state.handler(state.value) + 2; }
Copy :: (value: $T) -> T { return value; }
''')
        command('git', '-C', override, 'init', '-q')
        (automatic / 'ziran.local.toml').write_text(f'[overrides]\nziran = "{toolchain}"\nstate = "{override}"\n')
        command(*auto_bundle, cwd=automatic)
        assert command(ziran, 'run', '--backend', 'engine', auto_output) == '44'
        assert automatic_lock.read_bytes() == saved and len(server.requests) == before
        env['XDG_CACHE_HOME'] = str(root / 'cache')
        env.pop('ZIRAN_ZX_REGISTRY')

        pins.write_text('{"format": 1,')
        command(*argv, '--offline', failure='invalid checked packages in ziran.zx.lock', cwd=consumer)

        wrong_identity = f'2\t{identity}\twaozi_xyz_ziranlang_ziran_000000000000\tdefault\n'.encode()
        wrong_tool = make_library([(b'checked-package.tsv', wrong_identity), *sections[1:]])
        server.images[checked_artifact] = wrong_tool
        pins.write_text(json.dumps({'format': 1, 'packages': [{'package': identity, 'profile': 'default',
            'sha256': hashlib.sha256(wrong_tool).hexdigest()}]}))
        command(*argv, failure='archive does not match its pinned package, toolchain or profile', cwd=consumer)
    finally:
        server.shutdown()
        server.server_close()
        thread.join()

print('Portable package publish, download, typed source-free linking and cache checks passed')
PY
