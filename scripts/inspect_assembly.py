#!/usr/bin/env python3
"""Build a stable, identity-checked direct-assembly inspection artifact set."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
from typing import Any


REPOSITORY = Path(__file__).resolve().parents[1]
OUTPUT = REPOSITORY / "build" / "vscode-assembly" / "inspection"
STAGING = OUTPUT.parent / ".inspection.next"
PREVIOUS = OUTPUT.parent / ".inspection.previous"
DISPLAY_ENVIRONMENT = (
    "DISPLAY",
    "WAYLAND_DISPLAY",
    "XAUTHORITY",
    "DBUS_SESSION_BUS_ADDRESS",
)


class InspectionError(RuntimeError):
    def __init__(self, stage: str, detail: str):
        super().__init__(detail)
        self.stage = stage
        self.detail = detail


def digest(path: Path) -> str:
    value = hashlib.sha256()
    with path.open("rb") as handle:
        while block := handle.read(1024 * 1024):
            value.update(block)
    return value.hexdigest()


def clean_environment() -> dict[str, str]:
    environment = os.environ.copy()
    for name in DISPLAY_ENVIRONMENT:
        environment.pop(name, None)
    environment["HARMONY_DESKTOP_RECOVERY"] = "0"
    return environment


def display_path(path: Path) -> str:
    try:
        return path.relative_to(REPOSITORY).as_posix()
    except ValueError:
        return str(path)


def run(
    stage: str,
    command: list[str],
    *,
    cwd: Path = REPOSITORY,
    return_stdout: bool = False,
) -> str:
    print(f"[{stage}] {shlex.join(command)}", flush=True)
    try:
        result = subprocess.run(
            command,
            cwd=cwd,
            env=clean_environment(),
            capture_output=True,
            text=True,
            errors="replace",
            check=False,
        )
    except OSError as error:
        raise InspectionError(stage, f"{shlex.join(command)}: {error}") from error
    if result.returncode != 0:
        output = (result.stdout + result.stderr).strip()
        suffix = f"\n{output}" if output else ""
        raise InspectionError(
            stage,
            f"command exited with status {result.returncode}: "
            f"{shlex.join(command)}{suffix}",
        )
    if not return_stdout:
        if result.stdout:
            print(result.stdout, end="")
        if result.stderr:
            print(result.stderr, end="", file=sys.stderr)
    return result.stdout


def remove_transient(path: Path) -> None:
    if path.is_symlink() or path.is_file():
        path.unlink()
    elif path.exists():
        shutil.rmtree(path)


def recover_interrupted_publish() -> None:
    if PREVIOUS.exists() and not OUTPUT.exists():
        PREVIOUS.replace(OUTPUT)
    elif PREVIOUS.exists():
        remove_transient(PREVIOUS)


def repository_path(value: str, label: str) -> Path:
    path = Path(value)
    if not path.is_absolute():
        path = REPOSITORY / path
    path = path.resolve()
    if not path.is_relative_to(REPOSITORY):
        raise InspectionError("arguments", f"{label} must be inside {REPOSITORY}: {path}")
    return path


def checked_entry(checked: Path, entry: str, source: Path) -> Path:
    files = sorted(checked.rglob("*.zir"))
    if not files:
        raise InspectionError("ZIR", "zi2zir emitted no .zir files")

    module = entry.split(":", 1)[0]
    module_paths = {module, module.replace(".", "/")}
    module_leaf = module.replace(".", "/").rsplit("/", 1)[-1]
    matches = []
    for path in files:
        relative = path.relative_to(checked).with_suffix("").as_posix()
        if relative in module_paths or path.stem in {module_leaf, source.stem}:
            matches.append(path)
    matches = sorted(set(matches))
    if len(matches) == 1:
        return matches[0]
    if len(files) == 1:
        return files[0]
    emitted = ", ".join(display_path(path) for path in files)
    raise InspectionError(
        "ZIR",
        f"cannot select checked module for {module!r}; emitted: {emitted}",
    )


def require_file(stage: str, path: Path, description: str) -> None:
    if not path.is_file() or path.stat().st_size == 0:
        raise InspectionError(stage, f"{description} is missing or empty: {path}")


def load_json(stage: str, path: Path) -> dict[str, Any]:
    require_file(stage, path, "JSON artifact")
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise InspectionError(stage, f"invalid JSON in {path}: {error}") from error
    if not isinstance(value, dict):
        raise InspectionError(stage, f"expected a JSON object in {path}")
    return value


def publish() -> None:
    if PREVIOUS.exists():
        remove_transient(PREVIOUS)
    if OUTPUT.exists():
        OUTPUT.replace(PREVIOUS)
    try:
        STAGING.replace(OUTPUT)
    except OSError:
        if PREVIOUS.exists() and not OUTPUT.exists():
            PREVIOUS.replace(OUTPUT)
        raise
    if PREVIOUS.exists():
        remove_transient(PREVIOUS)


def inspect(source: Path, entry: str, root: Path, objdump_name: str) -> None:
    zi2zir = REPOSITORY / "build" / "bin" / "zi2zir"
    zi2asm = REPOSITORY / "build" / "bin" / "zi2asm"
    for tool in (zi2zir, zi2asm):
        if not tool.is_file() or not os.access(tool, os.X_OK):
            raise InspectionError(
                "tool check",
                f"required existing compiler is unavailable: {tool}. "
                "This inspection does not rebuild or fall back to another compiler.",
            )

    objdump = shutil.which(objdump_name)
    if objdump is None:
        raise InspectionError(
            "disassembly",
            f"inspection dependency {objdump_name!r} was not found. zi2asm encodes "
            "the ELF itself; objdump is required only to render final disassembly.",
        )
    objdump_path = Path(objdump).resolve()

    recover_interrupted_publish()
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    remove_transient(STAGING)
    STAGING.mkdir()

    checked = STAGING / "checked"
    checked.mkdir()
    checked_command = [
        str(zi2zir),
        "--root",
        str(root),
        "--entry",
        entry,
        "-o",
        str(checked),
        str(source),
    ]
    run("ZIR", checked_command)
    checked_input = checked_entry(checked, entry, source)
    require_file("ZIR", checked_input, "selected .zir")

    zx = STAGING / "lowered.zx"
    lower_command = [
        str(zi2asm),
        "--root",
        str(checked),
        "--entry",
        entry,
        "--emit",
        "zx",
        "-o",
        str(zx),
        str(checked_input),
    ]
    run("portable ZX", lower_command)
    require_file("portable ZX", zx, "portable .zx")

    lowered = STAGING / "lowered.lir"
    lowered_command = [
        str(zi2asm),
        "--root",
        str(checked),
        "--entry",
        entry,
        "--emit",
        "lir",
        "-o",
        str(lowered),
        str(checked_input),
    ]
    run("lowered instruction listing", lowered_command)
    require_file("lowered instruction listing", lowered, "lowered listing")

    optimized = STAGING / "optimized.lir"
    optimized_command = [
        str(zi2asm),
        "--input",
        "zx",
        "--opt",
        "--emit",
        "lir",
        "-o",
        str(optimized),
        str(zx),
    ]
    run("optimized instruction listing", optimized_command)
    require_file("optimized instruction listing", optimized, "optimized listing")

    optimization = STAGING / "optimization.json"
    optimization_command = [
        str(zi2asm),
        "--root",
        str(checked),
        "--entry",
        entry,
        "--opt",
        "--emit",
        "opt-report",
        "-o",
        str(optimization),
        str(checked_input),
    ]
    run("optimization counters", optimization_command)
    load_json("optimization counters", optimization)

    final_map = STAGING / "final.map.json"
    map_command = [
        str(zi2asm),
        "--input",
        "zx",
        "--opt",
        "--emit",
        "map",
        "-o",
        str(final_map),
        str(zx),
    ]
    run("final native map", map_command)

    final_elf = STAGING / "final.elf"
    elf_command = [
        str(zi2asm),
        "--input",
        "zx",
        "--opt",
        "--emit",
        "elf",
        "-o",
        str(final_elf),
        str(zx),
    ]
    run("final ELF", elf_command)
    require_file("final ELF", final_elf, "final ELF")
    with final_elf.open("rb") as handle:
        magic = handle.read(4)
    if magic != b"\x7fELF":
        raise InspectionError("final ELF", f"invalid ELF magic in {final_elf}")

    map_data = load_json("map/ELF identity", final_map)
    identity = map_data.get("identity")
    if not isinstance(identity, dict):
        raise InspectionError("map/ELF identity", "map has no identity object")
    elf_digest = digest(final_elf)
    if (
        identity.get("algorithm") != "sha256"
        or identity.get("scope") != "complete-elf"
        or identity.get("digest") != elf_digest
    ):
        raise InspectionError(
            "map/ELF identity",
            f"map identity does not match final ELF SHA-256 {elf_digest}",
        )
    if map_data.get("optimization_request") == "none":
        raise InspectionError("map/ELF identity", "final map does not identify optimized output")
    image = map_data.get("image")
    if not isinstance(image, dict):
        raise InspectionError("map/ELF identity", "map has no image object")
    text_address = image.get("text_virtual_address")
    if not isinstance(text_address, int) or text_address < 0:
        raise InspectionError(
            "map/ELF identity",
            "map image.text_virtual_address is missing or invalid",
        )

    adjustment = f"-0x{text_address:x}"
    disassembly_command = [
        str(objdump_path),
        "-d",
        f"--adjust-vma={adjustment}",
        "final.elf",
    ]
    disassembly_body = run(
        "final disassembly",
        disassembly_command,
        cwd=STAGING,
        return_stdout=True,
    )
    if not disassembly_body.strip():
        raise InspectionError("final disassembly", "objdump produced no disassembly")
    disassembly = STAGING / "final.disasm"
    disassembly.write_text(
        "\n".join(
            [
                "# Ziran direct-assembly final disassembly",
                f"# complete ELF SHA-256: {elf_digest}",
                "# map identity: sha256 complete-elf " f"{identity['digest']}",
                "# image.text_virtual_address: " f"0x{text_address:x} ({text_address})",
                f"# objdump address adjustment: {adjustment}",
                f"# command: {shlex.join(disassembly_command)}",
                "",
                disassembly_body,
            ]
        ),
        encoding="utf-8",
    )

    artifact_paths = [
        *sorted(checked.rglob("*.zir")),
        zx,
        lowered,
        optimized,
        optimization,
        final_map,
        final_elf,
        disassembly,
    ]
    artifacts = {
        path.relative_to(STAGING).as_posix(): {
            "sha256": digest(path),
            "bytes": path.stat().st_size,
        }
        for path in artifact_paths
    }
    manifest = {
        "format_version": 1,
        "source": {
            "path": source.relative_to(REPOSITORY).as_posix(),
            "sha256": digest(source),
            "root": root.relative_to(REPOSITORY).as_posix(),
            "entry": entry,
        },
        "tools": {
            "zi2zir": {
                "path": zi2zir.relative_to(REPOSITORY).as_posix(),
                "sha256": digest(zi2zir),
            },
            "zi2asm": {
                "path": zi2asm.relative_to(REPOSITORY).as_posix(),
                "sha256": digest(zi2asm),
            },
            "objdump": {"path": str(objdump_path)},
        },
        "commands": {
            "checked": checked_command,
            "lowered_binary": lower_command,
            "lowered_listing": lowered_command,
            "optimized_listing": optimized_command,
            "optimization_counters": optimization_command,
            "final_map": map_command,
            "final_elf": elf_command,
            "final_disassembly": disassembly_command,
        },
        "map_identity": identity,
        "image": image,
        "objdump_address_adjustment": adjustment,
        "artifacts": artifacts,
    }
    manifest_path = STAGING / "manifest.json"
    manifest_path.write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    load_json("manifest", manifest_path)
    publish()

    print(f"Assembly inspection complete: {display_path(OUTPUT)}")
    print(f"ELF identity: sha256:{elf_digest}")
    print(
        "Text-relative disassembly adjustment: "
        f"{adjustment} from image.text_virtual_address"
    )


def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Inspect Ziran's experimental direct backend without rebuilding or "
            "falling back to another compiler."
        )
    )
    parser.add_argument(
        "--source",
        default="tests/spec/assembly_scalar.zi",
        help="source file inside the repository",
    )
    parser.add_argument(
        "--entry",
        default="assembly_scalar:Answer",
        help="entry in MODULE:FUNCTION form",
    )
    parser.add_argument(
        "--root",
        help="module root inside the repository (default: source directory)",
    )
    parser.add_argument(
        "--objdump",
        default="objdump",
        help="objdump-compatible disassembler used only for inspection",
    )
    arguments = parser.parse_args()

    try:
        if arguments.entry.count(":") != 1 or not all(arguments.entry.split(":")):
            raise InspectionError(
                "arguments",
                f"entry must use nonempty MODULE:FUNCTION form: {arguments.entry!r}",
            )
        source = repository_path(arguments.source, "source")
        if not source.is_file():
            raise InspectionError("arguments", f"source does not exist: {source}")
        root = (
            repository_path(arguments.root, "root")
            if arguments.root
            else source.parent
        )
        if not root.is_dir():
            raise InspectionError("arguments", f"root is not a directory: {root}")
        inspect(source, arguments.entry, root, arguments.objdump)
    except InspectionError as error:
        print(
            f"assembly inspection failed during {error.stage}: {error.detail}",
            file=sys.stderr,
        )
    except (OSError, UnicodeError) as error:
        print(f"assembly inspection failed during filesystem I/O: {error}", file=sys.stderr)
    else:
        return 0

    if OUTPUT.is_dir():
        print(
            f"Last complete artifacts remain at {display_path(OUTPUT)}.",
            file=sys.stderr,
        )
    else:
        print("No previous complete artifact set is available.", file=sys.stderr)
    if STAGING.is_dir():
        print(
            f"Incomplete staging is at {display_path(STAGING)}.",
            file=sys.stderr,
        )
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
