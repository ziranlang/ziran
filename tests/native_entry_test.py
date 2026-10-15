import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

ziran, work = (Path(arg).resolve() for arg in sys.argv[1:])
repo = Path(__file__).resolve().parents[1]
compilers = {'c': shutil.which(os.environ.get('CC', 'cc')),
             'cpp': shutil.which(os.environ.get('CXX', 'c++'))}

def run(command, *, answer=0, text=None):
    result = subprocess.run([str(part) for part in command], capture_output=True, text=True, timeout=30)
    assert result.returncode == answer, (command, result.returncode, result.stdout, result.stderr)
    if text is not None: assert result.stdout == text, (command, result.stdout, text)
    return result

cases = {
    'unsigned': ('''#program_export
main :: (argc: s32, argv: **u8) -> s32 {
    if argc != 3 { return 71 }
    if argv[1][0] != 97 || argv[2][0] != 207 || argv[2][1] != 128 { return 72 }
    if argv[argc] != null { return 73 }
    return argc + 39
}
''', 42, ''),
    'signed': ('''#program_export
main :: (argc: s32, argv: **s8) -> s32 {
    if argc != 3 { return 71 }
    if argv[1][0] != 97 || argv[2][0] != -49 || argv[2][1] != -128 { return 72 }
    if argv[argc] != null { return 73 }
    return argc + 39
}
''', 42, ''),
    'early': ('''#program_export
main :: (argc: s32, argv: **u8) {
    unused argv
    if argc == 3 { print("void\\n"); return }
    print("bad\\n")
}
''', 0, 'void\n'),
    'fallthrough': ('#program_export\nmain :: () { print("fallthrough\\n") }\n', 0, 'fallthrough\n'),
    'wide': ('''Callback :: #type () -> u64;
#program_export "main"
Launch :: () -> u64 { return 4294967338 }
#program_export
Probe :: () -> s64 { value: Callback = Launch; return cast(s64)(value()) }
''', 42, ''),
    'renamed': ('''#program_export "Other"
main :: () -> s64 { return 7 }
#program_export
Probe :: () -> s64 { return main() + 35 }
''', 42, ''),
    'renamedvoid': ('''Callback :: #type () -> void;
Count: s32 = 0;
#program_export "Other"
main :: () { Count += 1 }
#program_export
Probe :: () -> s32 { value: Callback = main; value(); return Count + 40 }
''', 42, ''),
    'implicit': ('main :: () -> s32 { return 42 }\n', 42, ''),
    'collision': ('''main: s32 = 41;
#program_export "main"
Launch :: () -> s32 { return main + 1 }
''', 42, ''),
    'keywords': ('''#program_export
main :: (class: isize, switch: **u8) -> usize {
    if class != 3 || switch[1][0] != 97 { return 71 }
    return 42
}
''', 42, ''),
    'recursive': ('''#program_export
main :: (argc: s32, argv: **u8) -> s32 {
    if argc == 0 { return 39 }
    return main(argc - 1, argv) + 1
}
''', 42, ''),
    'voidslot': ('''Callback :: #type () -> void;
#program_export
main :: () { return }
#program_export
Probe :: () -> s32 { value: Callback = main; value(); return 42 }
''', 0, ''),
    'assembly': ('''#program_export
main :: () #asm("x86_64+popcnt", "", "", "", "memory", "volatile") {
    return
}
''', 0, ''),
}
compiled = 0
cases['longentry'] = ('#program_export "main"\nLaunch_'+('a'*180)+' :: () -> s32 { return 42 }\n', 42, '')
for name, (source, answer, output) in cases.items():
    folder = work/name
    folder.mkdir()
    module = 'main' if name == 'implicit' else name
    path = folder/(module+'.zi')
    path.write_text(source)
    run([ziran, 'ir', '--root', folder, '-o', folder/'ir', path])
    forms = {'source': (folder, path), 'saved': (folder/'ir', folder/'ir'/(module+'.zir'))}
    for form, (root, input_file) in forms.items():
        if name in ['wide', 'voidslot']:
            bundle = folder/(form+'.zib')
            run([ziran, 'bundle', '--root', root, '--entry', module+':Probe', '-o', bundle, input_file])
            run([ziran, 'run', bundle], text='4294967338\n' if name == 'wide' else '42\n')
        for target, compiler in compilers.items():
            out = folder/(target+'-'+form)
            suffix, header, standard = ('c', 'h', 'c11') if target == 'c' else ('cpp', 'hpp', 'c++17')
            run([ziran, 'build', '--target='+target, '--root', root, '-o', out, input_file])
            generated = out/(module+'.'+suffix)
            flags = ['-std='+standard, '-O2', '-Werror=main', '-I'+str(repo/'include'), '-I'+str(out)]
            if target == 'c': flags += ['-Werror=incompatible-pointer-types']
            host = folder/('host.'+suffix)
            sources = [generated]
            if name in ['renamed', 'renamedvoid']:
                call = 'void (*value)(void) = Other; value(); ' if name == 'renamedvoid' else ''
                host.write_text('#include "'+module+'.'+header+'"\nint main(int argc, char **argv) { '+call+'(void)argc; (void)argv; return (int)Probe(); }\n')
                sources.append(host)
                assert not re.search(r'^main\(', generated.read_text(), re.M)
            else:
                assert 'main(int argc, char **argv)' in generated.read_text()
                assert 'main(int argc, char **argv)' in (out/(module+'.'+header)).read_text()
            run([compiler, *flags, *sources, '-o', out/'run'])
            run([out/'run', 'alpha', 'π'], answer=answer, text=output)
            compiled += 1
            if form == 'source' and name == 'unsigned' and os.environ.get('ANDROID_NDK_HOME'):
                tools = Path(os.environ['ANDROID_NDK_HOME'])/'toolchains/llvm/prebuilt/linux-x86_64/bin'
                for triple in ['aarch64-linux-android24', 'armv7a-linux-androideabi24',
                               'x86_64-linux-android24', 'i686-linux-android24']:
                    cross = tools/(triple+('-clang' if target == 'c' else '-clang++'))
                    run([cross, *flags, generated, '-o', out/(triple+'.elf')])
            if form == 'source' and name in ['unsigned', 'assembly']:
                web = shutil.which('emcc' if target == 'c' else 'em++')
                node = shutil.which('node')
                if web and node:
                    run([web, *flags, '-sENVIRONMENT=node', '-sWASM_ASYNC_COMPILATION=0',
                         '-sEXIT_RUNTIME=1', generated, '-o', out/'web.js'])
                    run([node, out/'web.js', 'alpha', 'π'], answer=answer, text=output)
            if name == 'assembly':
                run([compiler, *flags, '-DZIRAN_NO_ASM', generated, '-o', out/'fallback'])
                run([out/'fallback'], answer=0)
            if name in ['wide', 'voidslot']:
                # Check the typed body through a function value, without the
                # process entry's deliberate conversion to an integer status.
                run([compiler, *flags, '-Dmain=process_main', '-c', generated, '-o', out/'body.o'])
                expected = 'UINT64_C(4294967338)' if name == 'wide' else '42'
                host.write_text('#include "'+module+'.'+header+'"\nint main(int argc, char **argv) { (void)argc; (void)argv; return Probe() == '+expected+' ? 0 : 1; }\n')
                run([compiler, *flags, out/'body.o', host, '-o', out/'probe'])
                run([out/'probe'])
                if target == 'c':
                    cpp_host = host.with_suffix('.cpp')
                    cpp_host.write_bytes(host.read_bytes())
                    run([compilers['cpp'], '-std=c++17', '-O2', '-I'+str(repo/'include'), '-I'+str(out), out/'body.o', cpp_host, '-o', out/'cpp-probe'])
                    run([out/'cpp-probe'])
    for target, suffix in [('c', 'c'), ('cpp', 'cpp')]:
        assert (folder/(target+'-source')/(module+'.'+suffix)).read_bytes() == (folder/(target+'-saved')/(module+'.'+suffix)).read_bytes()

# Invalid native boundaries fail in the compiler, not in the downstream C/C++
# compiler. Their ordinary portable procedure signatures remain legal.
bad_cases = ['main :: () -> string { return "text" }',
             'main :: (argc: string, argv: **u8) -> s32 { return 0 }',
             'main :: (argc: s32, argv: **s32) -> s32 { return 0 }',
             'main :: (argc: s32) -> s32 { return 0 }']
for index, text in enumerate(bad_cases):
    source = work/f'bad{index}.zi'
    source.write_text('#program_export\n'+text+'\n')
    run([ziran, 'check', '--root', work, source])
    for target in ['c', 'cpp']:
        result = subprocess.run([str(ziran), 'build', '--target='+target, '--diagnostics=json',
                                 '--root', str(work), '-o', str(work/'bad-output'), str(source)],
                                capture_output=True, text=True, timeout=30)
        assert result.returncode != 0, result.stdout
        items = [json.loads(line) for line in result.stderr.splitlines()]
        assert any(item['capability'] == 'entry.signature' for item in items), items
print(f'Native C/C++ entries preserve typed bodies, argument bytes, status, exports, assembly fallback and saved IR ({compiled} executables)')
