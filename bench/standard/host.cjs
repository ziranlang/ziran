// Only the Wasm ABI and clock live in JS; the workload is the same ZX bundle.
const fs=require('node:fs');
(async()=>{
 const [file,kind,size,samples,expected]=process.argv.slice(2);
 if(samples!=='11'&&samples!=='0')throw Error('expected 11 samples or 0 for verification');
 const values=[BigInt(kind),BigInt(size)];
 const budget=new WebAssembly.Global({value:'i64',mutable:true},9223372036854775807n);
 const {instance}=await WebAssembly.instantiate(fs.readFileSync(file),{zx:{budget,
  overrun:()=>{throw Error('budget exceeded')},
  host:(cap,record)=>{const view=new DataView(instance.exports.memory.buffer);const key=view.getBigInt64(record,true);if(cap!==0||key<0n||key>1n)throw Error("invalid input");view.setBigInt64(record+16,values[Number(key)],true);return 1},
  host_i64:(cap,key)=>{if(cap!==0||key<0n||key>1n)throw Error('invalid input');return values[Number(key)]}
 }});
 const call=()=>{budget.value=9223372036854775807n;return instance.exports.zx_entry().toString()};
 if(call()!==expected)throw Error('C oracle mismatch');
 if(samples==='0')return;
 let start=performance.now();do{if(call()!==expected)throw Error('C oracle mismatch')}while(performance.now()-start<150);
 const times=[];for(let i=0;i<11;i++){start=performance.now();const value=call();times.push(performance.now()-start);if(value!==expected)throw Error(`C oracle mismatch: ${value} != ${expected}`)}
 console.log(JSON.stringify({result:expected,batches:1,batch_samples_ms:times}));
})().catch(e=>{console.error(e);process.exitCode=1});
