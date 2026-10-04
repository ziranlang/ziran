#!/bin/sh
set -eu
ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
# A shared import graph must be searched as a graph, rather than once per
# path. New/free have no foreign declaration to find in these modules.
python3 - "$work" <<'PY'
from pathlib import Path
import sys
root = Path(sys.argv[1])
for i in range(40):
    imports = ''.join(f'#import "layer_{j}";\n' for j in (i + 1, i + 2) if j < 40)
    (root / f'layer_{i}.zi').write_text(imports + f'Marker{i} :: {i};\n')
(root / 'main.zi').write_text('''#import "layer_0";
#program_export
Answer :: () -> s32 {
    p := New(s32)
    <<p = 42
    value := <<p
    free(p)
    return value
}
''')
PY
timeout 20 "$ziran" check --root "$work" "$work/main.zi"
timeout 20 "$ziran" bundle --root "$work" --entry main:Answer -o "$work/app.zib" "$work/main.zi"
test "$("$ziran" run "$work/app.zib")" = 42
printf '%s\n' 'Borrow checking terminates over shared import graphs'
