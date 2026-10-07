#!/bin/sh
set -eu
ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

python3 - "$ziran" "$work" "$repo" <<'PY'
import json
from pathlib import Path
import subprocess
import sys

ziran, work, repo = sys.argv[1], Path(sys.argv[2]), Path(sys.argv[3])
node = 'Node :: struct { bytes: [2]u8; other: [2]u8; result: s32 }\n'
writer = 'Write :: (node: *Node) { node.bytes[0] = cast(u8)98 }\n'
cases = {
    'call': writer + '''Bad :: () -> s32 {
        node: Node
        text := TextView(node.bytes[:])
        Write(*node)
        return cast(s32)text[0]
    }''',
    'transitive': '''Bad :: () -> s32 {
        node: Node
        text := TextView(node.bytes[:])
        Indirect(*node)
        return cast(s32)text[0]
    }
    Indirect :: (node: *Node) { Write(node) }
    ''' + writer,
    'pointer_parameters': '''Bad :: (first: *Node, second: *Node) -> s32 {
        text := TextView(first.bytes[:])
        second.bytes[0] = cast(u8)98
        return cast(s32)text[0]
    }''',
    'slice_parameters': '''Bad :: (first: []u8, second: []u8) -> s32 {
        text := TextView(first[:])
        second[0] = cast(u8)98
        return cast(s32)text[0]
    }''',
    'slice_call': '''WriteSlice :: (bytes: []u8) { bytes[0] = cast(u8)98 }
    Bad :: () -> s32 {
        bytes: [2]u8
        text := TextView(bytes[:])
        WriteSlice(bytes[:])
        return cast(s32)text[0]
    }''',
    'callback': '''Mutator :: #type (node: *Node) -> ();
    Bad :: (mutate: Mutator) -> s32 {
        node: Node
        text := TextView(node.bytes[:])
        mutate(*node)
        return cast(s32)text[0]
    }''',
    'global': '''bytes: [2]u8;
    WriteGlobal :: () { bytes[0] = cast(u8)98 }
    Bad :: () -> s32 {
        text := TextView(bytes[:])
        WriteGlobal()
        return cast(s32)text[0]
    }''',
    'global_view_parameter': '''bytes: [2]u8;
    saved: string;
    Hold :: () { saved = TextView(bytes[:]) }
    WriteSlice :: (storage: []u8) { storage[0] = cast(u8)98 }
    Forward :: (storage: []u8) { WriteSlice(storage) }
    Bad :: () -> s32 {
        Hold()
        Forward(bytes[:])
        return cast(s32)saved[0]
    }''',
    'global_view_pointer_alias': '''state: Node;
    saved: string;
    Hold :: () { saved = TextView(state.bytes[:]) }
    Bad :: () -> s32 {
        Hold()
        pointer := *state
        pointer.bytes[0] = cast(u8)98
        return cast(s32)saved[0]
    }''',
    'local_record_global_backing': '''bytes: [2]u8;
    Box :: struct { text: string }
    WriteGlobal :: () { bytes[0] = cast(u8)98 }
    Bad :: () -> s32 {
        box: Box
        box.text = TextView(bytes[:])
        WriteGlobal()
        return cast(s32)box.text[0]
    }''',
    'local_record_member_backing': '''Box :: struct { text: string }
    Bad :: () -> s32 {
        node: Node
        box: Box
        box.text = TextView(node.bytes[:])
        text := box.text
        Write(*node)
        return cast(s32)text[0]
    }\n''' + writer,
    'nested_record_argument_backing': '''Box :: struct { text: string }
    Consume :: (node: *Node, box: Box) -> s32 {
        Write(node)
        return cast(s32)box.text[0]
    }
    Bad :: () -> s32 {
        node: Node
        box: Box
        box.text = TextView(node.bytes[:])
        return Consume(*node, box)
    }\n''' + writer,
    'inline': '''WriteRead :: (node: *Node, text: string) -> s32 {
        node.bytes[0] = cast(u8)98
        return cast(s32)text[0]
    }
    Bad :: () -> s32 {
        node: Node
        return WriteRead(*node, TextView(node.bytes[:]))
    }''',
    'argument_order': '''WriteResult :: (node: *Node) -> s32 {
        node.bytes[0] = cast(u8)98
        return 1
    }
    Use :: (text: string, value: s32) -> s32 { return cast(s32)text[0] + value }
    Bad :: () -> s32 {
        node: Node
        return Use(TextView(node.bytes[:]), WriteResult(*node))
    }''',
}
for name, body in cases.items():
    source = work / (name + '.zi')
    source.write_text(node + body + '\n')
    result = subprocess.run([ziran, 'check', '--diagnostics=json', '--root', str(work), str(source)],
                            capture_output=True, text=True, timeout=30)
    assert result.returncode != 0, (name, 'unsafe mutation accepted')
    items = [json.loads(line) for line in result.stderr.splitlines()]
    assert any(item['code'] == 'check.slice_lifetime' and
               'mutating text backing storage' in item['message'] for item in items), (name, items)

