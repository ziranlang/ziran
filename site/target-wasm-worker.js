/* Run the actual zi2wasm artifact, isolated from the page. */
importScripts('target-abi.js');
self.onmessage = async function (event) {
  try {
    const started = performance.now();
    const response = await fetch(event.data.path);
    if (!response.ok) throw new Error('Could not load the WebAssembly artifact.');
    const output = [];
    const abi = matrixABI(bytes => output.push(...bytes));
    const {instance} = await WebAssembly.instantiate(await response.arrayBuffer(), abi.imports);
    abi.attach(instance);
    const result = instance.exports.zx_entry().toString();
    if (result !== event.data.expected) throw new Error('The result differs from the published oracle.');
    const stdout = new TextDecoder().decode(new Uint8Array(output));
    if (stdout !== (event.data.stdout || '')) throw Error('Printed output differs from the published oracle.');
    self.postMessage({ok: true, result, stdout, elapsed: performance.now() - started});
  } catch (error) { self.postMessage({ok: false, message: error.message}); }
};
