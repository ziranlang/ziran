#!/usr/bin/env python3
"""Bootstrap fixed-point proof for the Ziran compiler.

Every generated compiler root is listed in bootstrap/manifest.txt in dependency
order. This script proves the checked-in generated C is what the compiler
itself produces, and that the next generation reproduces it byte for byte:

  stage 1  the current compiler regenerates every manifest root;
           compare_stage checks the result against the checked-in seeds.
  stage 2  a compiler built from stage 1 regenerates the same roots;
           compare_stage checks stage 2 equals stage 1 (the fixed point).

check mode performs both comparisons without writing the seeds. update mode
runs the same proof, additionally requires its own `make check` to pass, and
then publishes the stage-1 tree with atomic_update.

The runtime boundary check rejects a bootstrap/runtime C function that
ziran_bootstrap_runtime.h does not declare, so compiler policy cannot move
into the startup surface unnoticed.
"""

from __future__ import annotations

import argparse
import os
import re
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MANIFEST = os.path.join(ROOT, "bootstrap", "manifest.txt")
RUNTIME = os.path.join(ROOT, "bootstrap", "runtime")

# A generated root's command line, mirroring the Makefile's generation rules.
KIND_ARGUMENTS = {
    "compiler": ["--no-main", "--root", "cmd"],
    "platform": ["--no-main", "--root", "cmd", "--module-path", "std"],
    "darwin": ["--no-main", "--target-os", "macos", "--define", "__APPLE__",
               "--define", "__aarch64__", "--root", "cmd"],
    "simd": ["--root", "scripts/simd"],
    "driver": ["--entry", "zi2c:main", "--root", "cmd", "--module-path", "std"],
    "engine": ["--no-main", "--root", "cmd", "--module-path", "std"],
}
COMPILED_KINDS = ("compiler", "platform", "darwin")
IGNORED_SEED_FILES = {".build.lock", ".generated"}


class ManifestError(Exception):
    pass


class FixedPointError(Exception):
    pass


class Entry:
    def __init__(self, name, kind, source, files):
        self.name = name
        self.kind = kind
        self.source = source
        self.files = tuple(files)

    def directory(self, root):
        if self.kind == "darwin":
            return os.path.join(root, "darwin", "compiler_" + self.name)
        if self.kind == "simd":
            return os.path.join(root, "simd")
        if self.kind == "driver":
            return os.path.join(root, "zi2c")
        return os.path.join(root, "compiler_" + self.name)

    def provided(self):
        names = {self.name}
        for file in self.files:
            match = re.fullmatch(r"compiler_([a-z0-9_]+)\.h", file)
            if match:
                names.add(match.group(1))
        return names

    def __repr__(self):
        return f"Entry({self.name!r}, {self.kind!r})"


def _read_text(path):
    with open(path, "r", encoding="utf-8") as handle:
        return handle.read()


def _imports(source):
    return re.findall(r'#import\s+"([^"]+)"', _read_text(source))


