/* Run the actual direct zi2wasm artifact, isolated from the page. */
self.onmessage = async function (event) {
  try {
    const started = performance.now();
    const response = await fetch(event.data.path);
    if (!response.ok) throw new Error('Could not load the WebAssembly artifact.');
    const budget = new WebAssembly.Global({value: 'i64', mutable: true}, 10000000n);
    const {instance} = await WebAssembly.instantiate(await response.arrayBuffer(), {
      zx: {budget, overrun() { throw new Error('Execution budget exceeded.'); }},
    });
    const result = instance.exports.zx_entry().toString();
    if (result !== event.data.expected) throw new Error('The result differs from the published oracle.');
    self.postMessage({ok: true, result, elapsed: performance.now() - started});
  } catch (error) { self.postMessage({ok: false, message: error.message}); }
};
