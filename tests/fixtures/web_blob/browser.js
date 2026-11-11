
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
