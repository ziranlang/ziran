#!/bin/sh
set -eu

ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
mkdir -p "$work/src"

cat > "$work/src/main.zi" <<'ZI'
#program_export
Answer :: () -> s32 { return 42 }
ZI

"$ziran" build --target=c --root "$work/src" -o "$work/c" "$work/src/main.zi"
# The compiler command may carry flags; they follow the resolved program.
(cd "$work" && "$ziran" compile-commands --compiler "cc -O2" c)

python3 - "$work" <<'PY'
import json
import subprocess
import sys
from pathlib import Path

work = Path(sys.argv[1]).resolve()
entries = json.loads((work / "c/compile_commands.json").read_text())
assert entries, entries
for entry in entries:
    arguments = entry["arguments"]
    assert entry["directory"] == str(work), entry
    assert Path(arguments[0]).is_absolute() and arguments[1] == "-O2", arguments
    assert "-std=c99" in arguments and "-pedantic-errors" in arguments, arguments
    assert f"-I{work / 'c'}" in arguments, arguments
    assert arguments[-2:] == ["-c", entry["file"]], arguments
    # Every recorded command really compiles its file.
    subprocess.run(arguments[:-2] + ["-fsyntax-only", entry["file"]], check=True)
PY

if "$ziran" compile-commands --include "$work/missing" "$work/c" 2>/dev/null; then
    echo "a missing include directory was accepted" >&2
    exit 1
fi
