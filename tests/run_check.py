#!/usr/bin/env python3
"""Run independent compiler checks with bounded parallelism.

Every tests/*.sh script is a check. Each check gets its own TMPDIR, so the
scratch directories its mktemp calls create never meet another check's, and
separate checks can safely run together after the toolchain is built. Checks
share the normal Go build cache, so the Go standard library and race runtime
compile once rather than once per check.

A long check declares its rough cost in a header line, `# check-cost: SECONDS`
(its time in the sanitized CI run). Declared checks are spread over the shards
longest first and start before the others, so no shard is left waiting on two
long checks that started late.
"""

import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
import json
import os
import re
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import tempfile
import time


def run_check(name, command, repo, env, log_dir, scratch=None):
    """Run one check, logging its output; with SCRATCH, give it a private
    TMPDIR there that is removed afterwards."""
    started = time.monotonic()
    log = log_dir / (name + ".log")
    private = Path(tempfile.mkdtemp(prefix=name + ".", dir=scratch)) if scratch else None
    if private:
        env = dict(env, TMPDIR=str(private))
    with log.open("w", encoding="utf-8") as output:
        try:
            result = subprocess.run(command, cwd=repo, env=env, stdout=output,
                                    stderr=subprocess.STDOUT, check=False)
            code = result.returncode
        except OSError as error:
            output.write(str(error) + "\n")
            code = 127
    if private:
        shutil.rmtree(private, ignore_errors=True)
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


COST = re.compile(r"^# check-cost: ([0-9]+)$", re.MULTILINE)


def declared_cost(script):
    with Path(script).open(encoding="utf-8", errors="replace") as source:
        match = COST.search(source.read(4096))
    return int(match.group(1)) if match else 0


def plan_checks(checks, costs, part=1, parts=1):
    """Return shard PART of PARTS in start order. Every shard computes the same
    partition: costly checks go to the least-loaded shard, longest first, and
    the rest are dealt in name order."""
    costly = sorted((check for check in checks if costs.get(check[0], 0)),
                    key=lambda check: (-costs[check[0]], check[0]))
    load = [0] * parts
    mine = []
    for check in costly:
        target = min(range(parts), key=lambda index: (load[index], index))
        load[target] += costs[check[0]]
        if target == part - 1:
            mine.append(check)
    rest = [check for check in checks if not costs.get(check[0], 0)]
    return mine + rest[part - 1::parts]


def positive_int(value):
    try:
        number = int(value)
    except ValueError as error:
        raise argparse.ArgumentTypeError("jobs must be a positive integer") from error
    if number < 1:
        raise argparse.ArgumentTypeError("jobs must be at least 1")
    return number


def shard(value):
    try:
        part, parts = (int(number) for number in value.split("/"))
    except ValueError as error:
        raise argparse.ArgumentTypeError("shard must look like 1/2") from error
    if not 1 <= part <= parts:
        raise argparse.ArgumentTypeError("shard must look like 1/2")
    return part, parts