identity = work / 'identity.zi'
identity.write_text('''
    Identity :: struct { name: string; context: *void }
    Owner :: struct { identity: Identity }
    Signer :: #type (context: *void, text: string, output: []u8) -> bool;
    Sign :: (context: *void, text: string, output: []u8) -> bool {
        unused context
        output[0] = text[0]
        return true
    }
    Send :: (identity: Identity, sign: Signer, output: []u8) -> bool {
        return sign(identity.context, identity.name, output)
    }
    FromRecord :: (owner: *Owner, sign: Signer) -> s32 {
        bytes: [1]u8
        if !Send(owner.identity, sign, bytes[:]) { return 0 }
        return cast(s32)bytes[0]
    }
    #program_export
    CheckIdentity :: () -> s32 {
        owner: Owner
        owner.identity.name = "a"
        if FromRecord(*owner, Sign) != 97 { return 1 }
        return 0
    }
''')
subprocess.run([ziran, 'ir', '--root', str(work), '-o', str(work / 'identity-ir'), str(identity)], check=True)
for source in (identity, work / 'identity-ir/identity.zir'):
    subprocess.run([ziran, 'check', '--root', str(work), str(source)], check=True)
    native = work / ('c-source' if source == identity else 'c-saved')
    subprocess.run([ziran, 'build', '--target=c', '--no-main', '--root', str(work),
                    '-o', str(native), str(source)], check=True)
    (native / 'main.c').write_text('#include "identity.h"\nint main(void) { return CheckIdentity(); }\n')
    subprocess.run(['cc', '-std=c11', '-I' + str(repo / 'include'),
                    '-I' + str(native), *map(str, native.glob('*.c')),
                    '-o', str(native / 'test'), '-lm'], check=True)
    subprocess.run([str(native / 'test')], check=True)
    bundle = work / 'identity.zib'
    subprocess.run([ziran, 'bundle', '--root', str(work), '--entry', 'identity:CheckIdentity',
                    '-o', str(bundle), str(source)], check=True)
    result = subprocess.run([ziran, 'run', str(bundle)], capture_output=True, text=True, check=True)
    assert result.stdout.strip() == '0', result

safe = work / 'safe.zi'
safe.write_text(node + writer + '''
    global_bytes: [2]u8 = .[97, 98];
    global_view: string;
    Hold :: () { global_view = TextView(global_bytes[:]) }
    Read :: (node: *Node) -> s32 { return cast(s32)node.bytes[0] }
    Sibling :: (node: *Node) { node.other[0] = cast(u8)99 }
    Forward :: (node: *Node) { Sibling(node) }
    Params :: (first: *Node, second: *Node) -> s32 {
        text := TextView(first.bytes[:])
        second.result = 42
        return cast(s32)text[0]
    }
    #program_export
    Answer :: () -> s32 {
        Hold()
        node: Node
        node.bytes[0] = 97
        node.bytes[1] = 98
        {
            text := TextView(node.bytes[:])
            Forward(*node)
            if Read(*node) != 97 || text != "ab" { return 1 }
            if Params(*node, *node) != 97 { return 2 }
        }
        Write(*node)
        if node.bytes[0] != 98 || node.other[0] != 99 { return 3 }
        if global_view != "ab" { return 4 }
        return node.result
    }
''')
subprocess.run([ziran, 'ir', '--root', str(work), '-o', str(work / 'ir'), str(safe)], check=True)
for source in (safe, work / 'ir/safe.zir'):
    subprocess.run([ziran, 'check', '--root', str(work), str(source)], check=True)
    bundle = work / 'safe.zib'
    subprocess.run([ziran, 'bundle', '--root', str(work), '--entry', 'safe:Answer',
                    '-o', str(bundle), str(source)], check=True)
    result = subprocess.run([ziran, 'run', str(bundle)], capture_output=True, text=True, check=True)
    assert result.stdout.strip() == '42', result
PY
