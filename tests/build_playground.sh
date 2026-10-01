#!/bin/sh
# The Ziran build tool must preserve argv boundaries and private C names.
# Mock tool processes exercise orchestration without an Emscripten install.
set -eu
ziran=$(realpath "${1:?pass the ziran command}")
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY
mkdir -p "$work/tools with spaces" "$work/bin"
cat > "$work/bin/make" <<'PY'
#!/usr/bin/env python3
import json, os, sys
from pathlib import Path
stage = Path(sys.argv[0]).name
with open(os.environ['ZIRAN_PLAYGROUND_TEST_LOG'], 'a') as log:
    log.write(json.dumps({'stage':stage, 'args':sys.argv[1:], 'cwd':os.getcwd(),
                         'cache':os.environ.get('EM_CACHE'), 'frozen':'EM_FROZEN_CACHE' in os.environ,
                         'display':'DISPLAY' in os.environ or 'WAYLAND_DISPLAY' in os.environ})+'\n')
print('mock '+stage)
if stage == 'make':
    sys.exit(int(os.environ.get('ZIRAN_PLAYGROUND_MAKE_EXIT', '0')))
PY
cp "$work/bin/make" "$work/tools with spaces/emcc"
chmod +x "$work/bin/make" "$work/tools with spaces/emcc"
export PATH="$work/bin:$PATH"
export XDG_CACHE_HOME="$work/cache"
export ZIRAN_PLAYGROUND_TEST_LOG="$work/commands.jsonl"
export EM_CACHE="$work/em-cache"
export EM_FROZEN_CACHE=1

"$ziran" run --target=py "$repo/scripts/build_playground.zi" \
    --emcc "$work/tools with spaces/emcc" --output-dir "$work/assets with spaces" \
    --embed-file 'local file.zi@/file.zi' --embed-file 'modules@/modules' > "$work/output"
test "$(cat "$work/output")" = "$(printf 'mock make\nmock emcc')"
python3 - "$repo" "$work" <<'PY'
import json, shlex, sys
from pathlib import Path
repo, work = map(Path, sys.argv[1:])
calls = [json.loads(line) for line in (work/'commands.jsonl').read_text().splitlines()]
assert len(calls) == 2
make, emcc = calls
assert make['stage'] == 'make' and emcc['stage'] == 'emcc'
assert make['args'] == ['BUILD_DIR=build/playground64',
    'CC='+shlex.quote(str(work/'tools with spaces/emcc'))+' -sMEMORY64=2',
    'AR='+shlex.quote(str(work/'tools with spaces/emar')), 'OBJCOPY=true',
    'WASM_PRIVATE_HEADERS=build/playground64/private', 'CFLAGS=-O2',
    'FRAMEFLAGS=-Wframe-larger-than=16384', 'build/playground64/libziran.a']
assert emcc['args'][-8:] == ['--embed-file', 'std@/std', '--embed-file',
    'local file.zi@/file.zi', '--embed-file', 'modules@/modules',
    '-o', str(work/'assets with spaces/playground-runtime.js')]
assert '-sMEMORY64=2' in emcc['args'] and '-sSTACK_SIZE=8388608' in emcc['args']
for call in calls:
    assert call['cwd'] == str(repo) and call['cache'] == str(work/'em-cache')
    assert not call['frozen'] and not call['display']
private = repo/'build/playground64/private'
for group in ('parse', 'check', 'emit', 'vm'):
    lines = (private/(group+'.h')).read_text().splitlines()
    assert lines and len(lines) == len(set(lines))
    assert lines == sorted(lines)
    assert not any(line.startswith('#define '+keyword+' ') for line in lines
                   for keyword in ('char', 'void', 'int', 'const'))
assert '#define check_function private_check_check_function' in (private/'check.h').read_text()
PY

# Stop before the linker when make fails, preserving a nonzero result.
rm "$ZIRAN_PLAYGROUND_TEST_LOG"
code=0
ZIRAN_PLAYGROUND_MAKE_EXIT=17 "$ziran" run --target=py "$repo/scripts/build_playground.zi" \
    --emcc "$work/tools with spaces/emcc" --output-dir "$work/assets with spaces" > "$work/failure" || code=$?
test "$code" -eq 1
test "$(wc -l < "$ZIRAN_PLAYGROUND_TEST_LOG")" -eq 1
grep -Fq 'failed:' "$work/failure"