def default_jobs():
    """The build queue's share of the cores, or every core this process may use."""
    queued = os.environ.get("BUILD_QUEUE_JOBS", "")
    if queued.isdigit() and int(queued) > 0:
        return int(queued)
    return len(os.sched_getaffinity(0)) if hasattr(os, "sched_getaffinity") else os.cpu_count() or 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bin-dir", type=Path, default=Path("build/bin"))
    parser.add_argument("--jobs", type=positive_int, default=default_jobs(),
                        help="checks run at once (default: BUILD_QUEUE_JOBS or the CPU count)")
    parser.add_argument("--filter", default="", help="run checks whose name contains this text")
    parser.add_argument("--shard", type=shard, help="run only part N/M of the checks, as CI's parallel jobs do")
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
        "vm_locale.sh": str(bin_dir / "vm-locale-test"),
    }
    checks = [("bundle-link-test", [str(bin_dir / "bundle-link-test")])]
    costs = {}
    for script in sorted((repo / "tests").glob("*.sh")):
        costs[script.name] = declared_cost(script)
        compiler = str(bin_dir / "zi2zir") if script.name == "pointer_member_check.sh" else ziran
        command = ["sh", str(script), compiler]
        if script.name in extra:
            command.append(extra[script.name])
        checks.append((script.name, command))
    checks = [(name, command) for name, command in checks if args.filter in name]
    checks = plan_checks(checks, costs, *(args.shard or (1, 1)))
    if not checks:
        parser.error(f"no checks match {args.filter!r}")
    log_dir = (args.log_dir or bin_dir.parent / "check-logs").resolve()
    log_dir.mkdir(parents=True, exist_ok=True)

    env = os.environ.copy()
    for key in ("DISPLAY", "WAYLAND_DISPLAY", "XAUTHORITY", "DBUS_SESSION_BUS_ADDRESS"):
        env.pop(key, None)
    env["ZIRAN_LIB"] = str(bin_dir.parent / "libziran.a")
    env["ZIRAN_VM_LIB"] = str(bin_dir.parent / "libziran-vm.a")
    # Package builds select cgo/nocgo companions automatically. Preserve the
    # older checks' explicit file lists while applying the same Go constraints.
    go = env.get("ZIRAN_TEST_GO_REAL") or shutil.which("go")
    if go:
        helpers = log_dir / "tools"
        helpers.mkdir(exist_ok=True)
        wrapper = helpers / "go"
        wrapper.write_text("#!/bin/sh\nexec " + shlex.quote(sys.executable) + " " +
                           shlex.quote(str(repo / "tests/go_tool.py")) + " --go " +
                           shlex.quote(go) + ' "$@"\n')
        wrapper.chmod(0o755)
        env["ZIRAN_TEST_GO_REAL"] = go
        env["PATH"] = str(helpers) + os.pathsep + env.get("PATH", "")
        if not env.get("GOCACHE"):
            cache = subprocess.run([go, "env", "GOCACHE"], env=env, capture_output=True, text=True)
            if cache.returncode == 0 and cache.stdout.strip() not in ("", "off"):
                env["GOCACHE"] = cache.stdout.strip()

    print(f"Running {len(checks)} checks with {min(args.jobs, len(checks))} jobs", flush=True)
    results = []
    failures = []
    started = time.monotonic()
    with tempfile.TemporaryDirectory(prefix="ziran-check.") as scratch, \
            ThreadPoolExecutor(max_workers=args.jobs) as pool:
        futures = [pool.submit(run_check, name, command, repo, env, log_dir, scratch)
                   for name, command in checks]
        for future in as_completed(futures):
            result = future.result()
            results.append(result)
            name, code, elapsed = result["name"], result["exit_code"], result["seconds"]
            reason = "" if code == 0 else f", exit {code}" + (" (timeout)" if code == 124 else "")
            print(f"{'PASS' if code == 0 else 'FAIL'} {name} ({elapsed:.1f}s{reason})", flush=True)
            if code:
                failures.append(result)

    # Parallel checks finish in any order; collect every failure's output here.
    for result in sorted(failures, key=lambda result: result["name"]):
        print(f"\n=== FAIL {result['name']} (exit {result['exit_code']})", flush=True)
        output = failure_tail(result["log"])
        if output:
            print(output, flush=True)
        print(f"Full log: {result['log']}", flush=True)
    failed = len(failures)
    elapsed = time.monotonic() - started
    (log_dir / "results.json").write_text(json.dumps(dict(
        total=len(checks), failed=failed, seconds=elapsed,
        checks=sorted(results, key=lambda result: result["name"])), indent=2) + "\n")
    print(f"{len(checks) - failed}/{len(checks)} passed in {elapsed:.1f}s"
          + (": failed " + ", ".join(sorted(result["name"] for result in failures)) if failures else ""),
          flush=True)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
