#!/usr/bin/env python3
"""Execute one package's conformance program on every native language backend."""

import argparse
import os
from pathlib import Path
import shlex
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ziran", default="ziran")
    parser.add_argument("--tools", type=Path, help="use an already built local toolchain")
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--entry", required=True)
    parser.add_argument("--library-dir", required=True, type=Path)
    parser.add_argument("--libraries", required=True)
    parser.add_argument("--target", choices=("all", "c", "cpp", "go", "rust", "py"), default="all")
    parser.add_argument("--build-dir", type=Path, default=Path("build/backends"))
    args = parser.parse_args()
    env = os.environ.copy()
    env.pop("DISPLAY", None)
    env.pop("WAYLAND_DISPLAY", None)
    locked = [] if Path("ziran.local.toml").exists() else ["--locked"]
    ziran = shlex.split(args.ziran)
    toolchain = Path(subprocess.check_output(ziran + ["pkg", "path", "ziran"], env=env, text=True).strip())
    tools = args.tools.resolve() if args.tools else toolchain / "build/bin"
    launcher = str(tools / "ziran")
    library_dir = args.library_dir.resolve()
    env["LDFLAGS"] = f"-L{library_dir} " + env.get("LDFLAGS", "")
    env["LDLIBS"] = args.libraries
    env["LD_LIBRARY_PATH"] = str(library_dir) + ":" + env.get("LD_LIBRARY_PATH", "")
    env["GO111MODULE"] = "off"
    env["CGO_ENABLED"] = "1"
    targets = ("c", "cpp", "go", "rust", "py") if args.target == "all" else (args.target,)
    build = args.build_dir.resolve()
    build.mkdir(parents=True, exist_ok=True)

    def run(command, **kwargs):
        subprocess.run(command, env=env, check=True, timeout=300, **kwargs)

    # Saved IR must preserve the foreign ABI and the whole imported graph.
    ir = build / "ir"
    # This directory holds only this runner's generated IR.
    if ir.exists():
        for path in ir.rglob("*.zir"):
            path.unlink()
    if args.tools:
        source_options = ["--root", "src", "--module-path", "tests"]
        source_launcher = [launcher]
    else:
        source_options = ["--project"] + locked
        source_launcher = ziran
    run(source_launcher + ["check"] + source_options + [str(args.source)])
    run(source_launcher + ["ir"] + source_options + ["--entry", args.entry, "-o", str(ir), str(args.source)])
    saved = sorted(str(path) for path in ir.rglob("*.zir"))
    if not saved:
        raise RuntimeError("no saved modules were emitted")
    for target in targets:
        for kind in ("source", "saved"):
            out = build / target / kind
            out.mkdir(parents=True, exist_ok=True)
            command = (source_launcher + ["build"] + source_options + [str(args.source)]
                       if kind == "source" else
                       [launcher, "build", "--root", str(ir)] + saved)
            # Options precede source inputs for the standalone backends.
            inputs = [str(args.source)] if kind == "source" else saved
            command = command[:-len(inputs)]
            command += [f"--target={target}", "--entry", args.entry, "-o", str(out)]
            if target == "go": command += ["--pkg", "main", "--exe"]
            if target in ("rust", "py"): command += ["--exe"]
            run(command + inputs)
            program = out / "conformance"
            if target in ("c", "cpp"):
                compiler = shlex.split(env.get("CC" if target == "c" else "CXX", "cc" if target == "c" else "c++"))
                suffix = "c" if target == "c" else "cpp"
                run(compiler + ["-O2", "-std=c99" if target == "c" else "-std=c++17",
                    "-I" + str(toolchain / "include"), "-I" + str(out)] +
                    sorted(str(path) for path in out.glob(f"*.{suffix}")) +
                    shlex.split(env["LDFLAGS"]) + shlex.split(env["LDLIBS"]) + ["-o", str(program)])
                run([str(program)])
            elif target == "go":
                run(["go", "build", "-o", str(program), "."], cwd=out)
                run([str(program)])
            elif target == "rust":
                cargo_target = out / "target"
                run(["cargo", "build", "--offline", "--quiet", "--manifest-path", str(out / "Cargo.toml"),
                     "--target-dir", str(cargo_target)])
                run([str(cargo_target / "debug/ziran_generated")])
            else:
                run([sys.executable, str(out)])
            print(f"PASS {target} ({kind})", flush=True)


if __name__ == "__main__":
    main()
