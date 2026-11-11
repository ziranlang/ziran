// Execute the zi2wasm artifact using its shared budget/output ABI.
const fs = require('node:fs');
const abi = require('../site/target-abi.js')(bytes => process.stdout.write(bytes));
WebAssembly.instantiate(fs.readFileSync(process.argv[2]), abi.imports)
  .then(({instance}) => {
    abi.attach(instance);
    const result = instance.exports.zx_entry();
    if (!process.argv.includes('--output')) console.log(result.toString());
  })
  .catch(error => { console.error(error); process.exitCode = 1; });
