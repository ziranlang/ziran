#!/bin/sh
set -eu
# Real enum behavior must agree from source and saved IR, including storage
# widths, signedness, flags, casts, imported names and enum-valued entries.
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
bin=$(CDPATH= cd -- "$(dirname -- "$ziran")" && pwd)
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
python3 - "$repo" "$bin" <<'PY'
from pathlib import Path
import subprocess
import sys
from contextlib import nullcontext

repo, tools = map(Path, sys.argv[1:])
def run(*args):
    result = subprocess.run(list(map(str, args)), capture_output=True, text=True, timeout=45)
    assert result.returncode == 0, (args, result.stdout, result.stderr)
    return result.stdout.strip()

root = repo / 'build/tests/wasm-enums'
root.mkdir(parents=True, exist_ok=True)
with nullcontext(root):
    widths = [('s8', -128, 127), ('u8', 0, 255), ('s16', -32768, 32767),
              ('u16', 0, 65535), ('s32', -2147483648, 2147483647),
              ('u32', 0, 4294967295), ('s64', -9223372036854775808, 9223372036854775807),
              ('u64', 0, 9223372036854775807)]
    declarations, checks = [], []
    for backing, minimum, maximum in widths:
        name = 'E' + backing
        declarations.append(f'{name} :: enum {backing} {{ Low :: {minimum}; High :: {maximum}; }}')
        declarations.append(f'Pass{backing} :: (value: {name}) -> {name} {{ return value; }}')
        checks.append(f'if Pass{backing}({name}.High) != {name}.High {{ return 0; }}')
        checks.append(f'if cast({backing}){name}.Low >= cast({backing}){name}.High {{ return 0; }}')
        checks.append(f'if size_of({name}) != {int(backing[1:]) // 8} {{ return 0; }}')
    (root / 'widths.zi').write_text('\n'.join(declarations) + '\nAnswer :: () -> s64 {\n' + '\n'.join(checks) + '\nreturn 42;\n}\n')
    (root / 'foreign.zi').write_text('''
Mode :: enum u8 { Low :: 7; High :: 255; }
Wide :: enum u64 { High :: 9223372036854775807; }
Box :: struct { first: Mode; value: Wide; last: Mode; }
Bounce :: (value: Mode) -> Mode { return value; }
WideValue :: () -> Wide { return .High; }
''')
    (root / 'storage.zi').write_text('''
Other :: #import "foreign";
Mode :: enum s64 { High :: -7; }
Mask :: enum_flags u8 { A; B; Top :: 128; }
Sign :: enum s8 { Low :: -128; High :: 127; }
Kind :: enum u16 { Low :: 1; High :: 65535; }
Record :: struct { tag: u8; sign: Sign; kind: Kind; mask: Mask; }
global_mask: Mask = Mask.Top;
global_record: Record = Record.{tag = 9, sign = Sign.Low, kind = Kind.High, mask = Mask.B};
global_values: [2]Kind = .[Kind.High, Kind.Low];
Pick :: (positive: bool) -> Sign { return ifx positive then Sign.High else Sign.Low; }
Answer :: () -> s64 {
    if Pick(false) != Sign.Low || Pick(true) != Sign.High { return 0; }
    if cast(s64)Pick(false) != -128 { return 0; }
    if cast(Sign)128 != Sign.Low { return 0; }
    if cast(Kind)65537 != Kind.Low { return 0; }
    flags: Mask = .Top;
    flags += .Top;
    if flags != 0 { return 0; }
    flags = .A | .B;
    flags ^= .A;
    flags |= .Top;
    if flags != 130 { return 0; }
    flags &= cast(Mask)(~cast(u8)Mask.Top);
    if flags != .B { return 0; }
    global_mask |= .B;
    if global_mask != 130 { return 0; }
    global_record.sign = .High;
    global_record.kind = .Low;
    global_record.mask |= .Top;
    if global_record.tag != 9 || cast(s64)global_record.sign != 127 || global_record.kind != .Low || global_record.mask != 130 { return 0; }
    local: Record = Record.{tag = 3, sign = Sign.Low, kind = Kind.High, mask = flags};
    copy := local;
    if copy.tag != 3 || cast(s64)copy.sign != -128 || cast(u64)copy.kind != 65535 || copy.mask != .B { return 0; }
    values: [3]Sign;
    values[0] = .Low;
    values[1] = .High;
    view := values[0:2];
    if cast(s64)view[0] != -128 || cast(s64)view[1] != 127 { return 0; }
    global_values[0] = .Low;
    if global_values[0] != .Low || global_values[1] != .Low { return 0; }
    if Other.Bounce(Other.Mode.High) != Other.Mode.High { return 0; }
    foreign: Other.Box = Other.Box.{first = Other.Mode.Low, value = Other.Wide.High, last = Other.Mode.High};
    foreign.first = Other.Mode.High;
    if cast(u64)foreign.first != 255 || cast(u64)foreign.last != 255 || foreign.value != Other.WideValue() { return 0; }
    if Mode.High != cast(Mode)-7 { return 0; }
    return 42;
}
''')
    cases = [('widths', 42), ('storage', 42)]
    for backing, minimum, maximum in widths:
        for label, value in [('low', minimum), ('high', maximum)]:
            name = 'entry_' + backing + '_' + label
            (root / (name + '.zi')).write_text(f'Mode :: enum {backing} {{ Value :: {value}; }}\nAnswer :: () -> Mode {{ return .Value; }}\n')
            # The portable entry ABI carries all integer results as signed i64 bits.
            expected = value if value < (1 << 63) else value - (1 << 64)
            cases.append((name, expected))
    for name, expected in cases:
        source = root / (name + '.zi')
        ir = root / ('ir-' + name)
        run(tools / 'zi2zir', '--root', root, '-o', ir, source)
        images = []
        for form, input_, search in [('source', source, root), ('saved', ir / (name + '.zir'), ir)]:
            image = root / (name + '-' + form + '.wasm')
            run(tools / 'zi2wasm', '--root', search, '--entry', name + ':Answer', '-o', image, input_)
            actual = run('node', repo / 'scripts/site_target_wasm.cjs', image)
            assert actual == str(expected), (name, form, actual, expected)
            bundle = root / (name + '-' + form + '.zx')
            run(tools / 'zi2zx', 'bundle', '--root', search, '--entry', name + ':Answer', '-o', bundle, input_)
            assert run(tools / 'zi2zx', 'run', '--vm', bundle) == str(expected), (name, form, 'VM differs')
            images.append(image.read_bytes())
        assert images[0] == images[1], (name, 'source and saved IR differ')
print('WebAssembly enums: eight backings, signed/unsigned limits, narrowing, flags, globals, arrays, slices, records, imported scopes and enum entry results agree with the VM from source and saved IR')
PY
