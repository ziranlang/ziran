#!/usr/bin/env python3
"""Compare verified VM execution using independently checked numeric and record workloads."""
import argparse
import hashlib
import json
import json
import os
from pathlib import Path
import subprocess
import sys

import cpu_features

ROOT = Path(__file__).resolve().parents[1]
MASK = (1 << 64) - 1


def workloads(iterations):
    state, total = 123456789, 0
    for _ in range(iterations):
        state = (state * 1664525 + 1013904223) & 2147483647
        total += state
    yield 'arithmetic', '''#program_export
Answer :: () -> s64 {
    value: s64 = 123456789
    total: s64
    i: s32
    while i < ITERATIONS {
        value = (value * 1664525 + 1013904223) & 2147483647
        total += value
        i += 1
    }
    return total
}
'''.replace('ITERATIONS', str(iterations)), total

    state, total = MASK, 0
    for i in range(iterations):
        state ^= state >> 7
        state ^= (state << 13) & MASK
        total += int(state > (1 << 53)) + 2 * int(state == i) + (state & 255)
        signed = i - 5000
        total += (abs(signed) // 7) * (-1 if signed < 0 else 1)
    yield 'mixed-integers', '''#program_export
Answer :: () -> s64 {
    state: u64 = 18446744073709551615
    total: s64
    i: s32
    while i < ITERATIONS {
        state ^= state >> 7
        state ^= state << 13
        if state > 9007199254740992 { total += 1 }
        if state == cast(u64)i { total += 2 }
        total += cast(s64)(state & 255)
        signed: s32 = i - 5000
        total += cast(s64)(signed / 7)
        i += 1
    }
    return total
}
'''.replace('ITERATIONS', str(iterations)), total

    value, total = 1.5, 0
    for i in range(iterations):
        value = value * 0.5 + i % 13
        if value >= 3.0:
            total += int(value)
    yield 'floating', '''#program_export
Answer :: () -> s64 {
    value: float64 = 1.5
    total: s64
    i: s32
    while i < ITERATIONS {
        value = value * 0.5 + cast(float64)(i % 13)
        if value >= 3.0 { total += cast(s64)value }
        i += 1
    }
    return total
}
'''.replace('ITERATIONS', str(iterations)), total

    points = [(i * 17 + 3, i * 29 + 7) for i in range(32)]
    total = 0
    for _ in range(max(1, iterations // 32)):
        for i in range(32):
            left, right = points[i], points[(i + 1) & 31]
            points[i] = ((left[0] + right[1] + i) & 65535, (left[1] ^ right[0]) + i)
            total += sum(points[i])
    yield 'record-calls', '''Point :: struct { x: s64; y: s64; }
Mix :: (left: Point, right: Point, index: s32) -> Point {
    return Point.{(left.x + right.y + index) & 65535, (left.y ^ right.x) + index}
}
#program_export
Answer :: () -> s64 {
    points: [32]Point
    i: s32
    while i < 32 {
        points[i] = Point.{cast(s64)(i * 17 + 3), cast(s64)(i * 29 + 7)}
        i += 1
    }
    total: s64
    round: s32
    while round < ROUNDS {
        i = 0
        while i < 32 {
            points[i] = Mix(points[i], points[(i + 1) & 31], i)
            total += points[i].x + points[i].y
            i += 1
        }
        round += 1
    }
    return total
}
'''.replace('ROUNDS', str(max(1, iterations // 32))), total

    rounds = max(1, iterations // 128)
    yield 'array-returns', '''Make :: (seed: s64) -> [4096]s64 {
    values: [4096]s64
    values[0] = seed
    values[4095] = seed + 1
    return values
}
Relay :: (seed: s64) -> [4096]s64 { return Make(seed) }
#program_export
Answer :: () -> s64 {
    total: s64
    round: s32
    while round < ROUNDS {
        values := Relay(cast(s64)round)
        total += values[0] + values[17] + values[4095]
        round += 1
    }
    return total
}
'''.replace('ROUNDS', str(rounds)), sum(i + i + 1 for i in range(rounds))

    yield 'popcount', cpu_features.source(iterations), cpu_features.oracle(iterations)

    # Byte scans use explicit lengths, including raw UTF-8 and controls.
    # Python scores the encoded bytes independently of either runtime.
    texts = [' \t café 世界\n', 'alpha\\beta"quoted', 'Ω😀1234567890', '']
    rounds = max(1, iterations // 128)
    score = sum(sum(byte * (at + 1) for at, byte in enumerate(text.encode())
                    if at >= start and (at - start) % stride == 0)
                for text in texts for start, stride in [(0, 1), (1, 2)])
    yield 'string-scan', '''Score :: (text: string, start: s32, stride: s32) -> s64 {
    total: s64
    at := start
    while at >= 0 && at < cast(s32)text.count {
        total += cast(s64)text[at] * cast(s64)(at + 1)
        at += stride
    }
    return total
}
#program_export
Answer :: () -> s64 {
    texts: [4]string = .{TEXTS}
    total: s64
    for round: 0..ROUNDS - 1 {
        for text: texts {
            total += Score(text, 0, 1) + Score(text, 1, 2)
        }
    }
    return total
}
'''.replace('TEXTS', ', '.join(json.dumps(text, ensure_ascii=False) for text in texts)).replace(
        'ROUNDS', str(rounds)), score * rounds

    # Exercise the actual standard-library parser, including nested values,
    # raw UTF-8, escaped strings and surrogate pairs. Python independently
    # decodes the serialized documents and supplies the numeric/byte checksum.
    records = [dict(id=1000+i, weight=(-1)**i*(97*i+0.25),
                    label='café 日本語 Ω😀 "row"\\\n'+str(i),
                    meta=dict(active=i % 2 == 0, values=[None, i, 'nested']))
               for i in range(8)]
    documents = [json.dumps(records, ensure_ascii=ascii_only, separators=(',', ':'))
                 for ascii_only in (False, True)]
    rounds = max(1, iterations // 10000)
    checksum = sum(int(row['id']) + int(row['weight']) + sum(row['label'].encode())
                   for document in documents for row in json.loads(document))
    yield 'json-parser', '''#import "json_scan"
#program_export
Answer :: () -> s64 {
    documents: [2]string = .{DOCUMENTS}
    positions: [8]s32
    decoded: [128]u8
    total: s64
    for round: 0..cast(s32)(ROUNDS - 1) {
        for document: documents {
            if ArrayPositions(document, 0, positions[:]) != 8 { return -1 }
            for at: positions {
                id := ReadNumber(document, FieldAt(document, at, "id"))
                weight := ReadNumber(document, FieldAt(document, at, "weight"))
                length := ReadStringText(document, FieldAt(document, at, "label"), decoded[:])
                if !id.valid || !weight.valid || length < 0 { return -2 }
                total += cast(s64)id.value + cast(s64)weight.value
                for byte: decoded[:length] { total += cast(s64)byte }
            }
        }
    }
    return total
}
'''.replace('DOCUMENTS', ', '.join(json.dumps(d, ensure_ascii=False) for d in documents)).replace(
        'ROUNDS', str(rounds)), checksum * rounds


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--before-dir', type=Path, required=True)
    parser.add_argument('--after-dir', type=Path, default=ROOT/'build/bin')
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--iterations', type=int, default=10000)
    parser.add_argument('--repetitions', type=int, default=9)
    parser.add_argument('--runs', type=int, default=9)
    args = parser.parse_args()
    if not 1 <= args.iterations <= 10000 or args.repetitions < 1 or not 1 <= args.runs <= 10000:
        parser.error('iterations and runs must be 1..10000; repetitions must be positive')
    output = args.out.resolve()
    if output.exists():
        parser.error('output exists; preserve receipts and remove inactive outputs before reuse')
    fixtures = output/'fixtures'
    fixtures.mkdir(parents=True)
    before, after = args.before_dir.resolve(), args.after_dir.resolve()
    env = dict(os.environ)
    for key in ['DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'DBUS_SESSION_BUS_ADDRESS',
                'ZIRAN_PROFILE', 'ZIRAN_VM_PROFILE']:
        env.pop(key, None)
    command = [sys.executable, ROOT/'bench/vm_startup.py', '--before-dir', before,
               '--after-dir', after, '--out', output/'comparison',
               '--repetitions', args.repetitions, '--runs', args.runs]
    manifest = {'iterations': args.iterations, 'cases': [], 'source_hashes': {
        name: hashlib.sha256((ROOT/name).read_bytes()).hexdigest()
        for name in ['bench/vm_execution.py', 'bench/cpu_features.py']}}
    for name, source, expected in workloads(args.iterations):
        # Module names use underscores; descriptive case names can use hyphens.
        module = name.replace('-', '_')
        path = fixtures/f'{module}.zi'
        path.write_text(source)
        bundle = fixtures/f'{module}.zib'
        base = ['--root', fixtures, '--module-path', ROOT/'std', '--entry', module+':Answer']
        subprocess.run([str(p) for p in [before/'zi2zib', 'bundle', *base, '-o', bundle, path]],
                       cwd=ROOT, env=env, check=True)
        ir = output/'ir'/module
        subprocess.run([str(p) for p in [after/'zi2zir', '--root', fixtures, '--module-path', ROOT/'std', '-o', ir, path]],
                       cwd=ROOT, env=env, check=True)
        saved = fixtures/f'{module}-saved.zib'
        subprocess.run([str(p) for p in [after/'zi2zib', 'bundle', '--root', ir,
                       '--module-path', ROOT/'std', '--entry', module+':Answer', '-o', saved, ir/f'{module}.zir']],
                       cwd=ROOT, env=env, check=True)
        if saved.read_bytes() != bundle.read_bytes():
            raise AssertionError(f'{name}: source/saved bundle mismatch')
        saved.unlink()
        manifest['cases'].append({'name': name, 'expected': expected,
                                  'source_saved_bundle_identical': True})
        command += ['--case', name, path, bundle, expected]
    (output/'fixtures.json').write_text(json.dumps(manifest, indent=2)+'\n')
    subprocess.run([str(p) for p in command], cwd=ROOT, env=env, check=True)


if __name__ == '__main__':
    main()
