#!/usr/bin/env python3
"""Run on Alpha Personal inside build-queue. Fetch pinned, unmodified release packages."""
import argparse, hashlib, json, os, re, subprocess, sys, tarfile, time, urllib.request, zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
VERSION='2.4.5'
SOURCES=[('WAMR-2.4.5.tar.gz','https://github.com/wasm-micro-runtime/wasm-micro-runtime/archive/refs/tags/WAMR-2.4.5.tar.gz','1ab09d51099f276ca4a1d6629f6b589aab2bd0caa01445e05031a4bed22c199b',''),('wamrc-2.4.5.tar.gz','https://github.com/wasm-micro-runtime/wasm-micro-runtime/releases/download/WAMR-2.4.5/wamrc-2.4.5-x86_64-ubuntu-22.04.tar.gz','fedda99950a70f7dc45470ce88e12bf9e435b9482058ae97ce2cadd760e2329f','wamrc-2.4.5')]
def sha(p):
    h=hashlib.sha256()
    with open(p,'rb') as f:
        for b in iter(lambda:f.read(65536),b''):h.update(b)
    return h.hexdigest()
def main():
    parser=argparse.ArgumentParser();parser.add_argument('--android',action='store_true');a=parser.parse_args()
    if not os.environ.get('BUILD_QUEUE_JOBS'):raise RuntimeError('Run this entire script through build-queue')
    deps=ROOT/'build/deps';deps.mkdir(parents=True,exist_ok=True)
    for name,url,digest,subdir in SOURCES:
        archive=deps/name
        if not archive.exists():urllib.request.urlretrieve(url,archive)
        assert sha(archive)==digest,name
        dest=deps/subdir
        marker=(dest/'wamrc') if subdir else deps/'wasm-micro-runtime-WAMR-2.4.5/LICENSE'
        if not marker.exists():
            dest.mkdir(parents=True,exist_ok=True)
            with tarfile.open(archive) as t:t.extractall(dest,filter='data')
    out=ROOT/'build/benchmarks'/('zx-compare-android' if a.android else 'zx-compare')
    obj=ROOT/'build/benchmarks'/('zx-compare-apk' if a.android else 'zx-compare-embedded')
    out.mkdir(parents=True,exist_ok=True);commands=[]
    def run(argv):
        argv=list(map(str,argv));begin=time.monotonic();subprocess.run(argv,cwd=ROOT,check=True)
        commands.append({'argv':argv,'elapsed_ms':(time.monotonic()-begin)*1000})
    flags=[]
    if a.android:
        ndk=Path(os.environ.get('ANDROID_NDK_HOME',str(Path.home()/'Android/Sdk/ndk/android-ndk-r28c')))
        tools=ndk/"toolchains/llvm/prebuilt/linux-x86_64/bin"
        runtime="build/android-host/armeabi-v7a"
        archive=ROOT/runtime/"libziran.a";stamp=out/"pic-runtime.json"
        force=not archive.exists() or not stamp.exists() or json.loads(stamp.read_text())["sha256"]!=sha(archive)
        run(["make",*(["-B"] if force else []),"-j2","BOOTSTRAP=1","BUILD_DIR="+runtime,"CC="+str(tools/"armv7a-linux-androideabi28-clang"),"AR="+str(tools/"llvm-ar"),"OBJCOPY="+str(tools/"llvm-objcopy"),"CFLAGS=-O2 -fPIC -ffunction-sections -fdata-sections",runtime+"/libziran.a"])
        stamp.write_text(json.dumps({"sha256":sha(archive),"flags":"-O2 -fPIC -ffunction-sections -fdata-sections"}))
        flags=['-DCMAKE_TOOLCHAIN_FILE='+str(ndk/'build/cmake/android.toolchain.cmake'),'-DANDROID_ABI=armeabi-v7a','-DANDROID_PLATFORM=android-28']
    if not a.android:run(["make","-j2","build/libziran.a","build/bin/ziran","build/bin/zi2wasm"])
    cc=tools/"armv7a-linux-androideabi28-clang" if a.android else "cc"
    archive=ROOT/("build/android-host/armeabi-v7a/libziran.a" if a.android else "build/libziran.a")
    run([sys.executable,ROOT/"bench/zx_compare.py","prepare","--cc",cc,"--library",archive,"--out",out])
    run(['cmake','-S',ROOT/'bench/zx_compare_apk','-B',obj,'-DCMAKE_BUILD_TYPE=Release',*flags])
    run(['cmake','--build',obj,'-j'+str(min(2,int(os.environ['BUILD_QUEUE_JOBS'])))])
    target=['--target=armv7','--target-abi=eabi','--cpu=cortex-a9','--cpu-features=-neon'] if a.android else ['--target=x86_64','--cpu=x86-64']
    run([deps/'wamrc-2.4.5/wamrc','--opt-level=3',*target,'-o',out/'fixture.aot',out/'fixture.wasm'])
    if a.android:package(run,out,obj)
    data={'wamr_version':VERSION,'packages':SOURCES,'commands':commands,'aot_sha256':sha(out/'fixture.aot'),'source_revision':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),'sources':{str(p.relative_to(ROOT)):sha(p) for p in (ROOT/'bench/zx_compare_apk').glob('*') if p.is_file()}}
    cache=(obj/"CMakeCache.txt").read_text()
    compiler=re.search(r"^CMAKE_C_COMPILER:FILEPATH=(.+)$",cache,re.M)
    if compiler:data["compiler"]=subprocess.check_output([compiler[1],"--version"],text=True).splitlines()[0]
    library=ROOT/("build/android-host/armeabi-v7a/libziran.a" if a.android else "build/libziran.a")
    data["linked_archives"]={str(p.relative_to(ROOT)):sha(p) for p in (library,obj/"libwamr.a")}
    product=obj/("libzxcompare.so" if a.android else "zxcompare")
    data["product_sha256"]=sha(product)
    data["tinycc_version"]="0.9.28rc (vendored)"
    data["native_source_identity"]=(library.parent/".zx-native-identity").read_text().strip()
    data["runtime_compile_flags"]=(library.parent/".compiler-flags").read_text()[:3000]
    data["tools"]={name:sha(ROOT/"build/bin"/name) for name in ("ziran","zi2wasm")}
    if a.android:data["apk_sha256"]=sha(out/"zx-compare.apk")
    (out/'embedded-prepare.json').write_text(json.dumps(data,indent=2)+'\n')
