#!/usr/bin/env python3
"""Correctness-checked D-02 scalar baselines; invoke through build-queue.

No speedup threshold: shared-host samples are evidence, not a language ranking.
Artifacts, exact commands, diagnostics, hashes and raw samples stay in --out.
An existing output directory is refused, preserving earlier measurements.
"""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import platform
import random
import shutil
import statistics
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
FIXTURES = ROOT / 'tests/benchmarks'
KERNELS = ['BytePlane', 'Filter', 'Color']


def oracle(kind, rounds, seed):
    """Independent Python integer/array implementation, outside timed regions."""
    checksum = 0
    if kind == 'BytePlane':
        plane = [(i * 13 + seed) % 256 for i in range(256)]
        for step in range(rounds):
            for i in range(64):
                reversed_bytes = [plane[i + j * 64] for j in reversed(range(4))]
                swapped = int.from_bytes(bytes(reversed_bytes), 'little')
                plane[i] = (((swapped ^ (swapped >> 24)) & 255) + step + seed) % 256
                checksum += plane[i] * (i + 1) + swapped
    elif kind == 'Filter':
        samples = [(i * 17 + seed) % 256 for i in range(256)]
        for step in range(rounds):
            filtered = [(samples[max(0, i - 1)] + 2 * samples[i] +
                         samples[min(255, i + 1)] + 2) // 4 for i in range(256)]
            checksum += sum(value * (i + 1) for i, value in enumerate(filtered))
            samples = [(value + step % 7) % 256 for value in filtered]
    else:
        assert kind == 'Color'
        red = [(i * 3 + seed) % 256 for i in range(256)]
        green = [(i * 5 + seed * 2) % 256 for i in range(256)]
        blue = [(i * 7 + seed * 3) % 256 for i in range(256)]
        for step in range(rounds):
            for i in range(256):
                y = (77 * red[i] + 150 * green[i] + 29 * blue[i] + 128) // 256
                checksum += y * (i + 1)
                red[i] = (y + seed + step) % 256
    return checksum


def digest(path):
    result = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for chunk in iter(lambda: stream.read(65536), b''):
            result.update(chunk)
    return result.hexdigest()


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n')


C_HOST = r'''
#define _POSIX_C_SOURCE 200809L
#include "codec_kernels.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
static long long ns(clockid_t id) {
    struct timespec t;
    if(clock_gettime(id, &t)) exit(3);
    return (long long)t.tv_sec * 1000000000LL + t.tv_nsec;
}
int main(int argc, char **argv) {
    if(argc != 5) return 2;
    long long rounds = strtoll(argv[2], 0, 10), seed = strtoll(argv[3], 0, 10);
    int64_t (*kernels[])(int64_t, int64_t) = {BytePlane, Filter, Color};
    int which = atoi(argv[1]), warm = atoi(argv[4]);
    if(which < 0 || which > 2 || rounds < 0 || rounds > 100000 || seed < 0 || seed > 255) return 2;
    long long warm_result = warm ? kernels[which](rounds, seed) : -1;
    long long wall = ns(CLOCK_MONOTONIC), cpu = ns(CLOCK_PROCESS_CPUTIME_ID);
    long long result = kernels[which](rounds, seed);
    cpu = ns(CLOCK_PROCESS_CPUTIME_ID) - cpu;
    wall = ns(CLOCK_MONOTONIC) - wall;
    printf("{\"result\":%lld,\"warm_result\":%lld,\"kernel_wall_ns\":%lld,\"kernel_cpu_ns\":%lld}\n",
           result, warm_result, wall, cpu);
    return 0;
}
'''

GO_HOST = r'''
package main
import("fmt"; "os"; "strconv"; "time")
func main() {
    if len(os.Args) != 5 { panic("arguments") }
    which, e := strconv.Atoi(os.Args[1]); if e != nil { panic(e) }
    rounds, e := strconv.ParseInt(os.Args[2], 10, 64); if e != nil { panic(e) }
    seed, e := strconv.ParseInt(os.Args[3], 10, 64); if e != nil { panic(e) }
    if which < 0 || which > 2 || rounds < 0 || rounds > 100000 || seed < 0 || seed > 255 { panic("range") }
    kernels := []func(int64,int64) int64 {CodecKernels_BytePlane, CodecKernels_Filter, CodecKernels_Color}
    warm := int64(-1)
    if os.Args[4] == "1" { warm = kernels[which](rounds, seed) }
    started := time.Now(); result := kernels[which](rounds, seed); elapsed := time.Since(started).Nanoseconds()
    fmt.Printf("{\"result\":%d,\"warm_result\":%d,\"kernel_wall_ns\":%d}\n", result, warm, elapsed)
}
'''

C_LAYOUT = r'''
#include "codec_kernels_layout.h"
#include <stdio.h>
int main(void) {
    uint8_t byte = 19;
    Probe value = {7, &byte, 16};
    if(RoundTrip() != 42 || ReadProbe(&value) != 42) return 1;
    Slice slice = {&byte, 1};
    if(ReadSlice(slice) != 1 || SliceSize() != sizeof(Slice)) return 3;
    if(PointerSize() != sizeof(void *) || WordSize() != sizeof(size_t) ||
       RecordSize() != sizeof(Probe)) return 2;
    printf("{\"pointer\":%zu,\"word\":%zu,\"slice\":%lld,\"record\":%zu,"
           "\"record_alignment\":%zu,\"pointer_offset\":%zu,\"tail_offset\":%zu,\"round_trip\":42}\n",
           sizeof(void *), sizeof(size_t), (long long)SliceSize(), sizeof(Probe),
           _Alignof(Probe), offsetof(Probe, pointer), offsetof(Probe, tail));
    return 0;
}
'''

GO_LAYOUT = r'''
package main
import("fmt"; "unsafe")
func main() {
    octet := uint8(19); value := Probe{7, &octet, 16}
    if CodecKernelsLayout_RoundTrip() != 42 || CodecKernelsLayout_ReadProbe(&value) != 42 ||
       CodecKernelsLayout_ReadSlice([]uint8{19}) != 1 { panic("field access") }
    if CodecKernelsLayout_PointerSize() != int64(unsafe.Sizeof(&octet)) ||
       CodecKernelsLayout_WordSize() != int64(unsafe.Sizeof(uint(0))) ||
       CodecKernelsLayout_SliceSize() != int64(unsafe.Sizeof([]byte{})) ||
       CodecKernelsLayout_RecordSize() != int64(unsafe.Sizeof(value)) { panic("native size_of") }
    fmt.Printf("{\"pointer\":%d,\"word\":%d,\"slice\":%d,\"record\":%d,\"record_alignment\":%d,\"pointer_offset\":%d,\"tail_offset\":%d,\"round_trip\":42}\n",
        CodecKernelsLayout_PointerSize(), CodecKernelsLayout_WordSize(), CodecKernelsLayout_SliceSize(),
        CodecKernelsLayout_RecordSize(), unsafe.Alignof(value), unsafe.Offsetof(value.Pointer), unsafe.Offsetof(value.Tail))
}
'''

LOGICAL_LAYOUT = r'''
#include "zir.h"
#include "zir_check.h"
#include "zir_load.h"
#include <stdio.h>
int main(int argc, char **argv) {
    if(argc != 3) return 2;
    ProgramSet set = {0}; const char *inputs[] = {argv[2]};
    if(!ProgramsLoad(&set, argv[1], 0, 0, inputs, 1) || !CheckPrograms(set.programs, set.count)) return 3;
    const char *names[] = {"*u8", "usize", "[]u8", "Probe"};
    printf("[");
    for(int i = 0; i < 4; i++) {
        size_t size, alignment;
        if(!TypeLayout(&set.programs[0]->modules[0], names[i], &size, &alignment)) return 4;
        printf("%s{\"type\":\"%s\",\"size\":%zu,\"alignment\":%zu}", i ? "," : "", names[i], size, alignment);
    }
    puts("]"); ProgramsFree(&set); return 0;
}
'''


def run_wasm(path):
    """Optional real Wasmtime execution; no Node, downloads, or mock provider."""
    if importlib.util.find_spec('wasmtime') is None:
        return run_browser_wasm(Path(path))
    import wasmtime as w
    engine = w.Engine()
    store = w.Store(engine)
    module = w.Module.from_file(engine, path)
    budget = w.Global(store, w.GlobalType(w.ValType.i64(), True), 1000000000)

    def overrun():
        raise RuntimeError('Wasm instruction budget exhausted')

    imports = []
    for item in module.imports:
        if (item.module, item.name) == ('zx', 'budget'):
            imports.append(budget)
        elif (item.module, item.name) == ('zx', 'overrun'):
            imports.append(w.Func(store, w.FuncType([], []), overrun))
        else:
            raise RuntimeError(f'unexpected Wasm import: {item.module}:{item.name}')
    instance = w.Instance(store, module, imports)
    result = instance.exports(store)['zx_entry'](store)
    print(result)


def run_browser_wasm(path):
    """Private CPU-only browser; no owner profile, desktop, input or capture."""
    import functools
    from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
    import re
    import signal
    import tempfile
    from threading import Thread

    browser = shutil.which('firefox') or shutil.which('chromium')
    if not browser:
        raise RuntimeError('no Python Wasmtime binding, Firefox or Chromium executable')
    path = path.resolve()
    profiles = path.parent / 'browser-profiles'
    profiles.mkdir(exist_ok=True)
    profile = Path(tempfile.mkdtemp(prefix='wasm-', dir=profiles))
    page = profile / 'runner.html'
    page.write_text('''<!doctype html><meta charset="utf-8"><pre id="result">PENDING</pre>
<script>
(async () => {
    const response = await fetch('/''' + path.name + '''');
    if (!response.ok) throw new Error('HTTP ' + response.status);
    const {instance} = await WebAssembly.instantiate(await response.arrayBuffer(), {
        zx: {budget: new WebAssembly.Global({value: 'i64', mutable: true}, 1000000000n),
             overrun: () => { throw new Error('Wasm budget exhausted'); }}
    });
    document.getElementById('result').textContent = instance.exports.zx_entry().toString();
})().catch(error => {document.getElementById('result').textContent = 'ERROR: ' + error.message;});
</script>''')

    class Handler(SimpleHTTPRequestHandler):
        def log_message(self, *_args):
            pass

    server = ThreadingHTTPServer(('127.0.0.1', 0), functools.partial(Handler, directory=str(path.parent)))
    server.daemon_threads = True
    thread = Thread(target=server.serve_forever, daemon=True)
    thread.start()
    relative_page = page.relative_to(path.parent).as_posix()
    url = f'http://127.0.0.1:{server.server_port}/{relative_page}'
    command = [browser, '--headless', '--disable-gpu', '--use-gl=angle', '--use-angle=swiftshader',
               '--disable-vulkan', '--disable-features=Vulkan', '--in-process-gpu',
               '--disable-gpu-compositing', '--disable-dev-shm-usage', '--disable-background-networking',
               '--disable-extensions', '--disable-sync', '--no-first-run', '--no-default-browser-check',
               '--no-proxy-server', '--metrics-recording-only', '--user-data-dir=' + str(profile),
               '--virtual-time-budget=10000', '--dump-dom',
               url]
    # Chromium's Unix SingletonSocket path must fit sockaddr_un (108 bytes).
    # Keep its temporary socket parent short and inside the owning checkout.
    browser_tmp = ROOT / 'build/scratch/cb'
    browser_tmp.mkdir(mode=0o700, exist_ok=True)
    browser_env = dict(os.environ, TMPDIR=str(browser_tmp), XDG_RUNTIME_DIR=str(browser_tmp))
    write_json(profile / 'command.json', dict(argv=command, environment_overrides={
        'TMPDIR': str(browser_tmp), 'XDG_RUNTIME_DIR': str(browser_tmp)}))
    try:
        if Path(browser).name == 'firefox':
            return run_firefox_page(browser, profile, url, browser_env)
        with (profile / 'dom.html').open('wb') as stdout, (profile / 'stderr.log').open('wb') as stderr:
            child = subprocess.Popen(command, stdout=stdout, stderr=stderr, env=browser_env, start_new_session=True)
            try:
                code = child.wait(timeout=35)
            except subprocess.TimeoutExpired:
                os.killpg(child.pid, signal.SIGTERM)
                try:
                    child.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    os.killpg(child.pid, signal.SIGKILL)
                    child.wait()
                raise RuntimeError(f'private Chromium timed out; diagnostics: {profile}')
        with (profile / 'dom.html').open() as stream:
            dom = stream.read(16384)
        match = re.search(r'<pre id="result">(-?[0-9]+)</pre>', dom)
        if code or not match:
            raise RuntimeError(f'private Chromium failed (exit {code}); inspect {profile}/dom.html and stderr.log')
        print(match.group(1))
    finally:
        server.shutdown()
        server.server_close()
        thread.join()


def run_firefox_page(browser, profile, url, environment):
    """Small bounded Marionette client; uses only this test's browser/process."""
    import signal
    import socket

    def receive(connection):
        prefix = b''
        while not prefix.endswith(b':'):
            byte = connection.recv(1)
            if not byte or len(prefix) > 12:
                raise RuntimeError('invalid/closed Firefox protocol frame')
            prefix += byte
        length = int(prefix[:-1])
        if length > 65536:
            raise RuntimeError('oversized Firefox protocol response')
        data = bytearray()
        while len(data) < length:
            chunk = connection.recv(length - len(data))
            if not chunk:
                raise RuntimeError('Firefox protocol closed')
            data.extend(chunk)
        return json.loads(data)

    with socket.socket() as reserve:
        reserve.bind(('127.0.0.1', 0))
        port = reserve.getsockname()[1]
    # A fresh profile otherwise opens first-run, privacy-notice and update
    # pages from the network, and Marionette's NewSession waits for them.
    prefs = {'marionette.port': port, 'gfx.webrender.software': True,
             'browser.shell.checkDefaultBrowser': False, 'browser.startup.page': 0,
             'browser.startup.homepage_override.mstone': 'ignore', 'startup.homepage_welcome_url': 'about:blank',
             'startup.homepage_welcome_url.additional': '', 'browser.aboutwelcome.enabled': False,
             'datareporting.policy.dataSubmissionEnabled': False,
             'datareporting.policy.dataSubmissionPolicyBypassNotification': True,
             'toolkit.telemetry.reportingpolicy.firstRun': False, 'app.update.disabledForTesting': True,
             'browser.safebrowsing.malware.enabled': False, 'browser.safebrowsing.phishing.enabled': False,
             'browser.safebrowsing.downloads.enabled': False, 'network.captive-portal-service.enabled': False,
             'network.connectivity-service.enabled': False, 'extensions.update.enabled': False,
             'browser.newtabpage.enabled': False, 'toolkit.startup.max_resumed_crashes': -1}
    (profile / 'user.js').write_text(''.join(f'user_pref({json.dumps(name)}, {json.dumps(value)});\n'
                                             for name, value in prefs.items()))
    environment = environment | {'MOZ_HEADLESS': '1', 'LIBGL_ALWAYS_SOFTWARE': '1', 'YUE_DESKTOP_RECOVERY': '0'}
    command = [browser, '--headless', '--marionette', '--no-remote', '--profile', str(profile), 'about:blank']
    write_json(profile / 'command.json', dict(argv=command, environment_overrides={key: environment[key]
               for key in ['TMPDIR', 'XDG_RUNTIME_DIR', 'MOZ_HEADLESS', 'LIBGL_ALWAYS_SOFTWARE', 'YUE_DESKTOP_RECOVERY']}))
    with (profile / 'stderr.log').open('wb') as log:
        child = subprocess.Popen(command, env=environment, stdout=log, stderr=log, start_new_session=True)
        def stderr_tail():
            log.flush()
            with (profile / 'stderr.log').open('rb') as stream:
                stream.seek(0, os.SEEK_END)
                stream.seek(max(0, stream.tell() - 2048))
                return stream.read().decode(errors='replace').strip() or '(empty)'

        try:
            started = time.monotonic()
            deadline = started + 60
            connection = None
            while time.monotonic() < deadline and child.poll() is None:
                try:
                    connection = socket.create_connection(('127.0.0.1', port), timeout=1)
                    break
                except OSError:
                    time.sleep(.1)
            if connection is None:
                raise RuntimeError(f'private Firefox startup failed (exit {child.poll()}); stderr: {stderr_tail()}')
            with connection:
                connection.settimeout(10)
                receive(connection)
                sequence = 0

                def call(method, parameters):
                    nonlocal sequence
                    sequence += 1
                    payload = json.dumps([0, sequence, method, parameters]).encode()
                    connection.sendall(str(len(payload)).encode() + b':' + payload)
                    # A cold browser on a loaded machine can take longer than
                    # one fixed reply window; the whole run keeps its deadline.
                    connection.settimeout(max(1.0, deadline - time.monotonic()))
                    try:
                        reply = receive(connection)
                    except TimeoutError:
                        raise RuntimeError(f'Firefox {method} timed out {time.monotonic() - started:.1f}s after launch; '
                                           f'stderr: {stderr_tail()}') from None
                    if reply[1] != sequence or reply[2] is not None:
                        raise RuntimeError(f'Firefox {method}: {reply}')
                    return reply[3]

                call('WebDriver:NewSession', {'capabilities': {}})
                call('WebDriver:Navigate', {'url': url})
                value = None
                while time.monotonic() < deadline:
                    value = call('WebDriver:ExecuteScript', {'script': 'return document.getElementById("result")?.textContent',
                                 'args': [], 'newSandbox': True, 'sandbox': 'default', 'line': 1, 'filename': 'codec-kernels'})['value']
                    if value not in [None, 'PENDING']:
                        break
                    time.sleep(.05)
                (profile / 'result.txt').write_text(str(value) + '\n')
                call('WebDriver:DeleteSession', {})
                if value is None or not value.lstrip('-').isdigit():
                    raise RuntimeError(f'Firefox Wasm execution failed: {value}; see {profile}')
                print(value)
        finally:
            if child.poll() is None:
                os.killpg(child.pid, signal.SIGTERM)
                try:
                    child.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    os.killpg(child.pid, signal.SIGKILL)
                    child.wait()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, default=ROOT / 'build/scratch/codec-kernels')
    parser.add_argument('--bin-dir', type=Path, default=ROOT / 'build/bin')
    parser.add_argument('--rounds', type=int, default=512, help='native passes of 256 input samples')
    parser.add_argument('--repetitions', type=int, default=5)
    args = parser.parse_args()
    if not (8 <= args.rounds <= 100000 and 3 <= args.repetitions <= 31):
        parser.error('require 8..100000 rounds and 3..31 repetitions')
    out = args.out.resolve()
    if not out.is_relative_to(ROOT / 'build/scratch'):
        parser.error('--out must stay under this checkout build/scratch')
    if out.exists():
        parser.error('output exists; preserve needed evidence and choose an unused --out under build/scratch')
    cc = shutil.which(os.environ.get('CC', 'cc'))
    if not cc or not hasattr(os, 'wait4'):
        parser.error('a C compiler and POSIX wait4 are required')
    bins = args.bin_dir.resolve()
    required = ['ziran', 'zi2zir', 'zi2c', 'zi2go', 'zi2zx']
    for name in required:
        if not (bins / name).is_file():
            parser.error(f'missing compiler tool: {bins / name}; build the toolchain first')
    out.mkdir(parents=True)
    (out / 'commands').mkdir()
    env = dict(os.environ)
    for key in list(env):
        if key in ('DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'DBUS_SESSION_BUS_ADDRESS',
                   'SESSION_MANAGER', 'XDG_RUNTIME_DIR', 'DESKTOP_SESSION', 'DBUS_SYSTEM_BUS_ADDRESS',
                   'ZIRAN_PROFILE', 'GOBIN', 'GOFLAGS', 'GOWORK') or key.startswith('XDG_SESSION_'):
            env.pop(key, None)
    env.update(YUE_DESKTOP_RECOVERY='0', GO111MODULE='off', CGO_ENABLED='0',
               GOCACHE=str(out / 'go-cache'), GOTMPDIR=str(out / 'go-tmp'),
               TMPDIR=str(out / 'tmp'), GOMAXPROCS='2', GOTOOLCHAIN='local')
    for key in ['GOTMPDIR', 'TMPDIR']:
        Path(env[key]).mkdir()
    samples = []
    capabilities = {}
    counter = 0

    def invoke(command, phase, case, *, allow_failure=False, extra_env=None, cwd=ROOT):
        nonlocal counter
        counter += 1
        argv = [str(value) for value in command]
        prefix = out / 'commands' / f'{counter:04d}'
        started = time.perf_counter_ns()
        try:
            with prefix.with_suffix('.stdout').open('wb') as stdout, prefix.with_suffix('.stderr').open('wb') as stderr:
                child = subprocess.Popen(argv, cwd=cwd, env=env | (extra_env or {}), stdout=stdout, stderr=stderr)
                _, status, usage = os.wait4(child.pid, 0)
                child.returncode = os.waitstatus_to_exitcode(status)
            code = child.returncode
            wall_ns = time.perf_counter_ns() - started
        except OSError as error:
            prefix.with_suffix('.stderr').write_text(str(error) + '\n')
            code, usage, wall_ns = 127, None, time.perf_counter_ns() - started
        with prefix.with_suffix('.stdout').open('rb') as stream:
            result = stream.read(16385)
        with prefix.with_suffix('.stderr').open('rb') as stream:
            stream.seek(0, 2)
            stream.seek(max(0, stream.tell() - 4000))
            diagnostic = stream.read().decode(errors='replace')
        row = dict(phase=phase, case=case, argv=argv, cwd=str(cwd),
                   environment_overrides=extra_env or {}, returncode=code,
                   process_wall_ns=wall_ns, process_user_s=usage.ru_utime if usage else None,
                   process_system_s=usage.ru_stime if usage else None,
                   stdout_file=str(prefix.with_suffix('.stdout').relative_to(out)),
                   stderr_file=str(prefix.with_suffix('.stderr').relative_to(out)))
        with (out / 'commands.jsonl').open('a') as stream:
            stream.write(json.dumps(row) + '\n')
        if code and not allow_failure:
            raise RuntimeError(f'{case}: exit {code}; {diagnostic}; see {prefix}.stderr')
        if len(result) > 16384:
            raise RuntimeError(f'{case}: unexpectedly large command stdout; inspect {prefix}.stdout')
        return result.decode(errors='replace').strip(), row, diagnostic

    def git(*argv):
        return invoke(['git', *argv], 'metadata', 'git ' + ' '.join(argv))[0]

    metadata = dict(schema_version=1, created_utc=time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()),
                    repository='ziranlang/ziran', revision=git('rev-parse', 'HEAD'),
                    dirty_status=git('status', '--short'), platform=platform.platform(),
                    machine=platform.machine(), cpu_count=os.cpu_count(),
                    cpu_affinity=sorted(os.sched_getaffinity(0)) if hasattr(os, 'sched_getaffinity') else None,
                    repetitions=args.repetitions, native_rounds=args.rounds, portable_rounds=8,
                    seed=37, environment={k: env[k] for k in ['GOMAXPROCS', 'GOTOOLCHAIN', 'GO111MODULE', 'CGO_ENABLED', 'YUE_DESKTOP_RECOVERY']},
                    definitions={
                        'correctness': 'Python oracle, all native vectors, then source/saved IR small bundle outputs; all before timing',
                        'warm_native': 'fresh process, one full untimed call on identical input, then one timed call; both results checked',
                        'cold_process': 'first call in a fresh process, no cache flush; process wall/CPU include startup and kernel initialization',
                        'portable_process': 'fresh portable VM process for each sample; includes bundle load/verification and interpreter startup',
                        'kernel_region': 'initialization, scalar algorithm, checksum and call overhead; excludes code generation, build and process startup',
                        'compile': 'single separately recorded build phase, no claim of cold or repeated compile performance'},
                    limitations=[
                        'Small synthetic scalar building blocks; no decoder, desktop, GPU, throughput or speedup claim.',
                        'No CPU pinning, frequency lock, cache eviction or isolation from concurrent host work.',
                        'Native passes and portable passes differ; only compare matching round counts and timing regions.',
                        'Go inner CPU time is unavailable; process user/system CPU comes from POSIX wait4.',
                        'No allocation/energy measurement. Very short CPU samples may be below clock granularity.',
                        'Logical TypeLayout, C ABI and Go ABI are different contracts; a size difference alone is not a proven defect.'])
    cpu = {}
    if Path('/proc/cpuinfo').exists():
        with Path('/proc/cpuinfo').open() as stream:
            for line in stream:
                if not line.strip():
                    break
                key, _, value = line.partition(':')
                if key.strip() in ['vendor_id', 'model name', 'cpu family', 'model', 'stepping', 'flags', 'Features']:
                    cpu[key.strip()] = value.strip()
    metadata['cpu'] = cpu
    metadata['toolchain'] = {'cc': invoke([cc, '--version'], 'metadata', 'C compiler version')[0],
                             'cc_target': invoke([cc, '-dumpmachine'], 'metadata', 'C compiler target')[0],
                             'python': sys.version}
    tool_files = [bins / name for name in required] + [bins / 'zi2wasm', bins.parent / 'libziran.a']
    metadata['tools'] = {str(path): dict(sha256=digest(path), size=path.stat().st_size, mtime_ns=path.stat().st_mtime_ns)
                         for path in tool_files if path.is_file()}
    inputs = [Path(__file__), ROOT / 'tests/compiler_kernel_benchmarks.sh',
              FIXTURES / 'codec_kernels.zi', FIXTURES / 'codec_kernels_layout.zi']
    metadata['input_hashes'] = {str(path.relative_to(ROOT)): digest(path) for path in inputs}
    # Capture compiler/library source identity without loading source contents.
    metadata['compiler_source_hashes'] = {str(path.relative_to(ROOT)): digest(path)
                                          for folder in ['cmd', 'include', 'bootstrap/compiler_ir']
                                          for path in (ROOT / folder).rglob('*')
                                          if path.is_file() and path.suffix in ['.c', '.h']}
    write_json(out / 'metadata.json', metadata)
    write_json(out / 'capabilities.json', capabilities)
    ziran = bins / 'ziran'
    source = FIXTURES / 'codec_kernels.zi'
    layout_source = FIXTURES / 'codec_kernels_layout.zi'
    invoke([ziran, 'check', '--root', FIXTURES, source, layout_source], 'check', 'type-check fixtures')
    invoke([ziran, 'ir', '--root', FIXTURES, '-o', out / 'ir', source, layout_source], 'codegen', 'save checked IR')
    forms = [('source', FIXTURES, source, layout_source),
             ('saved', out / 'ir', out / 'ir/codec_kernels.zir', out / 'ir/codec_kernels_layout.zir')]
    native = []
    layout = {}
    parity = []
    go = shutil.which('go')
    if go:
        metadata['toolchain']['go'] = invoke([go, 'version'], 'metadata', 'Go version')[0]
        metadata['toolchain']['go_target'] = invoke([go, 'env', 'GOOS', 'GOARCH', 'GOAMD64', 'GOARM64'], 'metadata', 'Go target')[0]
    capabilities['go'] = {'execution': 'pending' if go else 'unavailable', 'reason': '' if go else 'go executable not in PATH'}
    cflags = ['-std=c11', '-O2', '-fno-lto']
    metadata['cflags'] = cflags
    metadata['goflags'] = ['build', '-p=2', '-buildvcs=false']
    (out / 'host.c').write_text(C_HOST)
    (out / 'layout-host.c').write_text(C_LAYOUT)
    (out / 'logical-layout.c').write_text(LOGICAL_LAYOUT)
    library = bins.parent / 'libziran.a'
    if not library.is_file():
        raise RuntimeError(f'logical TypeLayout audit needs {library}; build toolchain first')
    # A sanitized libziran.a (make sanitize) needs its instrumentation flags.
    library_flags = os.environ.get('VM_CFLAGS', '').split()
    invoke([cc, *cflags, *library_flags, '-I', ROOT / 'cmd/zir', '-I', ROOT / 'bootstrap/compiler_ir', '-I', ROOT / 'include',
            out / 'logical-layout.c', library, '-lm', '-pthread', '-o', out / 'logical-layout'], 'compile', 'logical TypeLayout probe')
    logical = json.loads(invoke([out / 'logical-layout', FIXTURES, layout_source], 'audit', 'logical TypeLayout source')[0])
    if [(r['size'], r['alignment']) for r in logical] != [(8, 8), (8, 8), (16, 8), (24, 8)]:
        raise AssertionError(f'logical ABI changed; review audit expectations: {logical}')
    layout['logical'] = logical
    saved_logical = json.loads(invoke([out / 'logical-layout', out / 'ir', out / 'ir/codec_kernels_layout.zir'], 'audit', 'logical TypeLayout saved IR')[0])
    if saved_logical != logical:
        raise AssertionError('source/saved IR logical layout differs')

    for form, root, entry, audit_entry in forms:
        for target in ['c', 'go']:
            if target == 'go' and not go:
                continue
            directory = out / f'{form}-{target}'
            flags = ['--pkg', 'main'] if target == 'go' else []
            invoke([ziran, 'build', f'--target={target}', *flags, '--root', root, '-o', directory, entry], 'codegen', f'{form} {target}')
            if target == 'c':
                for mode, defines in [('dispatch', []), ('fallback', ['-DZIRAN_NO_ASM'])]:
                    binary = directory / mode
                    invoke([cc, *cflags, *defines, '-I', directory, '-I', ROOT / 'include', directory / 'codec_kernels.c',
                            out / 'host.c', '-o', binary], 'compile', f'{form} C {mode}')
                    native.append((f'{form}-C-{mode}', binary))
            else:
                (directory / 'main.go').write_text(GO_HOST)
                binary = directory / 'run'
                invoke([go, 'build', '-p=2', '-buildvcs=false', '-o', binary, '.'], 'compile', f'{form} Go', cwd=directory)
                native.append((f'{form}-Go', binary))
                capabilities['go']['execution'] = 'available'

            audit_dir = out / f'{form}-layout-{target}'
            invoke([ziran, 'build', f'--target={target}', *flags, '--root', root, '-o', audit_dir, audit_entry], 'codegen', f'{form} layout {target}')
            if target == 'c':
                command = [cc, *cflags, '-I', audit_dir, '-I', ROOT / 'include',
                           audit_dir / 'codec_kernels_layout.c', out / 'layout-host.c']
                invoke([*command, '-o', audit_dir / 'run'], 'compile', f'{form} native C layout')
            else:
                (audit_dir / 'main.go').write_text(GO_LAYOUT)
                command = [go, 'build', '-p=2', '-buildvcs=false']
                invoke([*command, '-o', audit_dir / 'run', '.'], 'compile', f'{form} native Go layout', cwd=audit_dir)
            actual = json.loads(invoke([audit_dir / 'run'], 'audit', f'{form} {target} layout and field access')[0])
            layout[f'{form}-{target}'] = actual
            if form == 'saved' and actual != layout[f'source-{target}']:
                raise AssertionError(f'{target} source/saved ABI differs')
            if target == 'c':
                _, row, diagnostic = invoke([*command, '-m32', '-o', audit_dir / 'run32'], 'compile', f'{form} C32 layout', allow_failure=True)
            elif platform.machine() in ('x86_64', 'amd64', 'i386', 'i686'):
                _, row, diagnostic = invoke([*command, '-o', audit_dir / 'run32', '.'], 'compile', f'{form} Go386 layout',
                                            extra_env={'GOARCH': '386'}, cwd=audit_dir, allow_failure=True)
            else:
                capabilities[f'{form}-{target}-32'] = {'execution': 'unavailable', 'reason': 'no configured 32-bit Go execution architecture'}
                continue
            if row['returncode']:
                capabilities[f'{form}-{target}-32'] = {'execution': 'unavailable', 'reason': diagnostic, 'command': row}
            else:
                result, row, diagnostic = invoke([audit_dir / 'run32'], 'audit', f'{form} {target}32 field access', allow_failure=True)
                if row['returncode'] in [126, 127]:
                    capabilities[f'{form}-{target}-32'] = {'execution': 'unavailable', 'reason': diagnostic, 'command': row}
                elif row['returncode']:
                    raise AssertionError(f'{form} {target}32 native field access or size_of failed: {diagnostic}')
                else:
                    layout[f'{form}-{target}-32'] = json.loads(result)
                    capabilities[f'{form}-{target}-32'] = {'execution': 'validated', 'command': row}

    # Check every available native variant independently before any timed run.
    vectors = [(rounds, seed) for rounds in [0, 1, 3, 8] for seed in [0, 1, 127, 255]]
    expected = {kind: {f'{n}:{seed}': oracle(kind, n, seed) for n, seed in vectors + [(args.rounds, 37), (8, 37)]}
                for kind in KERNELS}
    write_json(out / 'oracles.json', expected)
    for name, binary in native:
        for k, kind in enumerate(KERNELS):
            for n, seed in vectors + [(args.rounds, 37), (8, 37)]:
                actual = json.loads(invoke([binary, k, n, seed, 0], 'correctness', f'{name} {kind} n={n} seed={seed}')[0])
                if actual['result'] != expected[kind][f'{n}:{seed}']:
                    raise AssertionError(f'{name} {kind}: oracle mismatch {actual}')
    print(f'Native correctness: {len(native)} variants, {len(vectors) + 2} vectors per kernel', flush=True)

    portable = []
    validation = sum(oracle(kind, n, seed) for kind in KERNELS for n, seed in [(0, 0), (1, 255), (3, 1)])
    wasm_python = importlib.util.find_spec('wasmtime') is not None
    browser = shutil.which('firefox') or shutil.which('chromium')
    wasm_engine = wasm_python or bool(browser)
    capabilities['wasm'] = dict(generator=str(bins / 'zi2wasm'), execution='available' if wasm_engine else 'unavailable',
                                runner='Python Wasmtime' if wasm_python else f'private headless {Path(browser).name}' if browser else None,
                                reason='' if wasm_engine else 'No Python wasmtime binding, Firefox or Chromium executable; Node/npm/npx prohibited. Generated modules do not count as executed validation.')
    if wasm_python:
        from importlib.metadata import version
        metadata['toolchain']['wasmtime_python'] = version('wasmtime')
    elif browser:
        metadata['toolchain']['wasm_browser'] = invoke([browser, '--version'], 'metadata', 'Wasm browser version')[0]
        metadata['limitations'].append('Wasm process timings include Python runner, private browser startup and local HTTP harness; these are not Wasm kernel-only timings.')
    for form, root, entry, audit_entry in forms:
        for function in ['Validate', *[kind + 'Bench' for kind in KERNELS]]:
            wanted = validation if function == 'Validate' else oracle(function[:-5], 8, 37)
            bundle = out / f'{form}-{function}.zx'
            invoke([ziran, 'bundle', '--root', root, '--entry', f'codec_kernels:{function}', '-o', bundle, entry], 'codegen', f'{form} {function} bundle')
            # `run --vm` is explicitly the portable interpreter, not native dispatch.
            command = [bins / 'zi2zx', 'run', '--vm', bundle]
            actual = invoke(command, 'correctness', f'{form} portable {function}')[0]
            if actual != str(wanted):
                raise AssertionError(f'{form} portable {function}: {actual} != {wanted}')
            if function != 'Validate':
                portable.append((f'{form}-portable-{function[:-5]}', command, wanted))
            if (bins / 'zi2wasm').is_file():
                wasm = out / f'{form}-{function}.wasm'
                _, row, diagnostic = invoke([bins / 'zi2wasm', '--root', root, '--entry', f'codec_kernels:{function}',
                                              '-o', wasm, entry], 'codegen', f'{form} {function} Wasm', allow_failure=True)
                if row['returncode']:
                    capabilities[f'wasm-{form}-{function}'] = {'generation': 'unsupported', 'execution': 'unavailable', 'reason': diagnostic, 'command': row}
                else:
                    capabilities[f'wasm-{form}-{function}'] = {'generation': 'succeeded', 'execution': 'pending' if wasm_engine else 'unavailable'}
                    if wasm_engine:
                        command = [sys.executable, Path(__file__), '--run-wasm', wasm]
                        actual = invoke(command, 'correctness', f'{form} Wasm {function}')[0]
                        if actual != str(wanted):
                            raise AssertionError(f'{form} Wasm {function}: {actual} != {wanted}')
                        capabilities[f'wasm-{form}-{function}']['execution'] = 'validated'
                        if function != 'Validate':
                            portable.append((f'{form}-wasm-{function[:-5]}', command, wanted))
            else:
                capabilities['wasm']['reason'] = 'zi2wasm tool is not built'
        bundle = out / f'{form}-layout.zx'
        invoke([ziran, 'bundle', '--root', root, '--entry', 'codec_kernels_layout:Sizes', '-o', bundle, audit_entry], 'codegen', f'{form} logical size bundle')
        value = int(invoke([bins / 'zi2zx', 'run', '--vm', bundle], 'audit', f'{form} portable logical sizes')[0])
        logical_fingerprint = 8 * 1000000 + 8 * 10000 + 16 * 100 + 24
        if value != logical_fingerprint:
            raise AssertionError(f'{form} portable size_of differs from TypeLayout: {value}')
        layout[f'{form}-portable-size-fingerprint'] = value
        if (bins / 'zi2wasm').is_file():
            wasm = out / f'{form}-layout.wasm'
            _, row, diagnostic = invoke([bins / 'zi2wasm', '--root', root, '--entry',
                                         'codec_kernels_layout:Sizes', '-o', wasm, audit_entry],
                                        'codegen', f'{form} Wasm layout audit', allow_failure=True)
            capabilities[f'wasm-{form}-layout'] = {'generation': 'unsupported' if row['returncode'] else 'succeeded',
                                                  'execution': 'unavailable', 'reason': diagnostic, 'command': row}
            if not row['returncode'] and wasm_engine:
                actual = int(invoke([sys.executable, Path(__file__), '--run-wasm', wasm],
                                    'audit', f'{form} Wasm logical sizes')[0])
                if actual != logical_fingerprint:
                    raise AssertionError(f'{form} Wasm size_of differs from TypeLayout: {actual}')
                layout[f'{form}-wasm-size-fingerprint'] = actual
                capabilities[f'wasm-{form}-layout']['execution'] = 'validated'

    # Byte equality supplements execution; it does not replace it.
    for target, suffix in [('c', 'c'), ('c', 'h'), ('go', 'go')]:
        if target == 'go' and not go:
            continue
        for module in ['codec_kernels', 'codec_kernels_layout']:
            folder = f'layout-{target}' if module.endswith('layout') else target
            left = out / f'source-{folder}' / f'{module}.{suffix}'
            right = out / f'saved-{folder}' / f'{module}.{suffix}'
            same = digest(left) == digest(right)
            parity.append(dict(artifact=f'{module}.{suffix}', identical=same))
            if not same:
                raise AssertionError(f'source/saved generated {module}.{suffix} differs')
    for function in ['Validate', *[kind + 'Bench' for kind in KERNELS], 'layout']:
        for suffix in ['zx', 'wasm']:
            left, right = out / f'source-{function}.{suffix}', out / f'saved-{function}.{suffix}'
            if left.exists() and right.exists():
                same = digest(left) == digest(right)
                parity.append(dict(artifact=f'{function}.{suffix}', identical=same))
                if not same:
                    raise AssertionError(f'source/saved {function}.{suffix} differs')
    write_json(out / 'source-saved-parity.json', parity)
    layout['interpretation'] = 'Native size_of matches actual native objects and field access where executed. Portable size_of matches logical TypeLayout. Differences are audit findings, not demonstrated memory corruption.'
    write_json(out / 'layout-audit.json', layout)
    write_json(out / 'capabilities.json', capabilities)
    print('Portable correctness, source/saved parity and layout audit complete; starting timing', flush=True)

    rng = random.Random(20261104)
    jobs = []
    for name, binary in native:
        for index, kind in enumerate(KERNELS):
            for n in sorted(set([8, args.rounds])):
                for warm in [0, 1]:
                    jobs.append((f'{name}-{kind}-n{n}', [binary, index, n, 37, warm],
                                 oracle(kind, n, 37), 'warm_native' if warm else 'cold_process', kind, n))
    for name, command, wanted in portable:
        kind = name.split('-')[-1]
        jobs.append((name, command, wanted, 'portable_process', kind, 8))
    for repetition in range(args.repetitions):
        rng.shuffle(jobs)
        for name, command, wanted, mode, kind, n in jobs:
            stdout, row, _ = invoke(command, 'timing', name)
            result = json.loads(stdout) if stdout.startswith('{') else {'result': int(stdout)}
            if result['result'] != wanted or (mode == 'warm_native' and result['warm_result'] != wanted):
                raise AssertionError(f'timing output mismatch: {name}: {stdout}')
            row.update(result, mode=mode, kernel=kind, rounds=n, repetition=repetition,
                       input_samples=256 * n, result_sha256=hashlib.sha256(str(wanted).encode()).hexdigest())
            samples.append(row)
            with (out / 'samples.jsonl').open('a') as stream:
                stream.write(json.dumps(row) + '\n')
        print(f'Timing repetition {repetition + 1}/{args.repetitions}', flush=True)
    groups = {}
    for row in samples:
        groups.setdefault((row['case'], row['mode']), []).append(row)
    summary = []
    for (case, mode), rows in groups.items():
        item = dict(case=case, mode=mode, repetitions=len(rows), rounds=rows[0]['rounds'])
        for metric in ['process_wall_ns', 'process_user_s', 'process_system_s', 'kernel_wall_ns', 'kernel_cpu_ns']:
            values = [row[metric] for row in rows if row.get(metric) is not None]
            if values:
                values.sort()
                item[metric] = dict(median=statistics.median(values), min=values[0], max=values[-1],
                                    p95=values[min(len(values) - 1, int(len(values) * .95))])
        summary.append(item)
    write_json(out / 'summary.json', summary)

    objdump = shutil.which('objdump')
    capabilities['native-disassembly'] = {'status': 'available' if objdump else 'unavailable'}
    if objdump:
        metadata['toolchain']['objdump'] = invoke([objdump, '--version'], 'metadata', 'objdump version')[0]
        for name, binary in native:
            if '-C-' in name:
                # Save full disassembly to disk; never load it into model memory.
                for symbol in ['codec_kernels_Swap32', *KERNELS]:
                    _, row, _ = invoke([objdump, '-d', f'--disassemble={symbol}', binary], 'disassembly', f'{name} {symbol}')
                    capabilities['native-disassembly'].setdefault('artifacts', []).append(row['stdout_file'])
                    if symbol == 'BytePlane':
                        found = False
                        with (out / row['stdout_file']).open() as stream:
                            for line in stream:
                                found |= '\tbswap' in line
                        capabilities['native-disassembly'].setdefault('bswap_in_BytePlane', {})[name] = found
                        if platform.machine() in ('x86_64', 'i386', 'i686') and '-dispatch' in name and not found:
                            raise AssertionError(f'{name}: expected existing #asm bswap instruction in BytePlane')
    metadata['artifacts'] = {str(path.relative_to(out)): digest(path) for path in out.rglob('*')
                             if path.is_file() and not set(path.relative_to(out).parts) & {'go-cache', 'go-tmp', 'tmp', 'commands'}
                             and path.suffix not in ['.json', '.jsonl']}
    metadata['revision_after'] = git('rev-parse', 'HEAD')
    metadata['dirty_status_after'] = git('status', '--short')
    metadata['compiler_sources_changed_during_run'] = [name for name, previous in metadata['compiler_source_hashes'].items()
                                                       if not (ROOT / name).is_file() or digest(ROOT / name) != previous]
    # Other work may commit in this checkout meanwhile, so HEAD is only
    # recorded. What was built and measured must stay unchanged: the hashed
    # tools and inputs, and the headers the C builds above include.
    built_headers = [name for name in metadata['compiler_sources_changed_during_run']
                     if name.endswith('.h') and (name.startswith('include/') or
                         Path(name).parent in (Path('cmd/zir'), Path('bootstrap/compiler_ir')))]
    if built_headers:
        raise RuntimeError('headers the benchmark built against changed during run: '
                           + ', '.join(built_headers) + '; rerun on stable sources')
    if any(digest(path) != metadata['input_hashes'][str(path.relative_to(ROOT))] for path in inputs):
        raise RuntimeError('benchmark inputs changed during run; rerun on stable inputs')
    for path, identity in metadata['tools'].items():
        if digest(path) != identity['sha256']:
            raise RuntimeError(f'tool changed during run: {path}')
    capabilities['native-c'] = {'execution': 'validated', 'modes': ['dispatch', 'ZIRAN_NO_ASM']}
    capabilities['portable-zx'] = {'execution': 'validated', 'runner': str(bins / 'zi2zx') + ' run --vm'}
    wasm_results = [value['execution'] for key, value in capabilities.items()
                    if key.startswith('wasm-') and key.endswith('Bench')]
    if wasm_results and all(value == 'validated' for value in wasm_results):
        capabilities['wasm']['execution'] = 'validated'
    write_json(out / 'capabilities.json', capabilities)
    write_json(out / 'metadata.json', metadata)
    print(f'PASS: {len(samples)} checked timing samples; metadata, raw commands, source/saved parity, layout and disassembly: {out}', flush=True)
    print('Wasm execution: ' + capabilities['wasm']['execution'] + '; ' + capabilities['wasm']['reason'], flush=True)
    for key, value in capabilities.items():
        if key.endswith('-32'):
            print(f'{key}: {value["execution"]}', flush=True)


if __name__ == '__main__':
    if len(sys.argv) == 3 and sys.argv[1] == '--run-wasm':
        run_wasm(sys.argv[2])
    else:
        main()
