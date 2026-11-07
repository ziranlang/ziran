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

For local iteration, use --incremental --filter lint_ (or another name).
Reuse is limited to audited compiler-only checks; unknown dependency scopes
always execute. --full/--no-cache executes every selected check. Ordinary
make check and bootstrap runs continue to execute their complete selections.
"""

import argparse
import contextlib
import hashlib
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


# Audited checks use only the compiler, repository fixtures, and the tools
# below. Pin their content: a new shell command or external dependency needs
# a new audit, rather than silently extending the cache's dependency scope.
# Other checks still run normally in incremental mode.
CACHE_SCRIPTS = {
    'builtin_type_names.sh': '55464a5b261592743baeb92b26c826bad95557f4ff6bc03faad4f33cdcc904f9',
    'diagnostic_details.sh': '216ec6569842831ec98e775d0016894ff89865cbd415ab672292b1f740376209',
    'fuzz_regressions.sh': 'aaf7480aa8ecef4a63ab88b899b3cb83c4658e91488eadb28cfe2a8a7c2f3738',
    'lint_casts.sh': '43e3de70f12506f9bfc59476d5040dafcad37021ffe529e8ea5ce5352d4806c1',
    'lint_characters.sh': 'c5437b7cf122e414e41600bc7d0a172564243d3d911e71bc1617023379bcb0d7',
    'lint_flags.sh': 'd08757c1468431ccf6225175016317c822bf4158d04fc68cf0e2f317b1024a58',
    'pointer_to_array.sh': 'ea98f9b14aa37a675275e38086fe0834f4fc0327102b545b1b88cd414fd8d2a0',
    'stored_type_diagnostic.sh': '5a380bca85a16688a194682a6a3276567ee853d6e6a781e6406768bd8fdbc316',
    'structure_first_error.sh': 'f1aa7ffa55018b957154fe051bac3c884edbb1e44c3736789fb5fde2ddb39a95',
    'unterminated_declarations.sh': 'bb9f76a91f2904a9487930d46d34001574d6bed65b6c2570e015bbb7f7492bd9',
}
CACHE_TOOLS = 'sh cat grep sed cmp mktemp rm dirname basename mkdir cp timeout'.split()


def file_hash(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as source:
        for block in iter(lambda: source.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def input_fingerprint(repo, bin_dir, env):
    """Conservative shared inputs, streamed by content, never by timestamps.

    Unknown tool wrappers, external source symlinks, unsupported platforms or
    unreadable inputs disable reuse. These local checks don't invoke C/Go
    compilers or fetch packages; checks that do are deliberately not cached.
    """
    try:
        if not sys.platform.startswith('linux'):
            return None
        if bin_dir != repo / 'build/bin' or any((bin_dir.parent / name).exists()
                                               for name in ('std', 'include')):
            return None  # Installed/alternate toolchains may find external sources.
        if any(env.get(key) for key in ('ZIRAN_STD', 'ZIRAN_INCLUDE', 'ZIRAN_PACKAGE_MAP',
                                       'LD_PRELOAD', 'LD_AUDIT', 'LOCPATH',
                                       'ASAN_OPTIONS', 'LSAN_OPTIONS', 'UBSAN_OPTIONS')):
            return None  # These can name dependency files outside this checkout.
        files = subprocess.run(['git', 'ls-files', '-z', '--cached', '--others',
                                '--exclude-standard'], cwd=repo, check=True,
                               capture_output=True).stdout.split(b'\0')
        paths = {repo / os.fsdecode(name) for name in files if name}
        # Include ignored fixtures too. Never hash this runner's logs/scratch.
        for directory in ('tests', 'std', 'include', 'templates'):
            paths.update(p for p in (repo / directory).rglob('*')
                         if '__pycache__' not in p.parts and (p.is_symlink() or not p.is_dir()))
        paths.update(p for p in bin_dir.iterdir() if p.is_file())
        paths.update(bin_dir.parent.glob('*.a'))
        tools = [shutil.which(name, path=env.get('PATH')) for name in CACHE_TOOLS]
        if not all(tools):
            return None
        for tool in [*tools, *map(str, bin_dir.iterdir())]:
            path = Path(tool)
            if not path.is_file():
                continue
            with path.open('rb') as source:
                if source.read(4) != b'\x7fELF':
                    return None
            paths.add(path.resolve())
            linked = subprocess.run(['ldd', str(path)], env=env, capture_output=True,
                                    text=True, check=False)
            if linked.returncode and 'not a dynamic executable' not in linked.stderr + linked.stdout:
                return None
            if 'not found' in linked.stdout:
                return None
            paths.update(Path(name).resolve() for name in re.findall(r'(/[^\s()]+)', linked.stdout))
        semantic_env = dict(env)
        # Logging and queue lease identities change for each managed invocation.
        # The audited scripts/tools don't use either value as a test input.
        for key in ('INVOCATION_ID', 'HARMONY_BUILD_OWNER'):
            semantic_env.pop(key, None)
        digest = hashlib.sha256(json.dumps(semantic_env, sort_keys=True).encode())
        digest.update(str(os.uname()).encode())
        digest.update(str(repo).encode())
        for path in sorted(paths):
            # Missing tracked files are an input too, including deletions.
            digest.update(str(path).encode() + b'\0')
            if path.is_symlink():
                return None
            if path.exists():
                digest.update(str(path.stat().st_mode).encode())
                digest.update(file_hash(path).encode())
            else:
                digest.update(b'missing')
        return digest.hexdigest()
    except (OSError, subprocess.SubprocessError):
        return None


def cache_key(name, command, repo, fingerprint):
    script = repo / 'tests' / name
    if not fingerprint or name not in CACHE_SCRIPTS or not script.is_file():
        return None
    if file_hash(script) != CACHE_SCRIPTS[name]:
        return None
    return hashlib.sha256(json.dumps([fingerprint, command]).encode()).hexdigest()


def read_cache(path):
    try:
        data = json.loads(path.read_text())
        return data if isinstance(data, dict) else {}
    except (OSError, ValueError):
        return {}


def write_cache(path, entries):
    # Atomic replacement prevents a killed runner from leaving a partial cache.
    with tempfile.NamedTemporaryFile(mode='w', dir=path.parent, delete=False) as output:
        temporary = Path(output.name)
        json.dump(entries, output)
    temporary.replace(path)


@contextlib.contextmanager
def cache_lock(log_dir, incremental):
    """Serialize cache users and subsequent full runs sharing their logs."""
    lock_path = log_dir / 'incremental.lock'
    if sys.platform.startswith('linux') and (incremental or lock_path.exists()):
        import fcntl
        with lock_path.open('a') as lock:
            fcntl.flock(lock, fcntl.LOCK_EX)
            yield
    else:
        yield


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
    parser.add_argument("--incremental", action="store_true",
                        help="reuse unchanged successful audited local checks; combine with --filter; "
                             "checks with unknown dependencies still execute")
    parser.add_argument("--no-cache", "--full", action="store_true",
                        help="execute every selected check even with --incremental (default behavior)")
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

    with cache_lock(log_dir, args.incremental):
        return execute_checks(args, repo, bin_dir, checks, log_dir)


def execute_checks(args, repo, bin_dir, checks, log_dir):

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

    cache_path = log_dir / 'incremental.json'
    incremental = args.incremental and not args.no_cache
    entries = read_cache(cache_path) if incremental else {}
    fingerprint = input_fingerprint(repo, bin_dir, env) if incremental else None
    keys = {name: cache_key(name, command, repo, fingerprint)
            for name, command in checks} if incremental else {}
    pending = []
    results = []
    for name, command in checks:
        entry = entries.pop(name, None)
        if keys.get(name) and entry == keys[name]:
            results.append(dict(name=name, exit_code=0, seconds=0, reused=True))
            print(f"REUSE {name}", flush=True)
        else:
            pending.append((name, command))
    # Invalidate selected successes before executing, including interrupted runs.
    # Full/default runs bypass and discard the cache so a later failure cannot
    # leave a previous success reusable.
    if incremental or cache_path.exists():
        write_cache(cache_path, entries)
    print(f"Running {len(pending)} checks with {min(args.jobs, len(pending))} jobs "
          f"({len(results)} reused)", flush=True)
    failures = []
    started = time.monotonic()
    with tempfile.TemporaryDirectory(prefix="ziran-check.", dir=log_dir) as scratch, \
            ThreadPoolExecutor(max_workers=args.jobs) as pool:
        futures = [pool.submit(run_check, name, command, repo, env, log_dir, scratch)
                   for name, command in pending]
        for future in as_completed(futures):
            result = future.result()
            result['reused'] = False
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
    # Edits/builds during a run must not certify a mixed input snapshot.
    if incremental and fingerprint and input_fingerprint(repo, bin_dir, env) == fingerprint:
        for result in results:
            key = keys.get(result['name'])
            if result['exit_code'] == 0 and key:
                entries[result['name']] = key
        write_cache(cache_path, entries)
    reused = sum(result['reused'] for result in results)
    (log_dir / "results.json").write_text(json.dumps(dict(
        total=len(checks), failed=failed, seconds=elapsed, executed=len(pending), reused=reused,
        checks=sorted(results, key=lambda result: result["name"])), indent=2) + "\n")
    print(f"{len(checks) - failed}/{len(checks)} passed in {elapsed:.1f}s "
          f"({len(pending)} executed, {reused} reused)"
          + (": failed " + ", ".join(sorted(result["name"] for result in failures)) if failures else ""),
          flush=True)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