def read_manifest(path=MANIFEST):
    """Parse and validate the manifest; return entries in dependency order."""
    entries = []
    seen_keys = set()
    for number, line in enumerate(_read_text(path).splitlines(), 1):
        if not line.strip() or line.startswith("#"):
            continue
        columns = line.split("\t")
        if len(columns) != 4:
            raise ManifestError(f"{path}:{number}: expected name<TAB>kind<TAB>source<TAB>files")
        name, kind, source, files = columns
        if not re.fullmatch(r"[a-z0-9_]+", name):
            raise ManifestError(f"{path}:{number}: bad root name {name!r}")
        if kind not in KIND_ARGUMENTS:
            raise ManifestError(f"{path}:{number}: unknown kind {kind!r}")
        if not files.split():
            raise ManifestError(f"{path}:{number}: {name} lists no generated files")
        for file in files.split():
            if os.path.basename(file) != file:
                raise ManifestError(f"{path}:{number}: {file!r} is not a plain file name")
        if not os.path.isfile(os.path.join(ROOT, source)):
            raise ManifestError(f"{path}:{number}: missing source {source}")
        key = (kind, name)
        if key in seen_keys:
            raise ManifestError(f"{path}:{number}: duplicate {kind} root {name}")
        seen_keys.add(key)
        entries.append(Entry(name, kind, source, files.split()))

    # Every compiler import a root compiles against must be provided by an
    # entry, and dependency order must put the provider first. Roots whose
    # imports come from the assembly set (generated at build time) are exempt.
    providers = {}
    for entry in entries:
        for provided in entry.provided():
            providers.setdefault(provided, entry)
    order = {}
    for index, entry in enumerate(entries):
        if entry.kind in COMPILED_KINDS:
            order.setdefault(entry.name, index)
    for index, entry in enumerate(entries):
        if entry.kind not in COMPILED_KINDS:
            continue
        for imported in _imports(os.path.join(ROOT, entry.source)):
            if not imported.startswith("compiler_"):
                continue
            dependency = imported[len("compiler_"):]
            if dependency in entry.provided():
                continue
            provider = providers.get(dependency)
            if provider is None:
                raise ManifestError(
                    f"{entry.source}: imports {imported}, which no manifest root provides")
            if provider.name == entry.name:
                continue
            if provider.kind not in COMPILED_KINDS:
                raise ManifestError(
                    f"{entry.source}: imports {imported} from an uncompiled root")
            if order.get(provider.name, index) >= index:
                raise ManifestError(
                    f"{entry.source}: imports {imported} before its provider {provider.name}")
    return entries


def generate_stage(compiler, entries, root, kinds=None, environment=None):
    """Regenerate ROOT from ENTRIES with COMPILER; return the entry layout."""
    if environment is None:
        environment = _environment()
    shutil.rmtree(root, ignore_errors=True)
    os.makedirs(root)
    for entry in entries:
        if kinds is not None and entry.kind not in kinds:
            continue
        output = entry.directory(root)
        os.makedirs(output, exist_ok=True)
        command = [compiler] + KIND_ARGUMENTS[entry.kind] + ["-o", output, entry.source]
        result = subprocess.run(command, cwd=ROOT, env=environment,
                                capture_output=True, text=True)
        if result.returncode != 0:
            raise FixedPointError(
                f"generate {entry.name}: {compiler} exited {result.returncode}\n"
                f"{result.stderr.strip()}")
    return root


def compile_stage(stage_root, build_dir, make="make", jobs=None, environment=None):
    """Build the compiler from a generated stage root; return its zi2c path."""
    if environment is None:
        environment = _environment()
    build_dir = os.path.abspath(build_dir)
    command = [make, "-f", os.path.join(ROOT, "Makefile"),
               "BUILD_DIR=" + build_dir,
               "BOOTSTRAP=1",
               "BOOTSTRAP_ROOT=" + os.path.abspath(stage_root)]
    if jobs:
        command.append("JOBS=" + str(jobs))
    command.append(os.path.join(build_dir, "bin", "zi2c"))
    result = subprocess.run(command, cwd=ROOT, env=environment,
                            capture_output=True, text=True)
    if result.returncode != 0:
        raise FixedPointError(
            "compile stage failed: " + " ".join(command) + "\n" + result.stderr.strip())
    compiler = os.path.join(build_dir, "bin", "zi2c")
    if not os.access(compiler, os.X_OK):
        raise FixedPointError(f"compile stage produced no executable {compiler}")
    return compiler


def compare_stage(entries, expected_root, actual_root, extras=False):
    """Byte-compare every manifest file; EXTRAS also rejects stale seed files."""
    problems = []
    for entry in entries:
        expected_dir = entry.directory(expected_root)
        actual_dir = entry.directory(actual_root)
        listed = set(entry.files)
        for file in entry.files:
            expected = os.path.join(expected_dir, file)
            actual = os.path.join(actual_dir, file)
            if not os.path.isfile(actual):
                problems.append(f"{entry.name}: generated file {file} is missing")
                continue
            if not os.path.isfile(expected):
                problems.append(f"{entry.name}: checked-in seed {file} is missing")
                continue
            with open(expected, "rb") as left, open(actual, "rb") as right:
                if left.read() != right.read():
                    problems.append(
                        f"{entry.name}: {file} differs from {expected}")
        if extras and os.path.isdir(expected_dir):
            for file in sorted(os.listdir(expected_dir)):
                if file in listed or file in IGNORED_SEED_FILES:
                    continue
                if file.endswith((".o", ".d")):
                    continue
                problems.append(f"{entry.name}: stale seed file {file} is not in the manifest")
    return problems


