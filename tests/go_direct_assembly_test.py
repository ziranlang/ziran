"""Validate native Go ABI0, hardware guards and checked fallback selection."""
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
ZIRAN, WORK = map(lambda p: Path(p).resolve(), sys.argv[1:])
ENV = dict(os.environ, GO111MODULE='off')
for key in ('DISPLAY','WAYLAND_DISPLAY','XAUTHORITY','DBUS_SESSION_BUS_ADDRESS',
            'GOFLAGS','GOEXPERIMENT','CGO_CFLAGS','CGO_CPPFLAGS'):
    ENV.pop(key,None)
WORK.mkdir(parents=True,exist_ok=True)
ENV['GOCACHE'] = str(WORK/'go-cache')

def run(args,cwd=ROOT,env=ENV):
    p=subprocess.run(list(map(str,args)),cwd=cwd,env=env,capture_output=True,text=True,timeout=180)
    if p.returncode:raise RuntimeError(str(args)+'\n'+p.stdout+p.stderr)
    return p.stdout

HOST = '''package main
import ("math"; "runtime"; "sync")
func require(value bool){if !value{panic("direct Go ABI conformance")}}
func grow(depth int) int {var words [128]int; words[depth]=depth; if depth>0{return words[depth]+grow(depth-1)}; return words[0]}
func main(){
 require(GoDirectAssemblyTest_Marker()==MARKER)
 require(GoDirectAssemblyTest_Unknown()==UNKNOWN && GoDirectAssemblyTest_Priority()==UNKNOWN)
 require(GoDirectAssemblyTest_Reserved()==UNKNOWN && GoDirectAssemblyTest_Stack()==UNKNOWN)
 values:=[]int64{math.MinInt64,-2147483649,-32769,-129,-1,0,1,127,32767,2147483647,math.MaxInt64}
 for _,left:=range values {for _,right:=range values {
  require(GoDirectAssemblyTest_Compare(left,right)==(left<right))
  require(GoDirectAssemblyTest_Subtract(left,right)==left-right && GoDirectAssemblyTest_Multiply(left,right)==left*right)
 }}
 for count:=uint64(0);count<18;count++{require(GoDirectAssemblyTest_Labels(count)==count*(count+1)/2)}
 for _,value:=range values {
  p:=GoDirectAssemblyTest_CopySigned(int8(value),int16(value),int32(value),value)
  require(p.Byte==int8(value)&&p.Word==int16(value)&&p.Integer==int32(value)&&p.Wide==value)
  require(GoDirectAssemblyTest_Retained(value)==value)
  require(GoDirectAssemblyTest_Alternative(uint64(value))==uint64(value)+7)
 }
 for _,bits:=range []uint64{0,1,0x8000000000000000,0x7ff8123456789abc,0xffffffffffffffff} {
  single:=math.Float32frombits(uint32(bits)); real:=math.Float64frombits(bits)
  p:=GoDirectAssemblyTest_Copy(uint8(bits),uint16(bits),uint32(bits),bits,bits!=0,single,real)
  require(p.Byte==uint8(bits)&&p.Word==uint16(bits)&&p.Integer==uint32(bits)&&p.Wide==bits&&p.Flag==(bits!=0))
  require(math.Float32bits(p.Single)==math.Float32bits(single)&&math.Float64bits(p.Real)==math.Float64bits(real))
  for _,divisor:=range []uint64{1,3,17,0xffffffffffffffff}{p:=GoDirectAssemblyTest_Divide(bits,divisor);require(p.Low==bits/divisor&&p.High==bits%divisor)}
 }
 var wait sync.WaitGroup
 for worker:=0;worker<4;worker++{wait.Add(1);go func(){defer wait.Done();for pass:=0;pass<80;pass++{
  require(grow(80)==3240&&GoDirectAssemblyTest_Feature(^uint64(0))==64)
  require(GoDirectAssemblyTest_Compare(-1,1)); if pass%10==0{runtime.GC()}
 }}()}; wait.Wait()
}
'''

