#!/bin/sh
set -eu
exec python3 - "$1" <<'PY'
import base64
import hashlib
import http.server
import io
import json
import os
from pathlib import Path
import shutil
import ssl
import subprocess
import sys
import tempfile
import threading
import zipfile


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
        (hosts / 'fake.host').write_text(str(fake_host) + '\n')
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
        provider_module.write_text('''State :: struct { value: s32; }
Advance :: (state: *State) { state.value += 2; }
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
        archive = root / 'state.zir.zip'
        command(*package, 'pack-ir', repository, checked_revision, 'default', archive, cwd=source)
        archive_digest = hashlib.sha256(archive.read_bytes()).hexdigest()
        assert command(*package, 'publish-ir', repository, checked_revision, 'default', archive,
                       '--netrc', netrc) == archive_digest
        assert command(*package, 'publish-ir', repository, checked_revision, 'default', archive,
                       '--netrc', netrc) == archive_digest
        command(*package, 'publish-ir', repository, checked_revision, 'other', archive,
                '--netrc', netrc, failure='archive does not match the declared repository revision or profile')
        alternate = root / 'alternate.zir.zip'
        command(*package, 'pack-ir', repository, checked_revision, 'other', alternate, cwd=source)
        alternate_digest = command(*package, 'publish-ir', repository, checked_revision, 'other', alternate, '--netrc', netrc)
        shutil.rmtree(source)
        archive.unlink()
        alternate.unlink()
        consumer = root / 'consumer'
        consumer.mkdir()
        (consumer / 'ziran.toml').write_text('[package]\nname = "consumer"\nmodule_roots = ["."]\n' + tool_section +
                                             f'[dependencies.state]\ngit = "{repository}"\nref = "{checked_revision}"\n')
        identity = 'p' + hashlib.sha256(f'https://localhost/ziranlang/fixture.git\n{checked_revision}'.encode()).hexdigest()[:16]
        row = {'id': identity, 'name': 'state', 'url': repository, 'ref': checked_revision,
               'commit': checked_revision, 'dependencies': {}}
        project(consumer, 'consumer', [row], {'state': identity})
        pins = consumer / 'ziran.zx.lock'
        pins.write_text(json.dumps({'format': 1, 'packages': [{'package': identity,
            'profile': 'default', 'sha256': archive_digest}]}))
        app = consumer / 'app.zi'
        app.write_text('''using Model :: #import "state/model"
main :: () -> s32 {
    state := Model.State.{value = 40};
    Model.Advance(*state);
    return state.value;
}
''')
        built = consumer / 'app.zx'
        argv = [ziran, 'bundle', '--project', '--entry', 'app:main', '-o', built, app]
        command(*argv, '--offline', failure='absent from the offline cache', cwd=consumer)
        command(*argv, cwd=consumer)
        assert command(ziran, 'run', '--backend', 'engine', built) == '42'
        request_count = len(server.requests)
        command(*argv, '--offline', cwd=consumer)
        assert len(server.requests) == request_count

        # Ordinary add discovers the published descriptor and resolves its
        # package graph from the archive before attempting any Git checkout.
        automatic = root / 'automatic'
        automatic.mkdir()
        bare_manifest = '[package]\nname = "consumer"\nmodule_roots = ["."]\n' + tool_section
        automatic_manifest = automatic / 'ziran.toml'
        automatic_manifest.write_text(bare_manifest)
        (automatic / 'ziran.local.toml').write_text(f'[overrides]\nziran = "{toolchain}"\n')
        auto_cache = root / 'automatic-cache'
        env.update(XDG_CACHE_HOME=str(auto_cache), ZIRAN_ZX_REGISTRY=f'https://localhost:{server.server_port}')
        checked_artifact = f'/api/packages/ziranlang/generic/fixture-zx/{checked_revision}/default.zir.zip'
        descriptor = checked_artifact.removesuffix('.zip') + '.tsv'
        good_metadata = server.images[descriptor]
        expected_tool = 'p' + hashlib.sha256(f'{tool_url}\n{tool_revision}'.encode()).hexdigest()[:16]
        assert good_metadata == f'1\t{identity}\t{expected_tool}\tdefault\t{archive_digest}\n'.encode()
        add = [ziran, 'add', repository, '--name', 'state', '--ref', checked_revision]
        server.images[descriptor] = good_metadata.replace(expected_tool.encode(), b'p0000000000000000')
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
        auto_app.write_bytes(app.read_bytes())
        auto_output = automatic / 'app.zx'
        auto_bundle = [ziran, 'bundle', '--project', '--offline', '--entry', 'app:main', '-o', auto_output, auto_app]
        before = len(server.requests)
        saved = automatic_lock.read_bytes()
        command(ziran, 'lock', '--offline', cwd=automatic)
        assert automatic_lock.read_bytes() == saved
        command(*auto_bundle, cwd=automatic)
        assert command(ziran, 'run', '--backend', 'engine', auto_output) == '42'
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

        override = root / 'override'
        override.mkdir()
        (override / 'src').mkdir()
        (override / 'ziran.toml').write_text('[package]\nname = "state"\nmodule_roots = ["src"]\n' +
                                             tool_section + '[exports]\nmodel = "src/state.zi"\n')
        (override / 'src/state.zi').write_text('''State :: struct { value: s32; }
Advance :: (state: *State) { state.value += 4; }
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

        wrong_tool = io.BytesIO()
        with (zipfile.ZipFile(io.BytesIO(server.images[checked_artifact])) as original,
              zipfile.ZipFile(wrong_tool, 'w') as invalid):
            for name in original.namelist():
                data = original.read(name)
                if name == 'checked-package.tsv':
                    _, package_id, _, profile = data.decode().strip().split('\t')
                    data = f'1\t{package_id}\tp0000000000000000\t{profile}\n'.encode()
                invalid.writestr(name, data)
        server.images[checked_artifact] = wrong_tool.getvalue()
        pins.write_text(json.dumps({'format': 1, 'packages': [{'package': identity, 'profile': 'default',
            'sha256': hashlib.sha256(wrong_tool.getvalue()).hexdigest()}]}))
        command(*argv, failure='archive does not match its pinned package, toolchain or profile', cwd=consumer)
    finally:
        server.shutdown()
        server.server_close()
        thread.join()

print('Portable package publish, download, typed source-free linking and cache checks passed')
PY
