#!/bin/sh
set -eu

ziran=${1:?pass the ziran command}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

python3 - "$work/global_string.zi" <<'PY'
from pathlib import Path
import sys
source = 'long_text: string = "%s";\n\nAnswer :: () -> s64 { return long_text.count }\n' % ('a' * 2100)
Path(sys.argv[1]).write_text(source)
PY

for target in c cpp; do
    "$ziran" build "--target=$target" --root "$work" \
        -o "$work/$target" "$work/global_string.zi"
done