def main():
    if not shutil.which('go'):
        print('Go unavailable; direct Go assembly not executed');return
    source=ROOT/'tests/spec/go_direct_assembly_test.zi'
    run([ZIRAN,'ir','--root',source.parent,'-o',WORK/'ir',source])
    expected_features=None
    if platform.machine()=='x86_64':
        features=['popcnt','sse2','ssse3','sse4.1','sse4.2','avx','avx2','avx512f','bmi','bmi2','lzcnt','fma','aes','pclmul']
        c=WORK/'cpu.c';c.write_text('#include <stdio.h>\nint main(void){unsigned long long flags=0;__builtin_cpu_init();'+''.join('if(__builtin_cpu_supports("'+f+'"))flags|=1ULL<<'+str(i)+';' for i,f in enumerate(features))+'printf("%llu",flags);}\n')
        run([os.environ.get('CC','cc'),'-O2',c,'-o',WORK/'cpu'])
        expected_features=int(run([WORK/'cpu']))
    artifacts={};records=[]
    for form in ('source','saved'):
        path=source if form=='source' else WORK/'ir'/source.with_suffix('.zir').name
        out=WORK/form
        run([ZIRAN,'build','--target=go','--pkg','main','--root',path.parent,'-o',out,path])
        artifacts[form]={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in out.iterdir()
                         if p.suffix in ('.go','.s','.h') and p.name not in ('main.go','probe.go')}
        code='\n'.join(p.read_text() for p in out.glob('*_ziran.go'))
        def site(name):
            start=code.index('func GoDirectAssemblyTest_'+name+'(')
            end=code.find('\nfunc ',start+5)
            body=code[start:] if end<0 else code[start:end]
            found=re.search(r'zirGoAssemblyEnabled && (\w+) != nil',body)
            assert found,name
            return found[1]
        for mode,cgo,tags in [('ordinary','0',False),('direct','0',True),('cgo','1',True),('forced','1',True)]:
            env=dict(ENV,CGO_ENABLED=cgo)
            if tags:env['GOFLAGS']='-tags=ziran_asm'
            if mode=='forced':env['CGO_CFLAGS']='-O2 -DZIRAN_NO_ASM'
            native=platform.machine()=='x86_64' and mode in ('direct','cgo')
            marker=7 if native else 3;unknown=99 if native and mode=='cgo' else 3
            (out/'main.go').write_text(HOST.replace('MARKER',str(marker)).replace('UNKNOWN',str(unknown)))
            if mode=='direct' and expected_features is not None:
                text='\n'.join(p.read_text() for p in out.glob('*.assembly_direct_amd64.go'))
                detector=re.search(r'var (\w+Features) = func\(\) uint64',text)[1]
                checks=['if '+detector+' != '+str(expected_features)+' {panic("CPU feature predicates")}']
                checks+=['if '+site(name)+' == nil {panic("missing direct '+name+'")} ' for name in ['Marker','Compare','Subtract','Multiply','Labels','Copy','CopySigned','Retained','Divide','Alternative']]
                checks+=['if '+site(name)+' != nil {panic("unsupported direct '+name+'")} ' for name in ['Unknown','Priority','Reserved','Stack']]
                (out/'probe.go').write_text('package main\nfunc init(){'+';'.join(checks)+'}\n')
            run(['go','build','-o',out/'run','.'],cwd=out,env=env)
            run([out/'run'],env=dict(env,GOGC='1'))
            if mode=='direct':run(['go','vet','-asmdecl','.'],cwd=out,env=env)
            (out/'probe.go').unlink(missing_ok=True)
            records.append(dict(form=form,mode=mode,marker=marker,unsupported_marker=unknown))
        if form=='source':
            for target,arch in [('windows','amd64'),('darwin','amd64'),('linux','arm64'),('linux','386')]:
                env=dict(ENV,GOOS=target,GOARCH=arch,CGO_ENABLED='0',GOFLAGS='-tags=ziran_asm')
                (out/'main.go').write_text(HOST.replace('MARKER','7' if arch=='amd64' else '3').replace('UNKNOWN','3'))
                run(['go','build','-o',out/('cross-'+target+'-'+arch),'.'],cwd=out,env=env)
                records.append(dict(cross_built=True,os=target,arch=arch))
    assert artifacts['source']==artifacts['saved']
    (WORK/'verification.json').write_text(json.dumps(dict(records=records,source_saved_identical=True,cpu_features=expected_features),indent=2)+'\n')
    print('Direct Go ABI passed mixed scalar layouts, fixed/tied/alternative operands, CPU predicates, fallback priority, GC/stack stress and cross builds')

if __name__=='__main__':main()
