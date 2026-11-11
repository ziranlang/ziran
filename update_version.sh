#!/bin/sh
set -eu
cd "$(dirname "$0")"
version=$(sed -n 's/^## \[\([0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*\)\].*/\1/p' CHANGELOG.md | head -n 1)
test -n "$version"
printf '%s\n' "$version" > VERSION
python3 - "$version" <<'PY'
from pathlib import Path
import re
import sys
import json

version = sys.argv[1]
for name in ('index.html', 'get-started.html', 'status.html', 'api.html'):
    path = Path('site') / name
    path.write_text(re.sub(r'v\d+\.\d+\.\d+', 'v' + version, path.read_text()))
inventory = {'version': version, 'modules': sorted(path.stem for path in Path('std').glob('*.zi'))}
Path('site/stdlib-release.json').write_text(json.dumps(inventory, indent=2) + '\n')
PY
