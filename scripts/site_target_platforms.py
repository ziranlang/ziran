#!/usr/bin/env python3
"""Build and measure the same matrix entries on desktop, USB Android and web.

Run the complete command through build-queue. Android needs an explicit USB
serial. All samples consume the result and check an independent oracle. Warm
execution excludes loading, process startup, downloads and offline compilation.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import platform
import random
import re
import shlex
import shutil
import statistics
import subprocess
import sys
import time

import site_targets as base
from benchmark_samples import summarize_samples

ROOT = base.REPO
SITE = base.SITE
WORK = ROOT / 'build/site-target-platforms'
WEB = SITE / 'generated/targets/web'
TARGETS = ('c', 'go', 'zx', 'wasm')
NDK_VERSION = '28.2.13676358'


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def run(command, *, cwd=ROOT, env=None, timeout=300):
    result = subprocess.run(list(map(str, command)), cwd=cwd, env=env, capture_output=True, text=True, timeout=timeout)
    if result.returncode:
        raise RuntimeError(shlex.join(map(str, command)) + '\n' + result.stdout + result.stderr)
    return result.stdout.strip()


def environment():
    env = dict(os.environ, GO111MODULE='off', GOMAXPROCS='2', HARMONY_DESKTOP_RECOVERY='0', YUE_DESKTOP_RECOVERY='0')
    env['EM_CACHE'] = str(ROOT / 'build/emscripten-cache')
    for key in ('DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'DBUS_SESSION_BUS_ADDRESS',
                'ADB_SERVER_SOCKET', 'ANDROID_ADB_SERVER_PORT', 'ADB_SERVER_PORT', 'ANDROID_SERIAL',
                'GOFLAGS', 'ZIRAN_PROFILE', 'ZIRAN_VM_PROFILE'):
        env.pop(key, None)
    return env


def public(path):
    return {'path': path.relative_to(SITE).as_posix(), 'bytes': path.stat().st_size, 'sha256': sha(path)}


def summarize(result, expected):
    assert result.get('result') == str(expected), (result, expected)
    batches = result['batches']
    assert isinstance(batches, int) and batches > 0
    raw = result['batch_samples_ms']
    assert len(raw) >= 3 and all(value > 0 for value in raw)
    values = [value / batches for value in raw]
    analysis = summarize_samples(values)
    return {**result, 'execution_samples_ms': values, 'sample_statistics': analysis, 'execution_ms': analysis['median_ms'],
            'checksum': str(int(expected) * batches)}


def read_measurement(text, example):
    lines = text.splitlines(keepends=True)
    result = json.loads(lines[-1])
    observed = ''.join(lines[:-1])
    assert observed == example['stdout'], (example['id'], observed, example['stdout'])
    result['stdout_verified'] = True
    return result


def paths(example, target):
    return [SITE / file['path'] for file in example['targets'][target]['files']]


def c_symbol(example):
    header = next(path for path in paths(example, 'c') if path.name == example.get('module', example['id']) + '.h')
    symbol = re.search(r'\b(\w*' + example['entry'] + r')\(void\)', header.read_text())
    assert symbol, header
    return header, symbol[1]


def go_source(example, directory, web=False):
    directory.mkdir(parents=True, exist_ok=True)
    files = []
    for path in paths(example, 'go'):
        target = directory / path.name
        shutil.copyfile(path, target)
        files.append(target)
    primary = next(path for path in files if path.name == example.get('module', example['id']) + '.go')
    entry = example['entry'][0].upper() + example['entry'][1:]
    symbol = re.search(r'func (\w+_' + entry + r')\(', primary.read_text())[1]
    bridge = directory / 'matrix_bridge.go'
    call = f'{symbol}(); return 0' if example['stdout'] else f'return float64({symbol}())'
    if web:
        bridge.write_text('package main\nimport "syscall/js"\nvar matrixCallback js.Func\n'
                          'func main() { matrixCallback = js.FuncOf(func(_ js.Value, _ []js.Value) any { '
                          + call + ' }); js.Global().Set("matrixGoRun", matrixCallback); select {} }\n')
    else:
        body = f'{symbol}(); return 0' if example['stdout'] else f'return int64({symbol}())'
        bridge.write_text(f'package main\nfunc matrixEntry() int64 {{ {body} }}\n')
        host = directory / 'matrix_host.go'
        shutil.copyfile(ROOT / 'bench/site_target_host.go', host)
        files.append(host)
    files.append(bridge)
    return files


def c_build(example, cc, output, extra=()):
    header, symbol = c_symbol(example)
    sources = [path for path in paths(example, 'c') if path.suffix == '.c']
    command = [cc, '-O2', '-std=c11', '-DMATRIX_NATIVE', '-I' + str(header.parent),
               '-DMATRIX_HEADER="' + header.name + '"', '-DMATRIX_ENTRY=' + symbol,
               *(['-DMATRIX_VOID'] if example['stdout'] else []), *extra, ROOT / 'bench/site_target_host.c', *sources, '-lm', '-o', output]
    run(command, env=environment())
    return command


def runtime_build(cc, library, output, extra=()):
    libraries = ['-lm'] if '-DMATRIX_WEB' in extra else ['-lm', '-pthread', '-ldl']
    command = [cc, '-O2', '-std=c11', '-DMATRIX_ZX', '-I' + str(ROOT / 'include'),
               *extra, ROOT / 'bench/site_target_host.c', library, *libraries, '-o', output]
    run(command, env=environment())
    return command


def metadata(label, host, runtimes, samples):
    return {'label': label, 'host': host, 'measured_at': datetime.now(timezone.utc).isoformat(),
            'samples': samples, 'runtimes': runtimes, 'examples': {},
            'methodology': 'Median warm time per entry after symmetric 1.5×IQR outlier filtering; raw samples and exclusions are retained. Each runtime warms for at least 150 ms, calibrates a batch, '
                           'then measures the recorded number of batches. Every timed return-value checksum is verified. Printed output is verified before timing; timed output uses a null sink (native) or byte callback (web). Formatting and runtime-specific I/O remain included. '
                           'Includes the host call boundary and budget reset where required; excludes loading, '
                           'process startup, downloads and offline compilation. Tiny examples emphasize call overhead. '
                           'Shared machines and uncontrolled clocks; no universal target ranking.'}


def desktop(data, samples):
    out = WORK / 'desktop'
    out.mkdir(parents=True, exist_ok=True)
    library = ROOT / 'build/libziran.a'
    zx = out / 'zx-runner'
    runtime_build(os.environ.get('CC', 'cc'), library, zx)
    result = metadata('Desktop', platform.system() + ' ' + platform.machine() + ' · ' + base.cpu_name(),
                      {'c': 'C · cc -O2', 'go': 'Go · native', 'zx': 'ZX · instruction engine', 'wasm': 'WebAssembly · Node / V8'}, samples)
    jobs = []
    for example in data['examples']:
        directory = out / example['id']
        directory.mkdir(parents=True, exist_ok=True)
        c = directory / 'c-program'
        c_build(example, os.environ.get('CC', 'cc'), c)
        go = directory / 'go-program'
        run(['go', 'build', '-p=2', '-o', go, *go_source(example, directory / 'go')], env=environment())
        for target, command in [
            ('c', [c, '-', example['expected'], samples]),
            ('go', [go, '-', example['expected'], samples]),
            ('zx', [zx, paths(example, 'zx')[0], example['expected'], samples]),
            ('wasm', ['node', ROOT / 'scripts/site_target_node.cjs', paths(example, 'wasm')[0], example['expected'], samples]),
        ]:
            jobs.append((example, target, command))
    random.Random(42).shuffle(jobs)
    for example, target, command in jobs:
        measured = summarize(read_measurement(run(command, env=environment()), example), example['expected'])
        measured['command'] = base.command_text(command)
        measured['artifact_sha256'] = sha(command[0] if target in ('c', 'go', 'zx') else paths(example, 'wasm')[0])
        result['examples'].setdefault(example['id'], {})[target] = measured
    return result


def mobile(data, samples, serial, ndk):
    if not serial:
        raise RuntimeError('Mobile measurement requires an explicit --serial for the USB test device')
    env = environment()
    def adb(*args):
        return run(['adb', '-s', serial, *args], env=env, timeout=180)
    def shell(*args):
        return adb('shell', shlex.join(map(str, args)))
    assert adb('get-state') == 'device'
    assert shell('getprop', 'ro.product.cpu.abi') == 'armeabi-v7a', 'This runner targets the USB ARMv7 Moto'
    model = shell('getprop', 'ro.product.model')
    release = shell('getprop', 'ro.build.version.release')
    result = metadata('Mobile', model + ' · Android ' + release + ' · ARMv7',
                      {'c': 'C · Android Clang -O2', 'go': 'Go · Android / ARMv7', 'zx': 'ZX · instruction engine', 'wasm': 'WebAssembly · WAMR AOT'}, samples)
    result['serial_sha256'] = hashlib.sha256(serial.encode()).hexdigest()
    def battery():
        text = shell('dumpsys', 'battery')
        return {key: value.strip() for key, value in re.findall(r'^\s*(level|temperature|status):\s*(.+)$', text, re.M)}
    result['battery_before'] = battery()
    tools = ndk / 'toolchains/llvm/prebuilt/linux-x86_64/bin'
    cc = tools / 'armv7a-linux-androideabi21-clang'
    assert cc.is_file(), cc
    out = WORK / 'mobile'
    out.mkdir(parents=True, exist_ok=True)
    library = ROOT / 'build/bundle-runtime/android-armeabi-v7a/libziran.a'
    # The ordinary Android portable archive is maintained by the repository.
    run(['make', '-j4', 'BOOTSTRAP=1', 'BUILD_DIR=' + str(library.parent), 'CC=' + str(cc),
         'AR=' + str(tools / 'llvm-ar'), 'OBJCOPY=' + str(tools / 'llvm-objcopy'), 'CFLAGS=-O2 -fPIC', library], env=env, timeout=600)
    zx = out / 'zx-runner'
    runtime_build(cc, library, zx, ['-llog'])
    wamr = ROOT / 'build/benchmarks/zx-compare-apk/libwamr.a'
    assert wamr.is_file(), 'Build the existing bench/zx_compare_apk WAMR target first'
    wasm_runner = out / 'wasm-runner'
    run([cc, '-O2', '-std=c11', '-DMATRIX_WAMR', '-I' + str(ROOT / 'build/deps/wasm-micro-runtime-WAMR-2.4.5/core/iwasm/include'),
         '-I' + str(ROOT / 'cmd/zir'), '-I' + str(ROOT / 'bootstrap/compiler_ir'), '-I' + str(ROOT / 'include'), ROOT / 'bench/site_target_host.c', library, wamr, '-lm', '-ldl', '-pthread', '-llog', '-o', wasm_runner], env=env)
    result['runtime_hashes'] = {'ziran': sha(library), 'wamr': sha(wamr)}
    result['toolchains'] = {'ndk': run([cc, '--version'], env=env).splitlines()[0], 'go': run(['go', 'version'], env=env)}
    remote = '/data/local/tmp/ziran-target-matrix'
    shell('mkdir', '-p', remote)
    def push(file, name):
        destination = remote + '/' + name
        adb('push', file, destination)
        assert shell('sha256sum', destination).split()[0] == sha(file), 'USB artifact hash mismatch'
        return destination
    remote_zx = push(zx, 'zx-runner')
    remote_wasm = push(wasm_runner, 'wasm-runner')
    shell('chmod', '700', remote_zx, remote_wasm)
    jobs = []
    for example in data['examples']:
        directory = out / example['id']
        directory.mkdir(parents=True, exist_ok=True)
        c = directory / 'c-program'
        c_build(example, cc, c)
        go = directory / 'go-program'
        selected = dict(env, GOOS='android', GOARCH='arm', GOARM='7', CGO_ENABLED='1', CC=str(cc))
        run(['go', 'build', '-p=2', '-o', go, *go_source(example, directory / 'go')], env=selected)
        aot = directory / 'program.aot'
        run([ROOT / 'build/deps/wamrc-2.4.5/wamrc', '--opt-level=3', '--target=armv7', '--target-abi=eabi',
             '--cpu=cortex-a9', '--cpu-features=-neon', '-o', aot, paths(example, 'wasm')[0]], env=env)
        uploaded = {key: push(file, example['id'] + '-' + key) for key, file in [('c', c), ('go', go), ('zx', paths(example, 'zx')[0]), ('wasm', aot)]}
        shell('chmod', '700', uploaded['c'], uploaded['go'])
        for target, executable, artifact in [('c', uploaded['c'], '-'), ('go', uploaded['go'], '-'),
                                              ('zx', remote_zx, uploaded['zx']), ('wasm', remote_wasm, uploaded['wasm'])]:
            jobs.append((example, target, [executable, artifact, example['expected'], samples], sha({'c': c, 'go': go, 'zx': paths(example, 'zx')[0], 'wasm': aot}[target])))
    random.Random(42).shuffle(jobs)
    for example, target, command, digest in jobs:
        measured = summarize(read_measurement(shell(*command), example), example['expected'])
        measured.update(command=shlex.join(map(str, command)), artifact_sha256=digest)
        result['examples'].setdefault(example['id'], {})[target] = measured
    result['battery_after'] = battery()
    return result


def build_web(data, emcc):
    env = environment()
    env['EM_CACHE'] = str(ROOT / 'build/emscripten-cache')
    WEB.mkdir(parents=True, exist_ok=True)
    runtime = ROOT / 'build/playground64/libziran.a'
    run(['make', '-j4', 'BUILD_DIR=' + str(runtime.parent), 'CC=' + shlex.quote(str(emcc)) + ' -sMEMORY64=2',
         'AR=' + str(emcc.parent / 'emar'), 'OBJCOPY=true', 'CFLAGS=-O2',
         'FRAMEFLAGS=-Wframe-larger-than=16384', runtime], env=env, timeout=600)
    zx_script = WEB / 'zx-runtime.js'
    common = ['-sMEMORY64=2', '-sMODULARIZE=1', '-sENVIRONMENT=worker,node', '-sALLOW_MEMORY_GROWTH=1',
              '-sMAXIMUM_MEMORY=536870912', '-sSTACK_SIZE=8388608', '-sFORCE_FILESYSTEM=1', '-DMATRIX_WEB']
    runtime_build(emcc, runtime, zx_script, [*common, '-I' + str(ROOT / 'bootstrap/compiler_ir'),
                  '-sEXPORT_NAME=createMatrixZX', '-sEXPORTED_FUNCTIONS=_MatrixOpen,_MatrixRun', '-sEXPORTED_RUNTIME_METHODS=FS,ccall'])
    goroot = Path(run(['go', 'env', 'GOROOT'], env=env))
    go_runtime = next(path for path in [goroot / 'lib/wasm/wasm_exec.js', goroot / 'misc/wasm/wasm_exec.js'] if path.is_file())
    shutil.copyfile(go_runtime, WEB / 'wasm_exec.js')
    licenses = [goroot / 'LICENSE', Path('/usr/share/doc') / (goroot.name.replace('go-', 'golang-') + '-go') / 'copyright']
    license_file = next((path for path in licenses if path.is_file()), None)
    if license_file is None:
        raise RuntimeError('Go distribution license not found beside GOROOT or in its Debian package')
    shutil.copyfile(license_file, WEB / 'go-LICENSE.txt')
    for example in data['examples']:
        c_script = WEB / (example['id'] + '-c.js')
        c_build(example, emcc, c_script, [*common, '-sEXPORT_NAME=createMatrixC', '-sEXPORTED_FUNCTIONS=_MatrixRun'])
        go = WEB / (example['id'] + '-go.wasm')
        directory = WORK / 'web' / example['id'] / 'go'
        selected = dict(env, GOOS='js', GOARCH='wasm', CGO_ENABLED='0')
        run(['go', 'build', '-p=2', '-o', go, *go_source(example, directory, web=True)], env=selected)
        example['targets']['c']['web'] = {'script': c_script.relative_to(SITE).as_posix(), 'files': [public(c_script), public(c_script.with_suffix('.wasm'))]}
        example['targets']['go']['web'] = {'path': go.relative_to(SITE).as_posix(), 'runtime': (WEB / 'wasm_exec.js').relative_to(SITE).as_posix(),
                                           'files': [public(go), public(WEB / 'wasm_exec.js'), public(WEB / 'go-LICENSE.txt')]}
        example['targets']['zx']['web'] = {'script': zx_script.relative_to(SITE).as_posix(), 'files': [public(zx_script), public(zx_script.with_suffix('.wasm'))]}
    return data


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--platform', choices=['desktop', 'mobile', 'web', 'all'], default='all')
    parser.add_argument('--serial')
    parser.add_argument('--samples', type=int, default=5)
    parser.add_argument('--ndk', type=Path, default=Path.home() / 'Android/Sdk/ndk' / NDK_VERSION)
    parser.add_argument('--emcc', type=Path, default=Path.home() / 'emsdk/upstream/emscripten/emcc')
    parser.add_argument('--rebuild-outputs', action='store_true')
    args = parser.parse_args()
    if not 3 <= args.samples <= 31:
        parser.error('--samples must be between 3 and 31')
    if args.rebuild_outputs:
        base.build(ROOT / 'build/bin', args.samples, render_page=False)
    data = json.loads((base.OUTPUT / 'matrix.json').read_text())
    data['schema_version'] = 2
    data.setdefault('platforms', {})
    WORK.mkdir(parents=True, exist_ok=True)
    def checkpoint():
        (base.OUTPUT / 'matrix.json').write_text(json.dumps(data, indent=2) + '\n')
    if args.platform in ('desktop', 'all'):
        data['platforms']['desktop'] = desktop(data, args.samples)
        checkpoint()
        print('Desktop: 20 verified target measurements', flush=True)
    if args.platform in ('mobile', 'all'):
        data['platforms']['mobile'] = mobile(data, args.samples, args.serial, args.ndk.resolve())
        checkpoint()
        print('USB Android: 20 verified target measurements', flush=True)
    if args.platform in ('web', 'all'):
        build_web(data, args.emcc.resolve())
        manifest = WORK / 'web-manifest.json'
        manifest.write_text(json.dumps(data) + '\n')
        output = WORK / 'web-measurements.json'
        run(['node', ROOT / 'scripts/site_target_browser.mjs', manifest, output], env=environment(), timeout=300)
        measured = json.loads(output.read_text())
        result = metadata('Web', measured['browser'], {'c': 'C · Emscripten / Wasm', 'go': 'Go · js/wasm',
                          'zx': 'ZX · browser engine', 'wasm': 'WebAssembly · browser / V8'}, args.samples)
        for example in data['examples']:
            result['examples'][example['id']] = {target: summarize(measured['examples'][example['id']][target], example['expected']) for target in TARGETS}
        data['platforms']['web'] = result
        print('Web browser: 20 verified target measurements', flush=True)
    data['platform_harnesses'] = {name: sha(ROOT / name) for name in ['scripts/site_target_platforms.py', 'scripts/site_target_node.cjs',
        'scripts/site_target_browser.mjs', 'bench/site_target_host.c', 'bench/site_target_host.go', 'site/target-measure.js', 'site/target-benchmark-worker.js']}
    (base.OUTPUT / 'matrix.json').write_text(json.dumps(data, indent=2) + '\n')
    (SITE / 'targets.html').write_text(base.render(data))
    print('Platform snapshot saved', flush=True)


if __name__ == '__main__':
    try:
        main()
    except (RuntimeError, AssertionError, StopIteration) as error:
        sys.exit(str(error))