def validate_runtime_boundary(runtime_dir=RUNTIME):
    """Every non-static runtime C function must be declared by its header."""
    if not os.path.isdir(runtime_dir):
        return [f"{runtime_dir} is missing"]
    header = os.path.join(runtime_dir, "ziran_bootstrap_runtime.h")
    declared = set()
    if os.path.isfile(header):
        for match in re.finditer(r"\b([A-Za-z_][A-Za-z0-9_]*)\s*\(", _read_text(header)):
            declared.add(match.group(1))
    problems = []
    for file in sorted(os.listdir(runtime_dir)):
        if not file.endswith(".c"):
            continue
        path = os.path.join(runtime_dir, file)
        text = _read_text(path)
        text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
        text = re.sub(r"//[^\n]*", " ", text)
        text = re.sub(r'"(?:\\.|[^"\\])*"', '""', text)
        for match in re.finditer(
                r"(?m)^(?!static\b)(?!\s)(?:[A-Za-z_][A-Za-z0-9_ \t\*]*[ \t\*])"
                r"([A-Za-z_][A-Za-z0-9_]*)[ \t]*\(", text):
            name = match.group(1)
            if name in declared or name in ("if", "while", "for", "switch"):
                continue
            problems.append(
                f"{file}: {name} is not declared by ziran_bootstrap_runtime.h")
    return problems


def atomic_update(entries, stage_root, seed_root):
    """Publish a proved stage tree over the seeds, file by file, fsynced."""
    pending = os.path.abspath(seed_root) + ".next"
    shutil.rmtree(pending, ignore_errors=True)
    os.makedirs(pending)
    try:
        for entry in entries:
            target = entry.directory(pending)
            os.makedirs(target, exist_ok=True)
            source = entry.directory(stage_root)
            for file in entry.files:
                origin = os.path.join(source, file)
                if not os.path.isfile(origin):
                    raise FixedPointError(f"update stage is missing {entry.name}/{file}")
                with open(origin, "rb") as handle:
                    data = handle.read()
                destination = os.path.join(target, file)
                with open(destination, "wb") as handle:
                    handle.write(data)
                    handle.flush()
                    os.fsync(handle.fileno())
        for directory in _seed_directories(entries, seed_root):
            os.makedirs(directory, exist_ok=True)
            os.fsync(_open_directory(directory))
        seed_root_absolute = os.path.abspath(seed_root)
        for entry in entries:
            source = entry.directory(pending)
            target = entry.directory(seed_root)
            os.makedirs(target, exist_ok=True)
            for file in entry.files:
                os.replace(os.path.join(source, file), os.path.join(target, file))
            for file in sorted(os.listdir(target)):
                if file in entry.files or file in IGNORED_SEED_FILES:
                    continue
                if file.endswith((".c", ".h")):
                    os.unlink(os.path.join(target, file))
            os.fsync(_open_directory(target))
        os.fsync(_open_directory(seed_root_absolute))
    finally:
        shutil.rmtree(pending, ignore_errors=True)
    return pending


def _seed_directories(entries, seed_root):
    directories = []
    for entry in entries:
        directory = entry.directory(seed_root)
        if directory not in directories:
            directories.append(directory)
    return directories


def _open_directory(path):
    return os.open(path, os.O_RDONLY)


def _environment():
    environment = dict(os.environ)
    for name in ("DISPLAY", "WAYLAND_DISPLAY", "MAKEFLAGS", "MFLAGS"):
        environment.pop(name, None)
    environment["ZIRAN_DESKTOP_RECOVERY"] = "0"
    environment["HARMONY_DESKTOP_RECOVERY"] = "0"
    environment["YUE_DESKTOP_RECOVERY"] = "0"
    return environment


def _stage_compiler(args, stage_root, name):
    build = os.path.join(args.scratch, name)
    shutil.rmtree(build, ignore_errors=True)
    return compile_stage(stage_root, build, make=args.make, jobs=args.jobs)


