#!/usr/bin/env python3
"""USB-only interaction regression for the task-owned native Android fixture."""
import argparse,json,math,re,statistics,subprocess,sys,time
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from zx_compare import oracle,sha
from collect import PACKAGE,ROOT

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--serial',required=True);p.add_argument('--results',type=Path,required=True);a=p.parse_args()
    def adb(*args,**kw):return subprocess.run(['adb','-s',a.serial,*map(str,args)],check=True,capture_output=True,**kw).stdout
    def shell(*args):return adb('shell',*args,text=True).strip()
    def rows():return [json.loads(s) for s in shell('run-as',PACKAGE,'cat','files/visual.jsonl').splitlines()]
    selected=[s for s in subprocess.check_output(['adb','devices','-l'],text=True).splitlines() if s.split() and s.split()[0]==a.serial]
    assert len(selected)==1 and 'usb:' in selected[0] and selected[0].split()[1]=='device'
    assert shell('getprop','ro.product.model')=='moto e6 play'
    pid=shell('pidof',PACKAGE);assert pid.isdigit()
    initial=rows();assert initial[-1]['ready']==1 and initial[-1]['width']==720,initial[-1]
    actions=[];cold=[];warm=[]
    def checked_input(command,*args):
        before=rows()[-1];count=len(rows());shell('input',command,*args)
        after=rows()[count:];assert after,'No native input/draw receipt'
        assert shell('pidof',PACKAGE)==pid
        actions.append(dict(command=[command,*args],before=before,after=after))
        return before,after
    def select(engine):
        before,new=checked_input('tap',180 if engine=='native' else 530,185)
        assert new[-1]['event']=='select' and new[-1]['engine']==engine,new
        assert all(new[-1][k]==before[k] for k in ('taps','state','checksum'))
    def tap():
        before,new=checked_input('tap',360,395);last=new[-1]
        assert any(r['event']=='down' and r['pressed'] for r in new),new
        assert last['event']=='up' and not last['pressed'] and last['ready']==1,last
        assert last['taps']==before['taps']+1
        expected=oracle(123457+before['taps'],128,1)
        assert last['checksum']==expected and last['state']==expected%100000,(last,expected)
        return last
    # Retain the first lazy guest event separately from repeated interactions.
    for engine in ('native','wamr-aot'):
        select(engine)
        existing=[r for r in initial if r.get('event')=='up' and r.get('engine')==engine]
        cold.append(existing[0] if existing else tap())
    for trial in range(9):
        for engine in (('native','wamr-aot') if trial%2==0 else ('wamr-aot','native')):
            select(engine);r=tap();warm.append(dict(trial=trial,**r))
    before,new=checked_input('swipe',360,395,710,1000,300)
    assert any(r['event']=='cancel-drag' for r in new) and new[-1]['event']=='up-outside',new
    assert new[-1]['taps']==before['taps']
    # Drag into a selector from outside: never a click without matching DOWN.
    before,new=checked_input('swipe',100,280,180,185,300)
    assert new[-1]['event']=='up-outside' and new[-1]['engine']==before['engine']
    before,new=checked_input('swipe',180,185,530,185,300)
    assert any(r['event']=='cancel-drag' for r in new) and new[-1]['engine']==before['engine']
    assert new[-1]['taps']==before['taps']
    before=rows()[-1]
    shell('input','keyevent','KEYCODE_HOME')
    resumed=shell('am','start','-W','-n',PACKAGE+'/android.app.NativeActivity')
    after=rows()[-1]
    assert shell('pidof',PACKAGE)==pid and all(after[k]==before[k] for k in ('taps','state','checksum','engine'))
    allrows=rows()
    summary=[]
    for engine in ('native','wamr-aot'):
        summary.append(dict(engine=engine,n=9,**{metric:dict(median=statistics.median(values),p95=sorted(values)[-1]) for metric in ('input_to_buffer_post_ms','compute_ms','draw_post_ms') for values in [[r[metric] for r in warm if r['engine']==engine]]}))
    native=shell('run-as',PACKAGE,'cat','files/native.log')
    receipt=dict(complete=True,external_oracle_verified=True,pid=int(pid),initial=initial,actions=actions,initial_guest_interactions=cold,warm_interactions=warm,summary=summary,rows=allrows,native_evidence=native,background_resume=dict(launch=resumed,before=before,after=after),apk_sha256=sha(ROOT/'build/benchmarks/zx-compare-android/zx-compare.apk'),completed_utc=time.strftime('%Y-%m-%dT%H:%M:%SZ',time.gmtime()),limits=['ADB synthetic touch event to host buffer post, not physical input-to-photon','Native Android Bitmap/Canvas fixture, not Kryon renderer parity','Host state preserved across guest engine selection; no Wasm component migration','Drag cancellation verified; no separate hardware ACTION_CANCEL or multi-touch injection'])
    a.results.parent.mkdir(parents=True,exist_ok=True);a.results.write_text(json.dumps(receipt,indent=2)+chr(10));print(json.dumps(summary,indent=2))
if __name__=='__main__':main()
