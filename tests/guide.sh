#!/bin/sh
set -eu

ziran=${1:?pass the ziran command}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

"$ziran" guide > "$work/guide.txt"
"$ziran" --help > "$work/help.txt"
python3 - "$work/guide.txt" "$work/help.txt" "$work/main.zi" <<'PY'
from pathlib import Path
import sys

guide = Path(sys.argv[1]).read_text()
help_text = Path(sys.argv[2]).read_text()
assert 'ziran guide' in help_text
assert 'C, C++, Go, Rust, Python, and portable .zib are output paths; plan9-c is experimental.' in guide
assert 'Direct owned Vec locals and parameters drop at scope exit' in guide
assert 'ziran api --json' in guide
source = guide.split('Source shape:\n', 1)[1].split('\n\nDeclarations', 1)[0]
Path(sys.argv[3]).write_text('\n'.join(line[2:] for line in source.splitlines()) + '\n')
PY
"$ziran" check --root "$work" "$work/main.zi"
"$ziran" bundle --root "$work" --entry main:Answer \
    -o "$work/main.zib" "$work/main.zi"
test "$("$ziran" run "$work/main.zib")" = 42
if "$ziran" guide unexpected > "$work/out" 2> "$work/err"; then
    echo 'guide accepted unexpected arguments' >&2
    exit 1
fi
