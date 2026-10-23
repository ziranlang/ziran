#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
cat > "$work/program.zi" <<'ZI'
Answer :: () -> s32 { return 42 }
ZI
"$ziran" ir --root "$work" -o "$work/ir" "$work/program.zi"
"$ziran" bundle --root "$work" --entry program:Answer -o "$work/source.zx" "$work/program.zi"
"$ziran" bundle --root "$work/ir" --entry program:Answer -o "$work/saved.zx" "$work/ir/program.zir"
test "$("$ziran" run "$work/source.zx")" = 42
test "$("$ziran" run "$work/saved.zx")" = 42
"$ziran" check --target=zx --root "$work" "$work/program.zi"
"$ziran" capabilities --target=zx --json > "$work/capabilities.json"
python3 - "$work" "$ziran" <<'PY'
import json
from pathlib import Path
import subprocess
import sys

work = Path(sys.argv[1])
ziran = sys.argv[2]
for name in ('source', 'saved'):
    assert (work / (name + '.zx')).read_bytes()[:4] == b'ZX\0\0'
capabilities = json.loads((work / 'capabilities.json').read_text())
assert capabilities['target'] == 'zx', capabilities
# A previous-format signature must fail even if the file has today's suffix.
data = (work / 'source.zx').read_bytes()
(work / 'obsolete.zx').write_bytes(b'ZIB\0' + data[4:])
run = subprocess.run([ziran, 'run', str(work / 'obsolete.zx')],
                     capture_output=True, text=True)
assert run.returncode != 0, run.stdout
assert 'invalid or truncated bundle' in run.stderr, run.stderr
for command in ([ziran, 'capabilities', '--target=zib', '--json'],
                [ziran, 'check', '--target=zib', '--root', str(work),
                 str(work / 'program.zi')]):
    result = subprocess.run(command, capture_output=True, text=True)
    assert result.returncode != 0, result.stdout
print('Portable .zx format, source/saved IR, target discovery and legacy rejection passed')
PY
