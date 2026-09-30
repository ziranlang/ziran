#!/usr/bin/env python3
"""Mutation fuzzer for the Ziran frontend.

Seeds are the example programs, standard modules, and the crash corpus in
tests/fuzz. Each run mutates one seed, checks it with the given compiler
(built with sanitizers), and saves every input that crashes, trips a
sanitizer, or hangs. Copy a minimized crash into tests/fuzz so
tests/fuzz_regressions.sh keeps it fixed.
"""

import argparse
import os
from pathlib import Path
import random
import subprocess
import sys
import time

TOKENS = [b"{", b"}", b"(", b")", b"[", b"]", b"::", b":=", b"$T", b"#run",
          b"#if", b"using ", b"*", b"..", b".[", b".{", b"\"", b"#char",
          b"cast(", b"ifx", b"defer", b"for", b"x" * 300, b"\n", b";",
          b"#import \"a\"", b"struct(", b"-> ", b"#string E\n", b"/*", b"//",
          b"#type", b"#foreign", b"if x == {", b"case;", b"#through;"]


def mutate(rng, data):
    data = bytearray(data)
    for _ in range(rng.randint(1, 6)):
        at = rng.randrange(len(data) + 1)
        roll = rng.random()
        if roll < 0.3:
            data[at:at] = rng.choice(TOKENS)
        elif roll < 0.5 and data:
            del data[at:at + rng.randint(1, 20)]
        elif roll < 0.7 and data:
            data[min(at, len(data) - 1)] = rng.randrange(256)
        else:
            source = rng.randrange(len(data) + 1)
            data[at:at] = data[source:source + rng.randint(1, 200)]
    return bytes(data)


def failure(result):
    text = result.stderr.decode("utf-8", "replace")
    for line in text.splitlines():
        if "ERROR: AddressSanitizer" in line:
            return line.split(" on ")[0]
        if "runtime error:" in line:
            return line.split(": runtime error")[0]
    if result.returncode < 0 or result.returncode > 1:
        return f"exit status {result.returncode}"
    return None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", required=True, type=Path, help="sanitized zi2zir")
    parser.add_argument("--seconds", type=float, default=60)
    parser.add_argument("--seed", type=int, default=None)
    parser.add_argument("--out", type=Path, default=Path("build/sanitize/fuzz"))
    args = parser.parse_args()

    repo = Path(__file__).resolve().parent.parent
    seeds = [path.read_bytes() for pattern in ("site/examples/*.zi", "std/*.zi", "tests/fuzz/*.zi")
             for path in sorted(repo.glob(pattern)) if path.stat().st_size < 20000]
    rng = random.Random(args.seed if args.seed is not None else time.time_ns())
    work = args.out / "work"
    work.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ)
    env.pop("DISPLAY", None)
    env.pop("WAYLAND_DISPLAY", None)
    env.setdefault("ASAN_OPTIONS", "detect_leaks=0")

    found = {}
    runs = 0
    end = time.time() + args.seconds
    while time.time() < end:
        data = mutate(rng, rng.choice(seeds))
        (work / "m.zi").write_bytes(data)
        try:
            result = subprocess.run([str(args.compiler), "--check-only", "--root", str(work),
                                     "-o", str(work / "out"), str(work / "m.zi")],
                                    capture_output=True, timeout=10, env=env)
            key = failure(result)
            report = result.stderr
        except subprocess.TimeoutExpired:
            key, report = "timeout", b""
        runs += 1
        if key and key not in found:
            found[key] = args.out / f"crash-{len(found) + 1}.zi"
            found[key].write_bytes(data)
            found[key].with_suffix(".txt").write_bytes(report[:20000])
    print(f"{runs} runs, {len(found)} distinct failures")
    for key, path in found.items():
        print(f"{path}: {key}")
    return 1 if found else 0


if __name__ == "__main__":
    sys.exit(main())
