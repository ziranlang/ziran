#!/bin/sh
set -eu
# Verify the published snapshot and execute its real portable artifacts.
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
bin=$(CDPATH= cd -- "$(dirname -- "$ziran")" && pwd)
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
python3 "$repo/scripts/site_targets.py" --check
python3 - "$repo" "$bin" <<'PY'
import json
from pathlib import Path
import subprocess
import sys
repo, bin_dir = map(Path, sys.argv[1:])
site = repo / 'site'
data = json.loads((site / 'generated/targets/matrix.json').read_text())
for example in data['examples']:
    for target in ('zx', 'wasm', 'zir'):
        cell = example['targets'][target]
        if cell['status'] != 'ok':
            assert target == 'wasm' and 'outside the wasm execution subset' in cell['diagnostic']
            continue
        if target == 'zir':
            for file in cell['files']:
                subprocess.run([bin_dir / 'zi-inspect', site / file['path']], check=True, capture_output=True)
            continue
        artifact = site / cell['files'][0]['path']
        command = [bin_dir / 'zi2zx', 'run', '--vm', artifact] if target == 'zx' else ['node', repo / 'scripts/site_target_wasm.cjs', artifact, *(['--output'] if example['stdout'] else [])]
        result = subprocess.run(command, check=True, capture_output=True, text=True, timeout=30)
        assert result.stdout.strip() == (example['stdout'].strip() if example['stdout'] else example['expected']), (example['id'], target, result.stdout)
print('Published ZX and WebAssembly artifacts agree with the independent oracles; checked IR decodes')
PY
