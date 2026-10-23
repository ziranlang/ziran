#!/usr/bin/env python3
"""Compare saved VM libraries on one explicitly selected Android device."""
import argparse
import hashlib
import json
from pathlib import Path
import random
import re
import shlex
import statistics
import time

from android_execution import TARGETS, environment, execute, sha
from vm_execution import ROOT, workloads


def prepare(args):
    out, vm = args.out.resolve(), args.vm_results.resolve()
    if out.exists():
        raise ValueError('Preserve receipts and remove inactive outputs before reusing --out')
    labels = [label for label, _ in args.library]
    if len(set(labels)) != len(labels) or any(not re.fullmatch(r'[A-Za-z0-9_-]+', s) for s in labels):
        raise ValueError('Library labels must be unique simple names')
    fixtures = json.loads((vm/'fixtures.json').read_text())
    expected = {name: (source, answer) for name, source, answer in workloads(fixtures['iterations'])}
    if {case['name'] for case in fixtures['cases']} != set(expected):
        raise ValueError('Fixture inventory differs from the independent workload generator')
    out.mkdir(parents=True)
    env = environment(); env['YUE_DESKTOP_RECOVERY'] = '0'
    cc = args.ndk.resolve()/'toolchains/llvm/prebuilt/linux-x86_64/bin'/(TARGETS[args.abi][0]+'21-clang')
    manifest = dict(schema=1, abi=args.abi, files={}, variants={}, cases=[], libraries={},
                    harness_sha256=sha(Path(__file__)), runner_source_sha256=sha(ROOT/'bench/vm_startup.c'),
                    helper_sha256=sha(ROOT/'bench/android_execution.py'),
                    fixture_manifest_sha256=sha(vm/'fixtures.json'),
                    compiler=execute([cc, '--version'], env, capture_output=True, text=True).stdout,
                    caveats=['Libraries must match the current VM API and selected ABI.',
                             'One device and warm caches; CPU frequency, temperature and background work remain uncontrolled.',
                             'Each case/repetition runs all variants consecutively; rotations balance positions across repetitions.',
                             'Optional CPU affinity applies only to benchmark child processes; device settings are unchanged.',
                             'Every first and steady execution must match the independent fixture oracle.',
                             'Tracked live values exclude stacks, IR, metadata and allocator overhead.'])
    for label, library in args.library:
        library = Path(library).resolve(); runner = out/('runner-'+label)
        argv = [cc, '-O2', '-std=c11', '-D_GNU_SOURCE', '-I'+str(ROOT/'include'),
                '-I'+str(ROOT/'cmd/zir'), ROOT/'bench/vm_startup.c', library,
                '-lm', '-pthread', '-o', runner]
        execute(argv, env)
        manifest['files'][runner.name] = sha(runner)
        manifest['variants'][label] = runner.name
        manifest['libraries'][label] = dict(path=str(library), sha256=sha(library), argv=list(map(str, argv)))
    for case in fixtures['cases']:
        name = case['name']; source, answer = expected[name]
        module = name.replace('-', '_'); bundle = vm/'fixtures'/(module+'.zx')
        if case['expected'] != answer or (vm/'fixtures'/(module+'.zi')).read_text() != source:
            raise ValueError('Fixture source or oracle changed: '+name)
        copied = out/(module+'.zx'); copied.write_bytes(bundle.read_bytes())
        manifest['files'][copied.name] = sha(copied)
        manifest['cases'].append(dict(name=name, bundle=copied.name, expected=answer))
    (out/'manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')
    print(f'Prepared {len(labels)} VM libraries and {len(expected)} independently checked workloads.')


