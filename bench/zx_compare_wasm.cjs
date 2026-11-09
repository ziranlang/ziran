// Preliminary Node-only measurement; final comparison embeds WAMR natively.
async function zxCompare(bytes, seed, rounds, kind, batches, expected) {
  const now = () => performance.now();
  let started = now();
  const module = await WebAssembly.compile(bytes);
  const compiled = now();
  const inputs = [BigInt(seed), BigInt(rounds), BigInt(kind)];
  const budget = new WebAssembly.Global({value:'i64', mutable:true}, 1000000000n);
  const instance = await WebAssembly.instantiate(module, {zx: {budget,
    overrun: () => { throw Error('budget exceeded'); },
    host_i64: (cap, key) => {
      if(cap !== 0 || key < 0n || key > 2n) throw Error('invalid host call');
      return inputs[Number(key)];
    }}});
  const opened = now();
  const invoke = (j) => { inputs[0] = BigInt(seed+j); budget.value=1000000000n; return instance.exports.zx_entry(); };
  invoke(0);
  const warmed = now();
  let warmupCount=0;
  do { invoke(warmupCount++ & 1023); } while(now()-warmed < 500);
  const measured = now();
  let checksum = 0n;
  for(let j=0;j<batches;j++) checksum += invoke(j);
  const ended = now();
  if(checksum.toString() !== String(expected)) throw Error('checksum mismatch: '+checksum+' != '+expected);
  return {compile_observed_ms:compiled-started, instantiate_ms:opened-compiled,
    first_ms:warmed-opened, warm_ms:ended-measured, warmup_ms:measured-warmed, warmup_count:warmupCount, checksum:checksum.toString(), batches};
}
if(typeof module !== 'undefined') {
  module.exports = {zxCompare};
  if(require.main === module) {
    const [file, seed, rounds, kind, batches, expected] = process.argv.slice(2);
    zxCompare(require('fs').readFileSync(file), +seed, +rounds, +kind, +batches, expected)
      .then(row => console.log(JSON.stringify(row))).catch(e=>{console.error(e);process.exitCode=1;});
  }
}
