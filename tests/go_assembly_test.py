"""Exercise native Go assembly, its checked fallback, aliases and Go lifetimes."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ZIRAN, WORK = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve()
ROOT = Path(__file__).resolve().parents[1]
ENV = dict(os.environ, GO111MODULE='off')
for key in ('DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'DBUS_SESSION_BUS_ADDRESS',
            'CGO_CFLAGS', 'CGO_CPPFLAGS', 'GOFLAGS', 'GOEXPERIMENT'):
    ENV.pop(key, None)
WORK.mkdir(parents=True, exist_ok=True)
ENV['GOCACHE'] = str(WORK/'go-cache')


def run(args, *, cwd=ROOT, env=ENV, expected=None, ok=True):
    result = subprocess.run(list(map(str, args)), cwd=cwd, env=env,
                            capture_output=True, text=True, timeout=180)
    if ok and result.returncode:
        raise RuntimeError(f'{args}\n{result.stdout}{result.stderr}')
    if expected is not None:
        assert result.stdout.strip() == expected, (args, result.stdout, expected)
    return result


HOST = '''package main
import ("fmt"; "sync")
func require(ok bool) { if !ok { panic("Go assembly conformance") } }
func main() {
    marker := GoAssemblyTest_Marker()
    require(marker == EXPECTED)
    block := int32(3); if marker == 7 { block = 9 }
    require(GoAssemblyTest_BlockMarker() == block && GoAssemblyTest_Startup() == marker)
    require(RustAssemblyTest_Answer() == 42)
    require(RustAssemblyTest_Byte(-100) == -100 && RustAssemblyTest_Narrow(0x1234567887654321) == 0x87654321)
    require(RustAssemblyTest_FixedDestination(0x1234567887654321) == 0x87654321)
    require(RustAssemblyTest_Odd(7) && !RustAssemblyTest_Odd(8) && RustAssemblyTest_BoolValue(true) == 1)
    require(RustAssemblyTest_Add(-9) == -2 && RustAssemblyTest_NamedTie(-9) == -2)
    pair := RustAssemblyTest_DivParts(47, 5); require(pair.Low == 9 && pair.High == 2)
    pair = RustAssemblyTest_MemParts(0x1234567887654321); require(pair.Low == 0x1234567887654321 && pair.High == 0x12345678)
    require(RustAssemblyTest_MemInput(0xffffffffffffffff) == 0xffffffffffffffff)
    keywords := RustAssemblyTest_KeywordFields(57); require(keywords.Type == 57 && keywords.ZiranKeywordType0 == 57)
    require(RustAssemblyTest_Word(41) == 42 && RustAssemblyTest_SignedWord(-7) == -7)
    require(RustAssemblyTest_FloatAdd(1.25, 2.5) == 3.75)
    require(RustAssemblyTest_Memory(-13) == -26 && RustAssemblyTest_Mixed(93) == 0 && RustAssemblyTest_BlockOdd(93))
    require(RustAssemblyTest_Shadow(17,25) == 42 && RustAssemblyTest_Dialect(-19) == -19 && RustAssemblyTest_DialectSuffix(-19) == -19)
    require(RustAssemblyTest_Labels(7) == 28 && RustAssemblyTest_LabelData() == 48)
    RustAssemblyTest_Barrier()
    require(GoAssemblyTest_InputAlias(5) == 18)
    value := int32(40); GoAssemblyTest_Store(&value); require(value == 41)
    require(GoAssemblyTest_Pointer(&value) == &value && GoAssemblyTest_PointerBlock(&value) == &value)
    pointed := GoAssemblyTest_Pair(-17,&value); require(pointed.First == -17 && pointed.Second == 7 && pointed.Pointer == &value)
    link := Link{Value:42}; link.Next = &link; require(GoAssemblyTest_Graph(&link) == 42)
    GoAssemblyTest_Comment(); require(GoAssemblyTest_Shadow(42) == 42)
    var wait sync.WaitGroup
    for worker:=0;worker<4;worker++ { wait.Add(1); go func(){defer wait.Done(); for i:=0;i<200;i++ {
        require(RustAssemblyTest_Count(^uint64(0)) == 64 && RustAssemblyTest_Count32(^uint32(0)) == 32)
        require(GoAssemblyTest_InputAlias(int64(i)) == 18)
    }}() }; wait.Wait()
    fmt.Println(marker,block)
}
'''


def main():
    if not shutil.which('go'):
        print('Go toolchain unavailable; native Go assembly execution not checked')
        return
    source = WORK/'source'; source.mkdir(exist_ok=True)
    for name in ('go_assembly_test.zi', 'rust_assembly_test.zi'):
        shutil.copy2(ROOT/'tests/spec'/name, source/name)
    inputs = [source/'go_assembly_test.zi', source/'rust_assembly_test.zi']
    run([ZIRAN, 'ir', '--root', source, '-o', WORK/'ir', *inputs])
    records = []
    for form in ('source', 'saved'):
        root = source if form == 'source' else WORK/'ir'
        entry = inputs if form == 'source' else [root/'go_assembly_test.zir', root/'rust_assembly_test.zir']
        out = WORK/form
        run([ZIRAN,'build','--target=go','--pkg','main','--root',root,'--prune-stale','-o',out,*entry])
        assert list(out.glob('*.assembly.go')) and list(out.glob('*.assembly.h'))
        for mode in ('portable', 'ordinary', 'native', 'forced', 'checked'):
            selected = dict(ENV, CGO_ENABLED='0' if mode == 'portable' else '1')
            if mode in ('native','forced','checked'): selected['GOFLAGS'] = '-tags=ziran_asm'
            if mode == 'forced': selected['CGO_CFLAGS'] = '-O2 -g -DZIRAN_NO_ASM'
            if mode == 'checked': selected['GOEXPERIMENT'] = 'cgocheck2'
            marker = 3 if mode in ('portable','ordinary','forced') else 7
            (out/'main.go').write_text(HOST.replace('EXPECTED',str(marker)))
            run(['go','build','-o',out/'run','.'],cwd=out,env=selected)
            run([out/'run'],env=selected,expected=f'{marker} {3 if marker == 3 else 9}')
            records.append(dict(form=form,mode=mode,marker=marker))
        # Legacy checks pass filenames. Their adapter must select the same
        # generated companions as a package build, including explicit tags.
        if form == 'source':
            real_go = os.environ.get('ZIRAN_TEST_GO_REAL') or shutil.which('go')
            for tags, marker in (([],3), (['-tags','ziran_asm'],7), (['-tags=ziran_asm'],7)):
                (out/'main.go').write_text(HOST.replace('EXPECTED',str(marker)))
                run([sys.executable,ROOT/'tests/go_tool.py','--go',real_go,'build',*tags,
                     '-o',out/'file-list',*sorted(out.glob('*.go'))],cwd=out,
                    env=dict(ENV,CGO_ENABLED='1'))
                run([out/'file-list'],expected=f'{marker} {3 if marker == 3 else 9}')
            records.append(dict(file_list_tag_selection=True))
        # Source/saved native headers retain the full GNU text; Go comments
        # cannot terminate a template because only an include enters Go's preamble.
        assert '*/ and' not in '\n'.join(p.read_text() for p in out.glob('*.assembly.go'))
        for target in ('c','cpp'):
            native = WORK/(form+'-'+target)
            run([ZIRAN,'build','--target='+target,'--root',root,'-o',native,entry[0]])
            suffix,header,compiler = ('c','h','cc') if target=='c' else ('cpp','hpp','c++')
            (native/('main.'+suffix)).write_text('#include "go_assembly_test.'+header+'"\nint main(void){return go_assembly_test_InputAlias(5)==18?0:1;}\n')
            run([compiler,'-O2','-std=c11' if target=='c' else '-std=c++17',
                 '-I'+str(native),*sorted(native.glob('*.'+suffix)),'-o',native/'run'])
            run([native/'run'])
            records.append(dict(form=form,target=target,memory_alias=True))
    # Test-only Go modules can keep their records in *_test.go: companion
    # signatures contain only primitive ABI values, not those test-only types.
    suite = WORK/'suite'; suite.mkdir(exist_ok=True)
    (suite/'suite_test.zi').write_text('''native :: #system_library "go:testing";
Test :: #type #foreign native "T";
Fatal :: (test: *Test, message: string) #foreign native "(*T).Fatal";
Box :: struct { value: s32 }
Marker :: () -> s32 #asm("x86_64", "movl $7, %[out]", "=r", "", "", "") #asm("aarch64", "mov %w[out], #7", "=r", "", "", "") { return 3 }
Boxed :: () -> Box #asm("x86_64", "movl $7, %[out_value]", "=r", "", "", "") #asm("aarch64", "mov %w[out_value], #7", "=r", "", "", "") { return Box.{3} }
#program_export "TestAssembly"
Check :: (test: *Test) { if Boxed().value != Marker() { Fatal(test, "native test record") } }
''')
    run([ZIRAN,'build','--target=go','--pkg','suite','--root',suite,'-o',WORK/'suite-go',suite/'suite_test.zi'])
    assert (WORK/'suite-go/suite_test.go').is_file()
    for cgo in ('0','1'):
        run(['go','test','.'],cwd=WORK/'suite-go',env=dict(ENV,CGO_ENABLED=cgo,GOFLAGS='-tags=ziran_asm' if cgo == '1' else ''))
        records.append(dict(native_test_module=True,cgo=cgo))
    collision = WORK/'collision'; collision.mkdir(exist_ok=True)
    (collision/'collision.zi').write_text('''F :: (__ziranAssembly0_0_0: s32) -> s32 #asm("x86_64", "movl %[__ziranAssembly0_0_0], %[out]", "=r", "r", "", "") { return __ziranAssembly0_0_0 }
G :: (zirGoAssemblyEnabled: s32, unsafe: *s32) -> s32 #asm("x86_64", "movl (%[unsafe]), %[out]\\n\\taddl %[zirGoAssemblyEnabled], %[out]", "=&r", "r;r", "memory", "") { return zirGoAssemblyEnabled + unsafe.* }
H :: (zirGoAssemblyEnabled: s32, unsafe: *s32) -> s32 {
    result: s32
    #asm("x86_64", "movl (%[unsafe]), %[out]\\n\\taddl %[zirGoAssemblyEnabled], %[out]", "=&r:out=result", "r:zirGoAssemblyEnabled;r:unsafe", "memory", "") { result = zirGoAssemblyEnabled + unsafe.* }
    return result
}
''')
    run([ZIRAN,'build','--target=go','--pkg','main','--root',collision,'-o',WORK/'collision-go',collision/'collision.zi'])
    (WORK/'collision-go/main.go').write_text('package main\nfunc main(){v:=int32(25);if Collision_F(42)!=42 || Collision_G(17,&v)!=42 || Collision_H(17,&v)!=42 {panic("assembly name collision")}}\n')
    run(['go','run','.'],cwd=WORK/'collision-go',env=dict(ENV,CGO_ENABLED='1',GOFLAGS='-tags=ziran_asm'))
    records.append(dict(local_name_collision=True))
    # Removing assembly prunes all owned companions, preserving an owner's file.
    prune=WORK/'prune';prune.mkdir(exist_ok=True)
    file=prune/'prune.zi'
    file.write_text('F :: () -> s32 #asm("x86_64", "movl $42, %[out]", "=r", "", "", "") { return 42 }\n')
    output=WORK/'prune-go'
    command=[ZIRAN,'build','--target=go','--pkg','main','--prune-stale','--root',prune,'-o',output,file]
    run(command)
    owned={p.name for p in output.glob('*assembly*')}|{'zi2go.assembly_enabled.go','zi2go.assembly_disabled.go'}
    assert owned
    (output/'owner.go').write_text('package main\nfunc main(){if Prune_F()!=42{panic("pruned assembly")}}\n')
    file.write_text('F :: () -> s32 { return 42 }\n');run(command)
    assert not any((output/name).exists() for name in owned)
    run(['go','run','.'],cwd=output,env=dict(ENV,CGO_ENABLED='0'))
    records.append(dict(owned_companions_pruned=True))
    (WORK/'verification.json').write_text(json.dumps(dict(records=records,
        sources={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}),indent=2)+'\n')
    print('Go assembly passed native/portable dispatch, strict cgo pointer checks, aliases, source/saved IR and concurrent calls')


if __name__ == '__main__': main()
