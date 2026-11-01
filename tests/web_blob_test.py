"""Exercise the actual web bridge with native browser Blob/memory behavior.

No Node or desktop display. Chromium runs in its own network/user namespace,
with a short private profile path and a read-only filesystem outside scratch.
"""
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
CHROME = shutil.which('chromium') or shutil.which('chromium-browser')
assert CHROME and shutil.which('bwrap'), 'Chromium and bubblewrap are required'
subprocess.run(['git', 'check-ignore', '-q', 'work/web-blob-test'], cwd=ROOT, check=True)
WORK = ROOT / 'work/web-blob-test'
WORK.mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory(dir=WORK) as directory:
    directory = Path(directory)
    private_tmp = directory / 'tmp'
    private_tmp.mkdir()
    (private_tmp / 'home').mkdir()
    (private_tmp / 'run').mkdir(mode=0o700)
    bridge = (ROOT / 'web/ziran_web.js').read_text()
    checks = r'''
globalThis.Module = {};
globalThis.ZiranWeb = bridge.$ZiranWeb;
const memory = new WebAssembly.Memory({initial:1, maximum:3});
globalThis.HEAPU8 = new Uint8Array(memory.buffer);
HEAPU8.set([11,22,33,44],32);
const originalSlice = ArrayBuffer.prototype.slice;
let blob;
try {
  ArrayBuffer.prototype.slice = function(...args) {
    if(this === memory.buffer) throw Error('Redundant Wasm byte copy');
    return originalSlice.apply(this,args);
  };
  blob = ZiranWeb().value(bridge.js_web_blob_bytes(32,4));
} finally { ArrayBuffer.prototype.slice = originalSlice; }
if(!(blob instanceof Blob) || blob.size !== 4) throw Error('Blob handle/size');
const copied = ZiranWeb().value(bridge.js_web_bytes(0,32,4));
HEAPU8[32] = 99;
if(copied[0] !== 11) throw Error('WebBytes lost copy semantics');
memory.grow(1);
globalThis.HEAPU8 = new Uint8Array(memory.buffer);
const actual = [...new Uint8Array(await blob.arrayBuffer())];
if(JSON.stringify(actual) !== '[11,22,33,44]') throw Error('Blob changed across mutation/growth');
for(const [pointer,count] of [[-1,1],[0,-1],[0,HEAPU8.length+1],
                            [HEAPU8.length,1],[0.5,1],[0,1.5],
                            [Number.MAX_SAFE_INTEGER,1]]) {
  if(bridge.js_web_blob_bytes(pointer,count) !== 0) throw Error('Invalid range accepted');
}
const empty = ZiranWeb().value(bridge.js_web_blob_bytes(HEAPU8.length,0));
if(empty.size !== 0) throw Error('Empty boundary range rejected');
const last = ZiranWeb().value(bridge.js_web_blob_bytes(HEAPU8.length-1,1));
if(last.size !== 1) throw Error('Last valid byte rejected');
const wide = ZiranWeb().value(bridge.js_web_blob_bytes(32n,4));
if(wide.size !== 4) throw Error('Memory64 pointer conversion');
document.getElementById('result').textContent = 'WEB_BLOB_PASS';
'''
    html = ('<!doctype html><pre id="result">PENDING</pre><script>'
            'globalThis.addToLibrary = value => {globalThis.bridge = value};\n'
            + bridge + '\n(async()=>{' + checks + '})().catch(error => '
            'document.getElementById("result").textContent="FAIL: "+error.message);</script>')
    page = directory / 'test.html'
    page.write_text(html)
    environment = dict(os.environ, HOME='/tmp/home', YUE_DESKTOP_RECOVERY='0')
    for key in ('DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'DBUS_SESSION_BUS_ADDRESS', 'SESSION_MANAGER'):
        environment.pop(key, None)
    command = ['bwrap', '--unshare-all', '--die-with-parent', '--ro-bind', '/', '/',
               '--dev', '/dev', '--proc', '/proc', '--bind', str(private_tmp), '/tmp',
               '--setenv', 'HOME', '/tmp/home', '--setenv', 'TMPDIR', '/tmp',
               '--setenv', 'XDG_CONFIG_HOME', '/tmp/config', '--setenv', 'XDG_CACHE_HOME', '/tmp/cache',
               '--setenv', 'XDG_RUNTIME_DIR', '/tmp/run',
               CHROME, '--headless', '--no-sandbox', '--disable-gpu',
               '--disable-dev-shm-usage', '--disable-background-networking',
               '--disable-breakpad', '--disable-crash-reporter', '--no-first-run',
               '--user-data-dir=/tmp/profile', '--virtual-time-budget=2000', '--dump-dom', page.as_uri()]
    result = subprocess.run(command, env=environment, text=True, capture_output=True, timeout=30)
    found = re.search(r'<pre id="result">([^<]*)</pre>', result.stdout)
    assert result.returncode == 0 and found and found[1] == 'WEB_BLOB_PASS', (
        result.returncode, found[1] if found else result.stdout[-1500:], result.stderr[-1500:])
print('local browser test: Blob snapshot without redundant copy, source mutation/memory growth, '
      'WebBytes copy semantics, Memory64 pointer and invalid ranges passed')
