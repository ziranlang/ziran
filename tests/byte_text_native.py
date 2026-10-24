#!/usr/bin/env python3
"""Run borrowed native strings through both 64-bit and 32-bit String ABIs."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BIN = Path(os.environ.get("ZIRAN_BIN_DIR", ROOT / "build/bin"))
EMCC = Path(os.environ.get("EMCC", Path.home() / "emsdk/upstream/emscripten/emcc"))
ENV = dict(os.environ)
for key in ("DISPLAY", "WAYLAND_DISPLAY", "XAUTHORITY", "GDK_DISPLAY"):
    ENV.pop(key, None)


def run(command):
    subprocess.run(list(map(str, command)), env=ENV, check=True)


with tempfile.TemporaryDirectory(prefix="ziran-byte-text-") as directory:
    work = Path(directory)
    source = ROOT / "tests/byte_text_native.zi"
    run([BIN / "zi2zir", "--root", ROOT / "tests", "--module-path", ROOT / "std",
         "-o", work / "ir", source])
    for mode, entry, root in (("source", source, ROOT / "tests"),
                              ("saved", work / "ir/byte_text_native.zir", work / "ir")):
        for target, compiler, suffix in (("c", os.environ.get("CC", "cc"), "c"),):
            output = work / (mode + "-" + target)
            run([BIN / ("zi2" + target), "--no-main", "--root", root,
                 "--module-path", ROOT / "std", "-o", output, entry])
            files = sorted(output.rglob("*." + suffix))
            includes = ["-I" + str(ROOT / "include")]
            includes += ["-iquote" + str(p) for p in sorted({p.parent for p in output.rglob("*.h*")})]
            run([compiler, "-O1", "-DZIRAN_BOUNDS_CHECK", *includes, *files,
                 "-o", output / "native"])
            run([output / "native"])
            run([EMCC, "-O1", "-DZIRAN_BOUNDS_CHECK", *includes, *files,
                 "-sENVIRONMENT=node", "-sWASM_ASYNC_COMPILATION=0",
                 "-sEXIT_RUNTIME=1", "-o", output / "wasm.js"])
            run(["node", output / "wasm.js"])
print("Borrowed strings: source/saved IR, C, native64/Wasm32 passed")
