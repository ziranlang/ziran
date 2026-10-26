#!/usr/bin/env python3
"""Source/saved IR Zstandard stream interop, reuse and bounded rejection."""
import ctypes as C
import ctypes.util
import os
from pathlib import Path
import random
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
compiler = str(Path(sys.argv[1]).resolve())
env = os.environ.copy()
for key in ("DISPLAY", "WAYLAND_DISPLAY", "XAUTHORITY", "DBUS_SESSION_BUS_ADDRESS"):
    env.pop(key, None)
env.setdefault("GOMAXPROCS", "1")
env.setdefault("GOFLAGS", "-p=1")
lib = C.CDLL(ctypes.util.find_library("zstd"))
lib.ZSTD_compressBound.argtypes=[C.c_size_t];lib.ZSTD_compressBound.restype=C.c_size_t
lib.ZSTD_compress.argtypes=[C.c_void_p,C.c_size_t,C.c_void_p,C.c_size_t,C.c_int];lib.ZSTD_compress.restype=C.c_size_t
lib.ZSTD_decompress.argtypes=[C.c_void_p,C.c_size_t,C.c_void_p,C.c_size_t];lib.ZSTD_decompress.restype=C.c_size_t
lib.ZSTD_isError.argtypes=[C.c_size_t];lib.ZSTD_isError.restype=C.c_uint

with tempfile.TemporaryDirectory(prefix="zstd-go-",dir=root/"build") as temporary:
    work=Path(temporary)
    plain=random.Random(71023).randbytes(262144)
    native=C.create_string_buffer(lib.ZSTD_compressBound(len(plain)))
    size=lib.ZSTD_compress(native,len(native),plain,len(plain),1)
    assert not lib.ZSTD_isError(size)
    (work/"native.zst").write_bytes(native.raw[:size]);(work/"plain.bin").write_bytes(plain)
    flags=["--root",str(root/"tests"),"--module-path",f"std={root/'std'}"]
    source=root/"tests/zstd_go_fixture.zi"
    subprocess.run([compiler,"ir",*flags,"-o",str(work/"ir"),str(source)],cwd=root,env=env,check=True)
    for mode in ("source","saved"):
        output=work/mode
        inputs=[*flags,str(source)] if mode=="source" else ["--root",str(work/"ir"),"--module-path",f"std={root/'std'}",str(work/"ir/zstd_go_fixture.zir")]
        subprocess.run([compiler,"build","--target=go","--no-main","--pkg","main","-o",str(output),*inputs],cwd=root,env=env,check=True)
        (output/"go.mod").write_text('module zstd-test\n\ngo 1.25\n\nrequire github.com/klauspost/compress v1.20.3\n')
        (output/"stream_test.go").write_text(r'''package main
import("bytes";"encoding/base64";"math/rand";"os";"testing")
func TestStream(t *testing.T){
 plain,err:=os.ReadFile("../plain.bin");if err!=nil{t.Fatal(err)}
 native,err:=os.ReadFile("../native.zst");if err!=nil{t.Fatal(err)}
 got:=ZstdGoFixture_Decode(native,int64(len(plain)),8<<20)
 if got.Error!=nil||!bytes.Equal(got.Value,plain){t.Fatal("native frame",got.Error)}
 for _,bad:=range [][]byte{nil,[]byte("broken"),native[:len(native)-1],append(append([]byte(nil),native...),0)}{
  if v:=ZstdGoFixture_Decode(bad,int64(len(plain)),8<<20);v.Error==nil{t.Fatal("malformed stream accepted")}
 }
 for _,bound:=range []int64{-1,0,int64(len(plain)-1)}{
  v:=ZstdGoFixture_Decode(native,bound,8<<20);if v.Error==nil||len(v.Value)!=0{t.Fatal("decoded bound",bound)}
 }
 var output bytes.Buffer
 opened:=ZstdGoFixture_NewEncoder(&output,1<<20);if opened.Error!=nil{t.Fatal(opened.Error)}
 raw:=make([]byte,3<<20);rand.New(rand.NewSource(407)).Read(raw)
 text:=[]byte(base64.RawURLEncoding.EncodeToString(raw))
 for _,data:=range [][]byte{text,[]byte("a different record")}{
  output.Reset();ZstdGoFixture_Reset(opened.Value,&output)
  if _,err:=opened.Value.Write(data);err!=nil{t.Fatal(err)}
  if err:=ZstdGoFixture_Close(opened.Value);err!=nil{t.Fatal(err)}
  if len(data)>1000&&output.Len()>len(data)*80/100{t.Fatal("literal entropy disabled")}
  v:=ZstdGoFixture_Decode(output.Bytes(),int64(len(data)),8<<20)
  if v.Error!=nil||!bytes.Equal(v.Value,data){t.Fatal("reset roundtrip",v.Error)}
 }
 if bad:=ZstdGoFixture_NewEncoder(&output,999);bad.Error==nil{t.Fatal("invalid window accepted")}
 if v:=ZstdGoFixture_Decode(native,int64(len(plain)),1024);v.Error==nil{t.Fatal("window bound ignored")}
 output.Reset();ZstdGoFixture_Reset(opened.Value,&output);opened.Value.Write(plain);ZstdGoFixture_Close(opened.Value)
 if err:=os.WriteFile("go.zst",output.Bytes(),0600);err!=nil{t.Fatal(err)}
}
''')
        subprocess.run(["go","test","-mod=mod","-race","."],cwd=output,env=env,check=True)
        encoded=(output/"go.zst").read_bytes()
        decoded=C.create_string_buffer(len(plain))
        size=lib.ZSTD_decompress(decoded,len(decoded),encoded,len(encoded))
        assert size==len(plain) and decoded.raw==plain
    print("Zstandard source/saved IR: native interop, literal entropy, reset, limits and malformed rejection passed")