def package(run,out,obj):
    apk=out/'zx-compare.apk';unsigned=out/'zx-compare-unsigned.apk'
    framework=Path('/usr/share/android-framework-res/framework-res.apk')
    run(['aapt','package','-f','-M',ROOT/'bench/zx_compare_apk/AndroidManifest.xml','-I',framework,'-F',unsigned])
    with zipfile.ZipFile(unsigned,'a',compression=zipfile.ZIP_DEFLATED) as z:
        z.write(obj/'libzxcompare.so','lib/armeabi-v7a/libzxcompare.so')
        z.write(ROOT/'build/deps/wasm-micro-runtime-WAMR-2.4.5/LICENSE','assets/WAMR-LICENSE')
        for name in ('fixture.zx','fixture.aot','component-old.zx','component-new.zx'):z.write(out/name,'assets/'+name)
    # Standard local Android debug identity, never part of maintained artifacts.
    key=Path.home()/'.android/debug.keystore';key.parent.mkdir(exist_ok=True)
    if not key.exists():
        run(['keytool','-genkeypair','-keystore',key,'-storepass','android','-keypass','android','-alias','androiddebugkey','-dname','CN=Android Debug,O=Android,C=US','-keyalg','RSA','-validity','10000'])
    run(['apksigner','sign','--ks',key,'--ks-pass','pass:android','--out',apk,unsigned])
    run(['apksigner','verify','--verbose',apk])
if __name__=='__main__':main()
