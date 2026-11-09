"""Run the ZX lifecycle WebAssembly fixtures in a private headless Chromium."""
import argparse
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import select
import shutil
import subprocess
import tempfile
import time
from threading import Thread


class DevToolsError(RuntimeError):
    def __init__(self, method, error):
        self.method, self.code = method, error.get('code')
        super().__init__(method + ': ' + str(error))


class Browser:
    def __init__(self, root, profile, chromium):
        shutil.rmtree(profile, ignore_errors=True)
        (profile / 'home').mkdir(parents=True)
        runtime = profile / 'runtime'
        runtime.mkdir(mode=0o700)
        scratch = root / 'build/t'
        scratch.mkdir(parents=True, exist_ok=True)
        removed = ('DISPLAY', 'WAYLAND_DISPLAY', 'WAYLAND_SOCKET', 'XAUTHORITY',
                   'GDK_DISPLAY', 'DBUS_SESSION_BUS_ADDRESS', 'SESSION_MANAGER',
                   'XDG_SESSION_ID', 'XDG_SESSION_TYPE')
        environment = {key: value for key, value in os.environ.items()
                       if key not in removed}
        environment.update({
            'HOME': str(profile / 'home'),
            'TMPDIR': str(scratch),
            'XDG_CONFIG_HOME': str(profile / 'home/config'),
            'XDG_CACHE_HOME': str(profile / 'home/cache'),
            'XDG_RUNTIME_DIR': str(runtime),
            'YUE_DESKTOP_RECOVERY': '0',
        })
        self.log = (profile / 'chromium.log').open('wb')
        self.process = subprocess.Popen(
            ['sh', '-c', 'exec 3<&0 4>&1 "$@" </dev/null >/dev/null',
             'chromium', chromium, '--headless', '--no-sandbox',
             '--disable-gpu', '--use-gl=disabled', '--disable-dev-shm-usage',
             '--disable-background-networking', '--disable-breakpad',
             '--disable-crash-reporter', '--no-first-run',
             '--no-default-browser-check', '--hide-scrollbars', '--mute-audio',
             '--remote-debugging-pipe', '--user-data-dir=' + str(profile),
             'about:blank'], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=self.log, env=environment, start_new_session=True)
        self.received, self.sequence, self.session = b'', 0, None
        self.errors, self.console = [], []
        target = next(item for item in self.command('Target.getTargets')['targetInfos']
                      if item['type'] == 'page')
        self.target_id = target['targetId']
        self.session = self.command('Target.attachToTarget',
                                    targetId=target['targetId'], flatten=True)['sessionId']
        for domain in ('Runtime', 'Page', 'Log'):
            self.command(domain + '.enable')

    def close(self):
        try:
            self.process.stdin.close()
        except OSError:
            pass
        try:
            os.killpg(self.process.pid, 15)
        except ProcessLookupError:
            pass
        try:
            self.process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            os.killpg(self.process.pid, 9)
            self.process.wait()
        self.log.close()

    def command(self, method, timeout=45, **params):
        self.sequence += 1
        message = {'id': self.sequence, 'method': method, 'params': params}
        if self.session:
            message['sessionId'] = self.session
        self.process.stdin.write(json.dumps(message).encode() + b'\0')
        self.process.stdin.flush()
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            while b'\0' in self.received:
                raw, self.received = self.received.split(b'\0', 1)
                if not raw:
                    continue
                reply = json.loads(raw)
                event = reply.get('method')
                if event == 'Runtime.exceptionThrown':
                    details = reply['params']['exceptionDetails']
                    self.errors.append((details.get('exception') or {}).get('description')
                                       or details.get('text'))
                if event == 'Runtime.consoleAPICalled':
                    values = reply['params'].get('args', [])
                    self.console.append(' '.join(str(value.get('value', ''))
                                                  for value in values))
                if reply.get('id') == self.sequence:
                    if 'error' in reply:
                        raise DevToolsError(method, reply['error'])
                    return reply.get('result', {})
            if select.select([self.process.stdout], [], [], .2)[0]:
                data = os.read(self.process.stdout.fileno(), 1 << 20)
                if not data:
                    raise RuntimeError('Chromium exited')
                self.received += data
        targets = []
        try:
            targets = [self.command('Target.getTargetInfo', timeout=2,
                                    targetId=self.target_id)['targetInfo']]
        except (TimeoutError, KeyError):
            pass
        titles = [target.get('title', '') for target in targets
                  if target.get('type') == 'page']
        raise TimeoutError(method + ' page titles=' + repr(titles) +
                           ' console=' + repr(self.console[-20:]))

    def evaluate(self, expression):
        result = self.command('Runtime.evaluate', expression=expression,
                              returnByValue=True, awaitPromise=True)
        if 'exceptionDetails' in result:
            raise RuntimeError(str(result['exceptionDetails'])[:1600])
        return result.get('result', {}).get('value')

    def until(self, expression, description, seconds=10):
        deadline = time.monotonic() + seconds
        value = None
        while time.monotonic() < deadline:
            try:
                value = self.evaluate(expression)
            except DevToolsError as error:
                if error.method != 'Runtime.evaluate' or error.code != -32000:
                    raise
            if value is not None:
                return value
            time.sleep(.1)
        raise AssertionError(description + ': timed out at ' + repr(value))


