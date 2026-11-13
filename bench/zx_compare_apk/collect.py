#!/usr/bin/env python3
"""Collect real embedded-engine trials; Android mode performs USB transport only."""
import argparse, hashlib, json, math, os, platform, re, signal, statistics, subprocess, sys, time
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from zx_compare import oracle, sha
ROOT=Path(__file__).resolve().parents[2]
PACKAGE='org.ziranlang.benchmarks.zxcompare'
def command(args,**kw):return subprocess.run(list(map(str,args)),check=True,text=True,capture_output=True,**kw).stdout.strip()
def validate(rows):
    assert rows[-1]=={'complete':True},rows[-1]
    samples=[r for r in rows if 'variant' in r]
    assert len(samples)==72,len(samples)
    for r in samples:
        expected=sum(oracle(r['seed']+j,r['rounds'],r['kind']) for j in range(r['batches']))
        assert r['checksum']==expected,(r,expected)
        assert r['warmup_ms']>=500
        r['per_entry_ms']=r['warm_ms']/r['batches']
    components=[r for r in rows if 'state_rollback_stale_verified' in r]
    assert len(components)==9 and all(r['state_rollback_stale_verified'] for r in components)
    summary=[]
    for kind in range(2):
        for variant in ('vm','native','c','wamr-aot'):
            values=sorted(r['per_entry_ms'] for r in samples if r['kind']==kind and r['variant']==variant)
            assert len(values)==9
            summary.append(dict(kind=kind,variant=variant,n=9,median_ms=statistics.median(values),p95_ms=values[math.ceil(.95*len(values))-1]))
    return samples,components,summary
def provenance(out,binary):
    return {'embedded_prepare':json.loads((out/'embedded-prepare.json').read_text()),'zx_prepare':json.loads((out/'prepare.json').read_text()),'binary_sha256':sha(binary),'fixture_sha256':{n:sha(out/n) for n in ('fixture.zx','fixture.wasm','fixture.aot','component-old.zx','component-new.zx')}}
def desktop(a):
    out=ROOT/'build/benchmarks/zx-compare';obj=ROOT/'build/benchmarks/zx-compare-embedded';raw=obj/'results.jsonl';binary=obj/'zxcompare'
    virtualization=command(['systemd-detect-virt'])
    assert platform.node()=='alpha' and virtualization=='none','Alpha physical host required'
    before={'cpu':Path('/proc/cpuinfo').read_text()[:3500],'platform':platform.platform(),'load':os.getloadavg(),'virtualization':virtualization}
    env=dict(os.environ,ZIRAN_NATIVE_TIMING='1',ZIRAN_NATIVE_CACHE=str(out/'embedded-cache'))
    for key in ('DISPLAY','WAYLAND_DISPLAY','XAUTHORITY','DBUS_SESSION_BUS_ADDRESS'):env.pop(key,None)
    env['YUE_DESKTOP_RECOVERY']='0'
    result=subprocess.run([str(binary),str(out),str(raw),'123456','4096','32','9'],env=env,text=True,capture_output=True,check=True)
    native=result.stderr;rows=[json.loads(s) for s in raw.read_text().splitlines()]
    receipt=dict(target='Alpha physical host',before=before,after={'load':os.getloadavg()},provenance=provenance(out,binary),rows=rows,native_evidence=native[-12000:])
    return receipt