def _prove(args, entries, update=False):
    stage1 = os.path.join(args.scratch, "stage1")
    stage2 = os.path.join(args.scratch, "stage2")
    os.makedirs(args.scratch, exist_ok=True)
    generate_stage(args.zi2c, entries, stage1)
    if not update:
        problems = compare_stage(entries, args.seed_root, stage1, extras=True)
        if problems:
            raise FixedPointError(
                "stage 1 does not match the checked-in seeds:\n  " + "\n  ".join(problems))
    compiler = _stage_compiler(args, stage1, "build-stage1")
    generate_stage(compiler, entries, stage2)
    problems = compare_stage(entries, stage1, stage2)
    if problems:
        raise FixedPointError(
            "stage 2 does not reproduce stage 1:\n  " + "\n  ".join(problems))
    if args.runtime:
        problems = validate_runtime_boundary(args.runtime)
        if problems:
            raise FixedPointError(
                "runtime boundary:\n  " + "\n  ".join(problems))
    return stage1


def _command(argv):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("mode", choices=("check", "update"))
    parser.add_argument("--manifest", default=MANIFEST)
    parser.add_argument("--zi2c", default=os.path.join(ROOT, "build", "bin", "zi2c"))
    parser.add_argument("--seed-root", default=os.path.join(ROOT, "bootstrap"))
    parser.add_argument("--scratch", default=os.path.join(ROOT, "build", "bootstrap-fixed"))
    parser.add_argument("--make", default="make")
    parser.add_argument("--jobs", type=int, default=None)
    parser.add_argument("--runtime", default=None,
                        help="validate this runtime boundary directory")
    parser.add_argument("--only", default=None,
                        help="gate these comma-separated roots after the stage-1 "
                             "seed comparison, without the stage-2 build")
    parser.add_argument("--check-command", default=None,
                        help="command that must pass before an update publishes")
    return parser.parse_args(argv)


def main(argv=None):
    args = _command(argv)
    if not os.access(args.zi2c, os.X_OK):
        print(f"bootstrap fixed point: no compiler at {args.zi2c}", file=sys.stderr)
        return 2
    try:
        entries = read_manifest(args.manifest)
        names = {entry.name for entry in entries}
        if args.only:
            selected = [name.strip() for name in args.only.split(",") if name.strip()]
            unknown = [name for name in selected if name not in names]
            if unknown:
                raise ManifestError("unknown roots in --only: " + ", ".join(unknown))
            entries = [entry for entry in entries if entry.name in selected]
        if args.only and args.mode == "check":
            stage1 = os.path.join(args.scratch, "stage1")
            os.makedirs(args.scratch, exist_ok=True)
            generate_stage(args.zi2c, entries, stage1)
            problems = compare_stage(entries, args.seed_root, stage1, extras=True)
            if problems:
                raise FixedPointError(
                    "stage 1 does not match the checked-in seeds:\n  "
                    + "\n  ".join(problems))
            print(f"bootstrap fixed point: {len(entries)} roots match their seeds")
            return 0
        if args.mode == "check":
            _prove(args, entries)
            print(f"bootstrap fixed point: {len(entries)} roots match; "
                  "stage 2 reproduces stage 1")
            return 0
        if args.check_command is None:
            print("bootstrap fixed point: update needs --check-command", file=sys.stderr)
            return 2
        check = subprocess.run(args.check_command, cwd=ROOT, shell=True,
                               env=_environment())
        if check.returncode != 0:
            print(f"bootstrap fixed point: refusing to update, "
                  f"'{args.check_command}' exited {check.returncode}", file=sys.stderr)
            return 1
        stage1 = _prove(args, entries, update=True)
        atomic_update(entries, stage1, args.seed_root)
        problems = compare_stage(entries, args.seed_root, stage1, extras=True)
        if problems:
            raise FixedPointError(
                "published seeds do not match the proof:\n  " + "\n  ".join(problems))
        print(f"bootstrap fixed point: updated {len(entries)} roots; "
              "stage 2 reproduces stage 1")
        return 0
    except (ManifestError, FixedPointError) as problem:
        print(f"bootstrap fixed point: {problem}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