def serve(directory):
    class Handler(SimpleHTTPRequestHandler):
        def __init__(self, *args, **kwargs):
            super().__init__(*args, directory=str(directory), **kwargs)

        def log_message(self, format, *args):
            pass

    server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
    Thread(target=server.serve_forever, daemon=True).start()
    return server, 'http://127.0.0.1:%d/' % server.server_address[1]


def write_page(path, export, mode, fixture):
    arguments = json.dumps(
        [mode, *[fixture[name] for name in
                 ('old', 'new', 'wrong', 'init', 'unrelated')]])
    path.write_text('''<!doctype html><meta charset="utf-8"><script src="%s">
</script><script>
(function() {
  globalThis.lifecycleDone = undefined;
  globalThis.lifecycleLines = [];
  const capture = value => globalThis.lifecycleLines.push(String(value));
  Promise.resolve(%s({
    arguments: %s,
    noExitRuntime: true,
    print: capture,
    printErr: value => capture('stderr: ' + value),
  })).catch(error => {
    capture('load error: ' + error);
    if(error && error.stack) capture(error.stack);
    globalThis.lifecycleDone = 1;
  });
})();
</script>''' % (path.name.replace('.html', '.js'), export, arguments))


def check_page(browser, url, required):
    browser.command('Page.navigate', url=url)
    status = browser.until('globalThis.lifecycleDone', 'load ' + url)
    lines = browser.evaluate('globalThis.lifecycleLines.slice()')
    missing = [name for name in required
               if not any(line == 'PASS ' + name for line in lines)]
    failures = [line for line in lines if line.startswith('FAIL') or
                line.startswith('stderr:') or line.startswith('load error:')]
    if status != 0 or missing or failures or browser.errors:
        raise AssertionError({'url': url, 'status': status, 'missing': missing,
                              'failures': failures, 'exceptions': browser.errors,
                              'lines': lines})
    return lines


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--work', type=Path, required=True)
    parser.add_argument('--chromium', default=os.environ.get('CHROMIUM', 'chromium'))
    args = parser.parse_args()
    root, work = args.root.resolve(), args.work.resolve()
    fixtures = {name: '/fixtures/' + name + '.zx' for name in
                ('old', 'new', 'wrong', 'init', 'unrelated')}
    write_page(work / 'wasm.html', 'ZxWebLifecycleWasm', 'wasm', fixtures)
    write_page(work / 'linked.html', 'ZxWebLifecycleLinked', 'linked', fixtures)
    server, base = serve(work)
    profile_root = root / 'build/t/profiles'
    profile_root.mkdir(parents=True, exist_ok=True)
    browser = Browser(root, Path(tempfile.mkdtemp(prefix='p', dir=profile_root)),
                      args.chromium)
    lifecycle = ('identity-entry-abi', 'instance-state-migration',
                 'component-independent-state',
                 'typed-abi-replacement-rejected', 'replacement-state-continuity',
                 'stale-reference-rejected', 'migration-failure-rollback',
                 'verification-failure-rollback',
                 'initialization-failure-rollback', 'unrelated-component-live')
    try:
        wasm = check_page(browser, base + 'wasm.html',
                          ('wasm-provider-independent', 'wasm-engine-active') + lifecycle)
        linked = check_page(browser, base + 'linked.html',
                            ('linked-independent',) + lifecycle)
    finally:
        browser.close()
        server.shutdown()
        server.server_close()
    assert any(line == 'PASS browser-lifecycle wasm' for line in wasm), wasm
    assert any(line == 'PASS browser-lifecycle linked' for line in linked), linked
    print('Private Chromium ZX lifecycle passed: wasm and linked hosts share code, '
          'isolate state, migrate atomically, reject stale/ABI mismatches, and roll back')


if __name__ == '__main__':
    main()
