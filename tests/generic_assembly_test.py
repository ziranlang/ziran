"""Check generic native assembly and each specialized portable body."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ZIRAN, WORK = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve()
ROOT = Path(__file__).resolve().parents[1]
ENV = dict(os.environ)
for key in ('DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'DBUS_SESSION_BUS_ADDRESS'):
    ENV.pop(key, None)
ENV.update(GO111MODULE='off', CARGO_TARGET_DIR=str(WORK / 'cargo-target'))
WORK.mkdir(parents=True, exist_ok=True)


def run(args, *, env=ENV, expected=None, ok=True, cwd=ROOT):
    result = subprocess.run(list(map(str, args)), env=env, cwd=cwd,
                            capture_output=True, text=True, timeout=180)
    if ok and result.returncode:
        raise RuntimeError(f'{args}\n{result.stdout}{result.stderr}')
    if expected is not None: assert result.stdout.strip() == str(expected), (args, result.stdout)
    return result


def main():
    source = WORK / 'source'; source.mkdir(exist_ok=True)
    for name in ('generic_assembly_ops.zi', 'generic_assembly_test.zi'):
        shutil.copyfile(ROOT / 'tests/spec' / name, source / name)
    entry = source / 'generic_assembly_test.zi'
    run([ZIRAN, 'ir', '--root', source, '-o', WORK / 'ir', entry])
    # Save the library before any caller exists, then instantiate a caller's
    # record and several integer/float widths from that saved template.
    library = WORK / 'library'; library.mkdir(exist_ok=True)
    run([ZIRAN, 'ir', '--root', source, '-o', library, source / 'generic_assembly_ops.zi'])
    shutil.copyfile(entry, library / entry.name)
    forms = [('source', source, entry), ('saved', WORK / 'ir', WORK / 'ir/generic_assembly_test.zir'),
             ('library', library, library / entry.name)]
    records = []
    for form, root, input_file in forms:
        bundle = WORK / (form + '.zib')
        run([ZIRAN, 'bundle', '--root', root, '--entry', 'generic_assembly_test:Answer', '-o', bundle, input_file])
        run([ZIRAN, 'run', bundle], expected=42)
        records.append(dict(form=form, target='zib'))
        for target in ('c', 'cpp', 'rust', 'go', 'py', 'plan9-c'):
            output = WORK / (form + '-' + target)
            run([ZIRAN, 'build', '--target=' + target, '--root', root,
                 *(['--pkg', 'main'] if target == 'go' else []), '-o', output, input_file])
            if target == 'plan9-c':
                for path in output.glob('*.c'):
                    assert '__asm__' not in path.read_text()
                records.append(dict(form=form, target=target, emitted=True))
                continue
            if target in ('c', 'cpp'):
                suffix, header, compiler, standard = ('c', 'h', os.environ.get('CC', 'cc'), 'c11') if target == 'c' else ('cpp', 'hpp', os.environ.get('CXX', 'c++'), 'c++17')
                host = output / ('host.' + suffix)
                host.write_text('#include <stdio.h>\n#include "generic_assembly_test.' + header + '"\nint main(void){long long value=Answer();if(value!=42)printf("generic case %lld\\n",value);return value==42?0:1;}\n')
                for optimization in (0, 2):
                    for mode in ('native', 'fallback'):
                        args = [compiler, '-std=' + standard, '-O' + str(optimization),
                                *(['-DZIRAN_NO_ASM'] if mode == 'fallback' else []),
                                '-I' + str(ROOT / 'include'), '-I' + str(output),
                                *sorted(output.glob('*.' + suffix)), '-o', output / 'run']
                        run(args); run([output / 'run'])
                        records.append(dict(form=form, target=target, optimization=optimization, mode=mode))
                assert '__asm__' in '\n'.join(p.read_text() for p in output.glob('*.' + suffix))
            elif target == 'rust':
                if not shutil.which('cargo'): continue
                (output / 'src/main.rs').write_text('fn main(){assert_eq!(ziran_generated::Answer(),42);}\n')
                assert 'core::arch::asm!' in (output / 'src/lib.rs').read_text()
                assert 'portable body for this variant' not in (output / 'src/lib.rs').read_text()
                for optimization in (0, 2):
                    for mode in ('native', 'fallback'):
                        selected = dict(ENV, RUSTFLAGS='-Dwarnings -Copt-level=' + str(optimization) + (' --cfg ziran_no_asm' if mode == 'fallback' else ''))
                        run(['cargo', 'run', '--quiet', '--offline', '--manifest-path', output / 'Cargo.toml'], env=selected)
                        records.append(dict(form=form, target=target, optimization=optimization, mode=mode))
            elif target == 'go':
                if not shutil.which('go'): continue
                (output / 'main.go').write_text('package main\nfunc main(){if GenericAssemblyTest_Answer()!=42{panic("generic assembly fallback")}}\n')
                for mode, enabled, flags in [('native','1','-O2 -g'), ('portable','0','-O2 -g'), ('forced','1','-O2 -g -DZIRAN_NO_ASM')]:
                    selected = dict(ENV, CGO_ENABLED=enabled, CGO_CFLAGS=flags, GOFLAGS='-tags=ziran_asm' if mode != 'portable' else '')
                    run(['go', 'build', '-o', output / 'run', '.'], env=selected, cwd=output); run([output / 'run'])
                    records.append(dict(form=form, target=target, mode=mode))
            else:
                run(['python3', '-c', 'import runpy; assert runpy.run_path(' + repr(str(output / '__init__.py')) + ')["Answer"]()==42'])
                records.append(dict(form=form, target=target))
    assert (WORK / 'source.zib').read_bytes() == (WORK / 'saved.zib').read_bytes()
    # Reject malformed unused declarations before specialization, then reject
    # bad concrete operands and broken portable bodies even when asm is valid.
    cases = [
        ('input-count', 'constraint counts', 'F :: (x:$T)->T #asm("x86_64","","=r","","",""){return x}'),
        ('duplicate', 'duplicate or too many', 'F :: (x:$T)->T #asm("x86_64","","=r","r","","") #asm("x86_64","","=r","r","",""){return x}'),
        ('known-input', 'named scalars or pointers', 'F :: (x:$T, text:string)->T #asm("x86_64","","=r","r;r","",""){return x}'),
        ('known-result', 'scalar/pointer operands', 'F :: (x:$T)->string #asm("x86_64","","=r","r","",""){return "bad"}'),
        ('qualified-result', 'constraint counts', 'ops :: #import "generic_assembly_ops"\nF :: (x: $Pair) -> ops.Pair #asm("x86_64", "", "=r", "r", "", "") { return ops.Pair.{0, 0} }'),
        ('missing-local', 'scalar/pointer locals', 'F :: (x:$T){#asm("x86_64","","=r:missing","","",""){}}'),
        ('return-block', 'must fall through', 'F :: (x:$T){#asm("x86_64","","","","",""){return}}'),
        ('aggregate', 'scalar/pointer operands', 'F :: (x:$T)->T #asm("x86_64","","=r","r","",""){return x}\nAnswer::()->s32{v: [2]s32 = .[1,2]; return F(v)[0]}'),
        ('specialized-output', 'constraint counts', 'F :: (x:$T)->T #asm("x86_64","","=r;=r","r","",""){return x}\nAnswer::()->s32{v: s32 = 1;return F(v)}'),
        ('specialized-block', 'scalar/pointer locals', 'F :: (x:$T)->T{#asm("x86_64","","=r:x","","",""){};return x}\nAnswer::()->s32{v: [2]s32 = .[1,2];return F(v)[0]}'),
        ('bad-body', 'not_a_binding', 'F :: (x:$T)->T #asm("x86_64","mov %[x], %[out]","=r","r","",""){return not_a_binding}\nAnswer::()->s32{v: s32 = 1;return F(v)}')]
    for name, message, text in cases:
        bad = WORK / (name + '.zi'); bad.write_text(text + '\n')
        result = run([ZIRAN, 'check', '--root', WORK, '--module-path', source, bad], ok=False)
        assert result.returncode and message in result.stderr, (name, result.stdout, result.stderr)
    (WORK / 'verification.json').write_text(json.dumps(dict(records=records, rejection_cases=[x[0] for x in cases],
        source_hashes={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in source.glob('*.zi')}), indent=2) + '\n')
    print('Generic assembly passed native/fallback execution, saved templates and specialization rejection checks')


if __name__ == '__main__': main()
