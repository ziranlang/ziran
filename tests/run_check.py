#!/usr/bin/env python3
"""Run independent compiler checks with bounded parallelism.

Every tests/*.sh script is a check. Each script uses its own mktemp directory,
so separate scripts can safely run together after the toolchain is built.
"""

import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
import json
import os
from pathlib import Path
import subprocess
import sys
import time


def run_check(name, command, repo, env, log_dir):
    started = time.monotonic()
    log = log_dir / (name + ".log")
    with log.open("w", encoding="utf-8") as output:
        try:
            result = subprocess.run(command, cwd=repo, env=env, stdout=output,
                                    stderr=subprocess.STDOUT, check=False)
            code = result.returncode
        except OSError as error:
            output.write(str(error) + "\n")
            code = 127
    return dict(name=name, exit_code=code, seconds=time.monotonic() - started,
                log=str(log))


def failure_tail(log):
    # Linker failures can produce tens of megabytes. Keep the complete output
    # on disk and bound both memory use and the CI console's failure excerpt.
    with Path(log).open("rb") as source:
        size = source.seek(0, os.SEEK_END)
        source.seek(max(0, size - 16384))
        lines = source.read().decode("utf-8", errors="replace").splitlines()
    if size > 16384:
        lines = lines[1:]
    return "\n".join(lines[-60:])


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
    parser.add_argument("--log-dir", type=Path, help="save full logs and results (default: BUILD/check-logs)")
    args = parser.parse_args()

    repo = Path(__file__).resolve().parent.parent
    bin_dir = args.bin_dir.resolve()
    ziran = str(bin_dir / "ziran")
    extra = {
        "host_capability.sh": str(bin_dir / "host-capability-test"),
        "host_slices.sh": str(bin_dir / "slice-host-test"),
        "host_arrays.sh": str(bin_dir / "array-host-test"),
        "portable_host_records.sh": str(bin_dir / "record-host-test"),
        "record_fields_by_name.sh": str(bin_dir / "record-fields-by-name-test"),
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
    log_dir = (args.log_dir or bin_dir.parent / "check-logs").resolve()
    log_dir.mkdir(parents=True, exist_ok=True)

    env = os.environ.copy()
    for key in ("DISPLAY", "WAYLAND_DISPLAY", "XAUTHORITY", "DBUS_SESSION_BUS_ADDRESS"):
        env.pop(key, None)
    env["ZIRAN_LIB"] = str(bin_dir.parent / "libziran.a")

    print(f"Running {len(checks)} checks with {min(args.jobs, len(checks))} jobs", flush=True)
    failed = 0
    results = []
    started = time.monotonic()
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        futures = [pool.submit(run_check, name, command, repo, env, log_dir) for name, command in checks]
        for future in as_completed(futures):
            result = future.result()
            results.append(result)
            name, code, elapsed = result["name"], result["exit_code"], result["seconds"]
            reason = "" if code == 0 else f", exit {code}" + (" (timeout)" if code == 124 else "")
            print(f"{'PASS' if code == 0 else 'FAIL'} {name} ({elapsed:.1f}s{reason})", flush=True)
            if code:
                failed += 1
                output = failure_tail(result["log"])
                if output:
                    print(output, flush=True)
                print(f"Full log: {result['log']}", flush=True)

    elapsed = time.monotonic() - started
    (log_dir / "results.json").write_text(json.dumps(dict(
        total=len(checks), failed=failed, seconds=elapsed,
        checks=sorted(results, key=lambda result: result["name"])), indent=2) + "\n")
    print(f"{len(checks) - failed}/{len(checks)} passed in {elapsed:.1f}s",
          flush=True)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
