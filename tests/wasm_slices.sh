#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
bin=$(CDPATH= cd -- "$(dirname -- "$ziran")" && pwd)
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
python3 - "$repo" "$bin" <<'PY'
from pathlib import Path
import subprocess
import sys
repo, tools = map(Path, sys.argv[1:])
root = repo / 'build/tests/wasm-slices'
root.mkdir(parents=True, exist_ok=True)
source = root / 'slices.zi'
source.write_text('''Sum :: (values: []s32) -> s32 {
    total: s32; for values { total += it; } return total;
}
Answer :: () -> s32 {
    values := s32.[2, 4, 8];
    full := values[:];
    prefix := values[:3];
    suffix := values[1:];
    empty := values[3:3];
    tail := values[3:];
    nested := full[:];
    if full.count != 3 || prefix.count != 3 || suffix.count != 2 { return 1; }
    if empty.count != 0 || tail.count != 0 || nested.count != 3 { return 2; }
    if Sum(full) != 14 || Sum(prefix) != 14 || Sum(suffix) != 12 { return 3; }
    if Sum(empty) != 0 || Sum(tail) != 0 || Sum(nested) != 14 { return 4; }
    return 42;
}
''')
def run(*args):
    return subprocess.run(list(map(str, args)), check=True, capture_output=True, text=True, timeout=45).stdout.strip()
ir = root / 'ir'
run(tools / 'zi2zir', '--root', root, '-o', ir, source)
for form, input_, search in [('source', source, root), ('saved', ir / 'slices.zir', ir)]:
    for target in ('wasm', 'zx'):
        image = root / (form + '.' + target)
        run(tools / ('zi2' + target), *(['bundle'] if target == 'zx' else []), '--root', search, '--entry', 'slices:Answer', '-o', image, input_)
        actual = run(tools / 'zi2zx', 'run', '--vm', image) if target == 'zx' else run('node', repo / 'scripts/site_target_wasm.cjs', image)
        assert actual == '42', (form, target, actual)
print('Wasm and VM: omitted slice bounds, full-length and empty slices agree from source and saved IR')
PY
