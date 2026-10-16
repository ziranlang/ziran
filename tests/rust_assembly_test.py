"""Execute checked GNU-style assembly through the native Rust backend."""
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys

ZIRAN, WORK = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve()
ROOT = Path(__file__).resolve().parents[1]
WORK.mkdir(parents=True, exist_ok=True)


def run(args, env=None, ok=True):
    result = subprocess.run(list(map(str, args)), cwd=ROOT, env=env, capture_output=True, text=True, timeout=180)
    if ok and result.returncode:
        raise RuntimeError(f'{args}\n{result.stdout}{result.stderr}')
    return result


def main():
    env = dict(os.environ)
    for key in ['DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'DBUS_SESSION_BUS_ADDRESS']:
        env.pop(key, None)
    env['CARGO_TARGET_DIR'] = str(WORK / 'cargo-target')
    source = WORK / 'asmcases.zi'
    shutil.copyfile(ROOT / 'tests/spec/rust_assembly_test.zi', source)
    run([ZIRAN, 'ir', '--root', WORK, '-o', WORK / 'ir', source], env)
    records = []
    for form, root, entry in [('source', WORK, source), ('saved', WORK / 'ir', WORK / 'ir/asmcases.zir')]:
        output = WORK / form
        run([ZIRAN, 'build', '--target=rust', '--root', root, '-o', output, entry], env)
        shutil.copyfile(ROOT / 'tests/rust_assembly_host.rs', output / 'src/main.rs')
        with (output / 'Cargo.toml').open('a') as manifest:
            manifest.write('\n[lib]\ncrate-type = ["rlib", "staticlib"]\n')
        native = (output / 'src/lib.rs').read_text()
        assert 'core::arch::asm!' in native and 'AtomicUsize' in native
        assert 'portable body for this variant' not in native
        assert 'options(pure' not in native and 'preserves_flags' not in native
        assert 'MaybeUninit' in native and 'addr_of_mut!' in native
        bundle = WORK / (form + '.zib')
        run([ZIRAN, 'bundle', '--root', root, '--entry', 'asmcases:Answer', '-o', bundle, entry], env)
        assert run([ZIRAN, 'run', bundle], env).stdout.strip() == '42'
        def body(name):
            return native.split('pub extern "C" fn ' + name + '(', 1)[1].split('\n#[no_mangle]', 1)[0]
        assert 'lateout(reg)' in body('AlternativeLate')
        assert 'op0 = out(reg)' in body('AlternativeEarly')
        assert 'op0 = out(reg)' in body('AlternativeEarlySuffix')
        assert 'op0 = inout(reg)' in body('AlternativeTied')
        assert 'core::ptr::addr_of!(value)' in body('AlternativeColumns')
        assert 'MaybeUninit::<u64>' in body('AlternativeMemory')
        for optimization in [0, 2]:
            for mode in ['native', 'fallback']:
                selected = dict(env)
                selected['RUSTFLAGS'] = '-Dwarnings -Copt-level=' + str(optimization)
                if platform.machine() == 'x86_64':
                    selected['RUSTFLAGS'] += ' -Ctarget-cpu=x86-64 -Ctarget-feature=-popcnt,-avx,-bmi2'
                if mode == 'fallback': selected['RUSTFLAGS'] += ' --cfg ziran_no_asm'
                result = run(['cargo', 'run', '--quiet', '--offline', '--manifest-path', output / 'Cargo.toml'], selected)
                records.append(dict(form=form, optimization=optimization, mode=mode, output=result.stdout.strip()))
        if form == 'source':
            (WORK / 'native-source.rs').write_text(native)
        else:
            assert native == (WORK / 'native-source.rs').read_text()
        # The same alternatives must obey GNU's contract on native C/C++ too.
        for target, compiler, extension, header, standard in [
                ('c', os.environ.get('CC', 'cc'), 'c', 'h', 'c11'),
                ('cpp', os.environ.get('CXX', 'c++'), 'cpp', 'hpp', 'c++17')]:
            generated = WORK / (form + '-' + target)
            run([ZIRAN, 'build', '--target=' + target, '--root', root, '-o', generated, entry], env)
            host = generated / ('host.' + extension)
            host.write_text('''#include "asmcases.HEADER"
#include <assert.h>
int main(void) {
    unsigned long long state = 0x123456789abcdefULL;
    assert(AlternativeGeneric() == 42);
    assert(Answer() == 42);
    for (unsigned i = 0; i < 200; i++) {
        state = state * 6364136223846793005ULL + 1;
        assert(Alternatives(state) == state);
        assert(AlternativeLate(state) == state);
        assert(AlternativeMemory(state) == state);
        assert(AlternativeColumns(state) == state);
        assert(AlternativeTied(state) == state + 7);
        assert(AlternativeEarly(state) == state);
        assert(AlternativeEarlySuffix(state) == state);
        assert(AlternativeCommutative(state, i) == (state ^ i));
        assert(AlternativeBlock(state) == state);
        assert(KeywordParameter(state) == state);
        assert(KeywordTie(state) == state + 7);
        KeywordComment(state);
        Pair parts = AlternativePair(state);
        assert(parts.low == state && parts.high == (state >> 32));
    }
    return 0;
}
'''.replace('HEADER', header))
            for optimization in [0, 2]:
                for mode in ['native', 'fallback']:
                    command = [compiler, '-std=' + standard, '-O' + str(optimization)]
                    if mode == 'fallback': command.append('-DZIRAN_NO_ASM')
                    run([*command, *generated.glob('*.' + extension), '-o', generated / 'check'], env)
                    run([generated / 'check'], env)
    assert (WORK / 'source.zib').read_bytes() == (WORK / 'saved.zib').read_bytes()
    # A real native assembler must reject a bad opcode. Forced fallback still
    # compiles the checked body, including when there are no operands.
    bad = WORK / 'bad.zi'
    arch = {'x86_64': 'x86_64', 'aarch64': 'aarch64', 'i686': 'x86'}.get(platform.machine())
    if arch:
        bad.write_text(f'#program_export\nBad :: () #asm("{arch}", "ziran_invalid_opcode", "", "", "", "volatile") {{}}\n')
        output = WORK / 'bad'
        run([ZIRAN, 'build', '--target=rust', '--root', WORK, '-o', output, bad], env)
        selected = dict(env, RUSTFLAGS='-Dwarnings')
        result = run(['cargo', 'build', '--quiet', '--offline', '--manifest-path', output / 'Cargo.toml'], selected, ok=False)
        assert result.returncode and 'ziran_invalid_opcode' in result.stderr
        (WORK / 'bad-native.log').write_text(result.stderr)
        selected['RUSTFLAGS'] += ' --cfg ziran_no_asm'
        run(['cargo', 'build', '--quiet', '--offline', '--manifest-path', output / 'Cargo.toml'], selected)
    unsupported = WORK / 'unsupported.zi'
    unsupported.write_text('''#program_export
UnavailableAlternatives :: (value: s64) -> s64 #asm("x86_64", "movq %[value], %[out]", "=b,b", "r,r", "", "") { return value }
#program_export
MismatchedAlternatives :: (value: s64) -> s64 #asm("x86_64", "movq %[value], %[out]", "=r,r", "r", "", "") { return value }
#program_export
Reserved :: (value: s64) -> s64 #asm("x86_64", "movq %[value], %[out]", "=b", "r", "", "") { return value }
#program_export
Unique :: () #asm("x86_64", ".Llabel%=: ", "", "", "", "volatile") {}
''')
    output = WORK / 'unsupported'
    run([ZIRAN, 'build', '--target=rust', '--root', WORK, '-o', output, unsupported], env)
    assert (output / 'src/lib.rs').read_text().count('portable body for this variant') == 4
    (output / 'src/main.rs').write_text('fn main() { assert_eq!(ziran_generated::UnavailableAlternatives(42),42); assert_eq!(ziran_generated::MismatchedAlternatives(42),42); assert_eq!(ziran_generated::Reserved(42),42); ziran_generated::Unique(); }\n')
    run(['cargo', 'run', '--quiet', '--offline', '--manifest-path', output / 'Cargo.toml'], dict(env, RUSTFLAGS='-Dwarnings'))
    (WORK / 'verification.json').write_text(json.dumps(dict(
        records=records, generated_sha256=hashlib.sha256((WORK / 'native-source.rs').read_bytes()).hexdigest(),
        source_sha256=hashlib.sha256(source.read_bytes()).hexdigest()), indent=2) + '\n')
    print('Native Rust function/block assembly, typed operands, memory aliases, CPU selection and forced fallback passed')


if __name__ == '__main__':
    main()
