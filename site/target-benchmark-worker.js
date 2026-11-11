importScripts('target-measure.js', 'target-abi.js');
self.onmessage = async function (event) {
  try {
    const {example, target, samples = 5} = event.data;
    const started = performance.now();
    const web = example.targets[target].web;
    const absolute = path => new URL(path, self.location.href).href;
    let recording = true;
    const output = [];
    const capture = bytes => { if (recording) output.push(...bytes); };
    let invoke;
    if (target === 'wasm') {
      const response = await fetch(absolute(example.targets.wasm.files[0].path));
      if (!response.ok) throw Error('Could not load WebAssembly.');
      const abi = matrixABI(capture);
      const {instance} = await WebAssembly.instantiate(await response.arrayBuffer(), abi.imports);
      abi.attach(instance);
      invoke = () => { abi.resetBudget(); return instance.exports.zx_entry(); };
    } else if (target === 'go') {
      importScripts(absolute(web.runtime));
      self.fs.writeSync = (fd, bytes) => {
        if (fd === 1) capture(bytes);
        else if (fd === 2) throw Error(new TextDecoder().decode(bytes));
        else throw Error('Unexpected output descriptor');
        return bytes.length;
      };
      const go = new Go();
      const response = await fetch(absolute(web.path));
      if (!response.ok) throw Error('Could not load Go WebAssembly.');
      const {instance} = await WebAssembly.instantiate(await response.arrayBuffer(), go.importObject);
      go.run(instance).catch(error => self.postMessage({ok: false, message: error.message}));
      if (typeof self.matrixGoRun !== 'function') throw Error('Go entry was not registered.');
      invoke = self.matrixGoRun;
    } else {
      importScripts(absolute(web.script));
      const factory = target === 'c' ? self.createMatrixC : self.createMatrixZX;
      const module = await factory({locateFile: name => new URL(name, absolute(web.script)).href,
        stdout: byte => { if (recording) output.push(byte); },
        printErr: message => { throw Error(message); }});
      if (target === 'zx') {
        const response = await fetch(absolute(example.targets.zx.files[0].path));
        if (!response.ok) throw Error('Could not load the ZX bundle.');
        module.FS.writeFile('/program.zx', new Uint8Array(await response.arrayBuffer()));
        if (!module.ccall('MatrixOpen', 'number', ['string'], ['/program.zx'])) throw Error('Could not open the ZX bundle.');
      }
      invoke = module._MatrixRun;
    }
    const loaded = performance.now() - started;
    if (String(invoke()) !== example.expected) throw Error('First result differs from oracle.');
    const observed = new TextDecoder().decode(new Uint8Array(output));
    if (observed !== example.stdout) throw Error('Printed output differs from oracle: ' + JSON.stringify({observed, expected: example.stdout}));
    recording = false;
    const result = matrixMeasure(invoke, example.expected, samples);
    self.postMessage({ok: true, ...result, stdout_verified: true, load_ms: loaded, user_agent: navigator.userAgent});
  } catch (error) { self.postMessage({ok: false, message: error.message}); }
};
