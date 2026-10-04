#!/bin/sh
set -eu
ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

# Shared imports and unused state are normal in whole application graphs.
# Locals must not trigger a complete global lookup for every declaration.
python3 - "$work" <<'PY'
from pathlib import Path
import sys
root = Path(sys.argv[1])
for index in range(40):
    imports = ''.join(f'using Next{child} :: #import "layer_{child}";\n'
                      for child in (index + 1, index + 2) if child < 40)
    globals = ''.join(f'unused_{index}_{item}: s32;\n' for item in range(40))
    leaf = '''leaf_count: s32;
Leaf :: () -> s32 { leaf_count += 1; return leaf_count }
''' if index == 39 else ''
    (root / f'layer_{index}.zi').write_text(imports + globals + leaf)
(root / 'state.zi').write_text('counter: s32;\nunused_counter: s32;\n')
(root / 'app.zi').write_text('''#import "layer_0";
State :: #import "state";
#program_export
Answer :: () -> s32 {
    pointer := New(s32)
    <<pointer = 40
    value := <<pointer
    free(pointer)
    leaf_count = 0
    State.counter += 1
    return value + State.counter + Leaf()
}
''')
PY
timeout 20 "$ziran" ir --root "$work" -o "$work/ir" "$work/app.zi"
timeout 20 "$ziran" bundle --root "$work" --entry app:Answer \
    -o "$work/source.zib" "$work/app.zi"
timeout 20 "$ziran" bundle --root "$work/ir" --entry app:Answer \
    -o "$work/saved.zib" "$work/ir/app.zir"
cmp "$work/source.zib" "$work/saved.zib"
test "$("$ziran" run "$work/source.zib")" = 42
test "$("$ziran" run "$work/saved.zib")" = 42
echo 'Application globals link through shared imports with source/saved parity'