def run(args):
    out = args.out.resolve(); manifest = json.loads((out/'manifest.json').read_text())
    env = environment(); env['YUE_DESKTOP_RECOVERY'] = '0'
    referenced = set(manifest['variants'].values()) | {case['bundle'] for case in manifest['cases']}
    if not referenced <= manifest['files'].keys() or sha(Path(__file__)) != manifest['harness_sha256']:
        raise ValueError('Unknown artifact reference or changed comparison harness')
    for name, digest in manifest['files'].items():
        if Path(name).name != name or sha(out/name) != digest:
            raise ValueError('Invalid artifact name or changed file: '+name)
    result = dict(manifest=manifest, samples=[], complete=False, cpu=args.cpu,
                  repetitions=args.repetitions, runs=args.runs, started_utc=time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()))
    remote = None
    output = (out/'results.json').open('x')
    def save():
        output.seek(0); json.dump(result, output, indent=2); output.write('\n'); output.truncate(); output.flush()
    def adb(*argv):
        return execute(['adb', '-s', args.serial, *map(str, argv)], env,
                       capture_output=True, text=True, timeout=150).stdout.strip()
    def shell(*argv):
        return adb('shell', shlex.join(list(map(str, argv))))
    def observe():
        return {key: value.strip() for key, value in re.findall(
            r'^\s*(level|temperature|status|USB powered):\s*(.+)$', shell('dumpsys', 'battery'), re.M)}
    save()
    try:
        assert adb('get-state') == 'device'
        assert shell('getprop', 'ro.product.cpu.abi') == manifest['abi']
        result['device'] = {key: shell('getprop', key) for key in (
            'ro.product.model', 'ro.build.version.release', 'ro.build.version.sdk', 'ro.product.cpu.abi')}
        result['serial_sha256'] = hashlib.sha256(args.serial.encode()).hexdigest()
        result['before'] = observe()
        affinity = ['taskset', format(1 << args.cpu, 'x')] if args.cpu is not None else []
        if affinity:
            status = shell(*affinity, 'cat', '/proc/self/status')
            allowed = re.search(r'^Cpus_allowed_list:\s*(.+)$', status, re.M)
            assert allowed and allowed.group(1).strip() == str(args.cpu)
            result['verified_cpu_affinity'] = allowed.group(1).strip()
        allocated = shell('mktemp', '-d', '/data/local/tmp/ziran-vm.XXXXXX')
        assert re.fullmatch(r'/data/local/tmp/ziran-vm\.[A-Za-z0-9]+', allocated)
        remote = allocated; result['remote'] = remote; save()
        for name, digest in manifest['files'].items():
            adb('push', out/name, remote+'/'+name)
            assert shell('sha256sum', remote+'/'+name).split()[0] == digest
            if name in manifest['variants'].values():
                shell('chmod', '700', remote+'/'+name)
        groups = [(case, repetition) for repetition in range(args.repetitions+1) for case in manifest['cases']]
        rng = random.Random(20261016); orders = {}; width = len(manifest['variants'])
        for case in manifest['cases']:
            for block in range((args.repetitions+width-1)//width):
                labels = list(manifest['variants']); rng.shuffle(labels)
                orders[case['name'], block] = labels
        rng.shuffle(groups); groups.sort(key=lambda row: row[1] != 0)
        for case, repetition in groups:
            if repetition == 0:
                labels = list(manifest['variants']); rng.shuffle(labels)
            else:
                block, offset = divmod(repetition-1, width)
                base = orders[case['name'], block]; labels = base[offset:]+base[:offset]
            for position, label in enumerate(labels):
                row = json.loads(shell(*affinity, 'timeout', '120', remote+'/'+manifest['variants'][label],
                                       remote+'/'+case['bundle'], args.runs, case['expected']))
                assert row['result'] == case['expected'] and row['runs'] == args.runs
                result['samples'].append(dict(case=case['name'], variant=label, repetition=repetition,
                                              position=position, warmup=repetition == 0, **row))
                save()
        result['after'] = observe(); result['summary'] = []
        for case in manifest['cases']:
            for label in manifest['variants']:
                rows = [r for r in result['samples'] if r['case'] == case['name'] and r['variant'] == label and not r['warmup']]
                assert len(rows) == args.repetitions
                positions = [sum(r['position'] == p for r in rows) for p in range(width)]
                assert max(positions)-min(positions) <= 1
                result['summary'].append(dict(case=case['name'], variant=label, samples=len(rows), **{
                    'median_'+key: statistics.median(r[key] for r in rows) for key in
                    ('load_ms', 'open_ms', 'first_run_ms', 'steady_run_ms', 'close_ms', 'live_value_bytes')}))
        result['checks_passed'] = True
    except Exception as error:
        result['error'] = repr(error)
        raise
    finally:
        if remote:
            try:
                shell('rm', '-rf', remote); shell('test', '!', '-e', remote); result['cleaned'] = True
            except Exception as error:
                result['cleanup_error'] = repr(error)
        result['complete'] = bool(result.get('checks_passed') and result.get('cleaned'))
        result['finished_utc'] = time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()); save(); output.close()
    if not result['complete']:
        raise RuntimeError('Inspect the saved attempt; remote cleanup or measurements are incomplete')
    print(f'{sum(not r["warmup"] for r in result["samples"])} measured samples passed; device artifacts removed.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    prep = commands.add_parser('prepare')
    prep.add_argument('--vm-results', type=Path, required=True)
    prep.add_argument('--out', type=Path, required=True)
    prep.add_argument('--ndk', type=Path, required=True)
    prep.add_argument('--abi', choices=TARGETS, required=True)
    prep.add_argument('--library', nargs=2, action='append', required=True, metavar=('LABEL', 'PATH'))
    device = commands.add_parser('run')
    device.add_argument('--out', type=Path, required=True)
    device.add_argument('--serial', required=True)
    device.add_argument('--cpu', type=int)
    device.add_argument('--repetitions', type=int, default=9)
    device.add_argument('--runs', type=int, default=9)
    args = parser.parse_args()
    if args.command == 'run' and (args.repetitions < 1 or not 1 <= args.runs <= 10000 or
                                  (args.cpu is not None and not 0 <= args.cpu < 256)):
        parser.error('Positive repetitions, 1..10000 runs and a CPU in 0..255 are required')
    prepare(args) if args.command == 'prepare' else run(args)


if __name__ == '__main__':
    main()
