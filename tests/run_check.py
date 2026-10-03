#!/usr/bin/env python3
"""Run independent compiler checks with bounded parallelism.

Every tests/*.sh script is a check. Each script uses its own mktemp directory,
so separate scripts can safely run together after the toolchain is built.
"""

import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
import os
from pathlib import Path
import subprocess
import sys
import time


def positive_int(value):
    try:
        number = int(value)
    except ValueError as error:
        raise argparse.ArgumentTypeError("jobs must be a positive integer") from error
    if number < 1:
        raise argparse.ArgumentTypeError("jobs must be at least 1")
    return number


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bin-dir", type=Path, default=Path("build/bin"))
    parser.add_argument("--jobs", type=positive_int, default=4)
    parser.add_argument("--filter", default="", help="run checks whose name contains this text")
    args = parser.parse_args()

    repo = Path(__file__).resolve().parent.parent
    bin_dir = args.bin_dir.resolve()
    ziran = str(bin_dir / "ziran")
    extra = {
        "host_capability.sh": str(bin_dir / "host-capability-test"),
        "host_slices.sh": str(bin_dir / "slice-host-test"),
        "host_arrays.sh": str(bin_dir / "array-host-test"),
        "portable_host_records.sh": str(bin_dir / "record-host-test"),
        "process.sh": str(bin_dir / "process-host-test"),
        "vm_vec_scope.sh": str(bin_dir / "vm-vec-scope-test"),
    }
    checks = [("bundle-link-test", [str(bin_dir / "bundle-link-test")])]
    for script in sorted((repo / "tests").glob("*.sh")):
        # This is an explicit QEMU/8c hardware runner, taking a fixture name
        # rather than a compiler. plan9_target.sh covers generated Plan 9 code.
        if script.name == "plan9_native_test.sh":
            continue
        compiler = str(bin_dir / "zi2zir") if script.name == "pointer_member_check.sh" else ziran
        command = ["sh", str(script), compiler]
        if script.name in extra:
            command.append(extra[script.name])
        checks.append((script.name, command))
    checks = [(name, command) for name, command in checks if args.filter in name]
    if not checks:
        parser.error(f"no checks match {args.filter!r}")

    env = os.environ.copy()
    env.pop("DISPLAY", None)
    env.pop("WAYLAND_DISPLAY", None)
    env["ZIRAN_LIB"] = str(bin_dir.parent / "libziran.a")

    def run(name, command):
        started = time.monotonic()
        result = subprocess.run(command, cwd=repo, env=env, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, text=True, check=False)
        return name, result.returncode, time.monotonic() - started, result.stdout

    print(f"Running {len(checks)} checks with {min(args.jobs, len(checks))} jobs", flush=True)
    failed = 0
    started = time.monotonic()
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        futures = [pool.submit(run, name, command) for name, command in checks]
        for future in as_completed(futures):
            name, code, elapsed, output = future.result()
            print(f"{'PASS' if code == 0 else 'FAIL'} {name} ({elapsed:.1f}s)", flush=True)
            if code:
                failed += 1
                print(output, end="" if output.endswith("\n") else "\n", flush=True)

    print(f"{len(checks) - failed}/{len(checks)} passed in {time.monotonic() - started:.1f}s",
          flush=True)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