def android(a):
    assert a.serial,'--serial must identify the authorized USB Moto'
    def adb(*args):return command(['adb','-s',a.serial,*args])
    def shell(*args):return adb('shell',*args)
    assert adb('get-state')=='device'
    selected=[s for s in command(['adb','devices','-l']).splitlines() if s.split() and s.split()[0]==a.serial]
    assert len(selected)==1 and 'usb:' in selected[0] and selected[0].split()[1]=='device','USB device required'
    identity={k:shell('getprop',k) for k in ('ro.product.model','ro.product.cpu.abi','ro.build.version.sdk','ro.build.version.release')}
    assert identity['ro.product.model']=='moto e6 play' and identity['ro.product.cpu.abi']=='armeabi-v7a' and identity['ro.build.version.sdk']=='28',identity
    out=ROOT/'build/benchmarks/zx-compare-android';apk=out/'zx-compare.apk'
    before={'battery':shell('dumpsys','battery'),'cpu':re.sub(r'(?m)^Serial.*$','',shell('cat','/proc/cpuinfo'))}
    temp=re.search(r'temperature: ([0-9]+)',before['battery']);assert temp and int(temp[1])<400
    install=adb('install','-r',apk)
    shell('am','force-stop',PACKAGE) # This task-owned package only; guarantee a fresh test process.
    start_at=shell('date','+%m-%d_%H:%M:%S.000').replace('_',' ')
    started=time.monotonic();launch=shell('am','start','-W','-n',PACKAGE+'/android.app.NativeActivity','--ez','benchmark','true')
    launch_ms=(time.monotonic()-started)*1000
    pid=shell('pidof',PACKAGE);assert pid.isdigit(),pid
    log=subprocess.Popen(['adb','-s',a.serial,'logcat','--pid',pid,'-v','brief','-T',start_at,'ZxCompare:I','*:S'],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
    observations=[];lines=[];lifecycle=[]
    def timeout(*unused):raise TimeoutError('APK benchmark did not finish within 180 seconds')
    previous=signal.signal(signal.SIGALRM,timeout);signal.alarm(180)
    try:
        for line in log.stdout:
            lines.append(line.strip())
            if 'progress trial=' in line:
                battery=shell('dumpsys','battery');temperature=re.search(r'temperature: ([0-9]+)',battery)
                observations.append({'progress':line.strip(),'battery':battery})
                if a.lifecycle and not lifecycle:
                    shell('input','keyevent','KEYCODE_BACK')
                    relaunched=shell('am','start','-W','-n',PACKAGE+'/android.app.NativeActivity','--ez','benchmark','true')
                    assert shell('pidof',PACKAGE)==pid,'Activity recreation restarted the benchmark process'
                    visual=[json.loads(s) for s in shell('run-as',PACKAGE,'cat','files/visual.jsonl').splitlines()]
                    assert visual and all(r.get('ready')==0 and r.get('taps')==0 for r in visual),visual
                    lifecycle.append(dict(event='Activity finish/recreate during compute; same PID; guest disabled',after_progress=line.strip(),launch=relaunched,visual=visual))
                if temperature and int(temperature[1])>=400:
                    shell('am','force-stop',PACKAGE)
                    raise RuntimeError('Moto reached 40C; own benchmark stopped')
            if 'finished seed=' in line:
                assert 'status=0' in line,line
                break
        else:raise RuntimeError('logcat ended before native benchmark finished')
    finally:
        signal.alarm(0);signal.signal(signal.SIGALRM,previous);log.terminate();log.communicate(timeout=10)
    rows=[json.loads(s) for s in shell('run-as',PACKAGE,'cat','files/results.jsonl').splitlines()]
    native=shell('run-as',PACKAGE,'cat','files/native.log')
    loaded=shell('run-as',PACKAGE,'sha256sum','files/fixture.zx','files/fixture.aot','files/component-old.zx','files/component-new.zx')
    for line in loaded.splitlines():
        digest,name=line.split();assert digest==sha(out/Path(name).name)
    package_info=shell('dumpsys','package',PACKAGE)
    return dict(target='USB Moto native Android APK single process',device=identity,before=before,after={'battery':shell('dumpsys','battery')},thermal_observations=observations,rows=rows,native_evidence=native[-12000:],provenance=provenance(out,apk),lifecycle=lifecycle,app={'package':PACKAGE,'pid':int(pid),'launch_ms_observed':launch_ms,'launch':launch,'install':install,'loaded_sha256':loaded,'debuggable':'DEBUGGABLE' in package_info,'native_log':lines,'package_info_selected':[s.strip() for s in package_info.splitlines() if any(k in s for k in ('versionName=','versionCode=','primaryCpuAbi=','flags=['))]})
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('target',choices=['desktop','android']);p.add_argument('--serial');p.add_argument('--lifecycle',action='store_true');p.add_argument('--results',type=Path,required=True);a=p.parse_args()
    try:receipt=(desktop if a.target=="desktop" else android)(a)
    except Exception as error:
        detail=str(error).replace(a.serial,"<USB Moto>") if a.serial else str(error)
        failure={"complete":False,"target":a.target,"error":detail,"attempted_utc":time.strftime("%Y-%m-%dT%H:%M:%SZ",time.gmtime())}
        if a.target=="android" and a.serial:
            for name in ("results.jsonl","native.log"):
                attempt=subprocess.run(["adb","-s",a.serial,"shell","run-as",PACKAGE,"cat","files/"+name],text=True,capture_output=True,timeout=10)
                failure[name]=attempt.stdout[-24000:]
        a.results.parent.mkdir(parents=True,exist_ok=True)
        a.results.write_text(json.dumps(failure,indent=2)+chr(10))
        raise
    receipt['samples'],receipt['component_samples'],receipt['summary']=validate(receipt['rows'])
    markers=[s for s in receipt["native_evidence"].splitlines() if s.startswith("ziran: native Answer: ")]
    native_rows=[r for r in receipt["samples"] if r["variant"]=="native"]
    assert len(markers)==len(native_rows)==18,"Missing real ZX native instantiation markers"
    for row,marker in zip(native_rows,markers):row["native_instantiation_evidence"]=marker
    receipt['complete']=True;receipt['external_oracle_verified']=True;receipt['completed_utc']=time.strftime('%Y-%m-%dT%H:%M:%SZ',time.gmtime())
    a.results.parent.mkdir(parents=True,exist_ok=True);a.results.write_text(json.dumps(receipt,indent=2)+'\n')
    print(json.dumps(receipt['summary'],indent=2))
if __name__=='__main__':main()
