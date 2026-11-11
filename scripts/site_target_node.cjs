const fs = require('node:fs');
const measure = require('../site/target-measure.js');
(async () => {
  const [file, expected, samples] = process.argv.slice(2);
  const started = performance.now();
  let recording = true;
  const abi = require('../site/target-abi.js')(bytes => { if (recording) process.stdout.write(bytes); });
  const {instance} = await WebAssembly.instantiate(fs.readFileSync(file), abi.imports);
  abi.attach(instance);
  const loaded = performance.now() - started;
  if (instance.exports.zx_entry().toString() !== expected) throw Error('First result differs from oracle');
  recording = false;
  const result = measure(() => { abi.resetBudget(); return instance.exports.zx_entry(); }, expected, Number(samples));
  console.log(JSON.stringify({...result, load_ms: loaded}));
})().catch(error => { console.error(error); process.exitCode = 1; });
