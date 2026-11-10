// The direct zi2wasm ABI; no Emscripten runtime is involved.
const fs = require('node:fs');
const budget = new WebAssembly.Global({value: 'i64', mutable: true}, 10000000n);
WebAssembly.instantiate(fs.readFileSync(process.argv[2]), {
  zx: {budget, overrun() { throw new Error('Execution budget exceeded'); }},
}).then(({instance}) => console.log(instance.exports.zx_entry().toString()))
  .catch(error => { console.error(error); process.exitCode = 1; });
