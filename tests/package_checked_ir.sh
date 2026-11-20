#!/bin/sh
set -eu
exec python3 - "$1" <<'PY'
import os
from pathlib import Path
import subprocess
import sys
import tempfile


tools = Path(sys.argv[1]).resolve().parent
with tempfile.TemporaryDirectory(prefix='package-checked-ir-', dir=tools.parent) as directory:
    root = Path(directory)
    model, widget, binary = (root / name for name in ('model', 'widget', 'binary'))
    for folder in (model, widget, binary):
        folder.mkdir()
    app = root / 'app.zi'
    app.write_text('''using Model :: #import "model/state"
using Widget :: #import "widget/control"
main :: () -> s32 {
    state: Model.State;
    Widget.Attach(*state);
    state.value = 40;
    return Model.Update(*state);
}
''')
    model_source = model / 'state.zi'
    model_source.write_text('''UpdateHandler :: #type (value: s32) -> s32;
State :: struct { value: s32; handler: UpdateHandler; }
Update :: (state: *State) -> s32 { return state.handler(state.value); }
''')
    widget_source = widget / 'control.zi'
    widget_source.write_text('''using Model :: #import "model/state"
Change :: (value: s32) -> s32 { return value + 2; }
Attach :: (state: *Model.State) { state.handler = Change; }
''')
    common = [('P', 'root', root), ('M', 'root', 'app', app),
              ('D', 'root', 'model/state', 'pmodel', 'state'),
              ('D', 'root', 'widget/control', 'pwidget', 'control')]
    edge = ('D', 'pwidget', 'model/state', 'pmodel', 'state')

    def command(tool, *arguments, rows=(), failure=None):
        module_map = root / 'module-map.tsv'
        module_map.write_text(''.join('\t'.join(map(str, row)) + '\n' for row in rows))
        env = dict(os.environ, ZIRAN_PACKAGE_MAP=str(module_map) if rows else '')
        result = subprocess.run([tools / tool, *arguments], env=env, cwd=root,
                                capture_output=True, text=True, timeout=30)
        if failure is not None:
            assert result.returncode != 0 and failure in result.stderr, result.stderr
            return
        assert result.returncode == 0, result.stderr
        return result.stdout.strip()

    command('zi2zir', '--root', root, '-o', binary, model_source, widget_source,
            rows=common + [('P', 'pmodel', model), ('P', 'pwidget', widget),
                           ('M', 'pmodel', 'state', model_source),
                           ('M', 'pwidget', 'control', widget_source), edge])
    model_source.unlink()
    widget_source.unlink()
    state = binary / 'pmodel_state.zir'
    control = binary / 'pwidget_control.zir'
    state, control = state.rename(model / state.name), control.rename(widget / control.name)
    rows = common + [('P', 'pmodel', model), ('P', 'pwidget', widget),
                     ('M', 'pmodel', 'state', state), ('M', 'pwidget', 'control', control)]
    bundle = root / 'app.zx'
    args = ('--root', root, '--entry', 'app:main', '-o', bundle, app)
    command('zi2zx', *args, rows=rows + [edge])
    assert command('zi2zx', 'run', '--backend', 'engine', bundle) == '42'
    command('zi2zx', *args, rows=rows, failure='outside the dependency scope')
    state.write_bytes(control.read_bytes())
    command('zi2zx', *args, rows=rows + [edge], failure='does not declare its mapped identity')

print('Checked package modules preserve shared typed state and enforce the saved dependency graph')
PY
