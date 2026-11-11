(async function () {
  const code = document.getElementById('matrix-code');
  if (!code) return;
  const live = document.getElementById('matrix-live-result');
  let data;
  try {
    const response = await fetch('generated/targets/matrix.json');
    if (!response.ok) throw new Error();
    data = await response.json();
  } catch (_) {
    live.textContent = 'The interactive snapshot could not load. The matrix links still open the generated files. Reload to try again.';
    return;
  }
  const byId = new Map(data.examples.map(example => [example.id, example]));
  const labels = {source: 'Source', c: 'C', go: 'Go', zx: 'ZX', wasm: 'WebAssembly', zir: 'Checked IR'};
  const descriptions = {
    source: 'The exact source used for every target in this row.',
    c: 'Readable generated C and its headers. Choose a file to inspect the complete output.',
    go: 'Readable generated Go, including imported modules. Choose a file to inspect it.',
    zx: 'The real .zx bundle, decoded by zi-inspect. Download the binary to run it with ziran.',
    wasm: 'The actual WebAssembly binary: header, sections, imports, exports and a byte preview. Download it or run it below.',
    zir: 'The saved checked IR, decoded by zi-inspect: declarations, typed expressions and statement links. ZIR is an intermediate stage.',
  };
  const tabs = [...document.querySelectorAll('[data-view]')];
  const files = document.getElementById('matrix-file');
  const download = document.getElementById('matrix-download');
  const runButton = document.getElementById('matrix-run-wasm');
  const fileLabel = document.getElementById('matrix-file-label');
  let selected = data.examples[0];
  let view = 'source';
  const platformSelect = document.getElementById('matrix-platform');
  let platform = new URL(location.href).searchParams.get('platform') || 'desktop';
  if (!data.platforms[platform]) platform = 'desktop';
  platformSelect.value = platform;
  let revision = 0;
  let worker;
  let deadline;
  const ms = value => value == null ? '—' : value < 0.001 ? (value * 1000000).toFixed(1) + ' ns' : value < 1 ? (value * 1000).toFixed(2) + ' µs' : value.toFixed(2) + ' ms';
  const measurement = (example, target) => data.platforms[platform]?.examples[example.id]?.[target];
  const bytes = value => value == null ? '—' : value.toLocaleString() + ' B';
  function stop() {
    if (worker) worker.terminate();
    worker = null;
    clearTimeout(deadline);
    runButton.disabled = false;
  }
  async function read(path) {
    const response = await fetch(path);
    if (!response.ok) throw new Error('Could not load ' + path + '. Reload to try again.');
    return response;
  }
  async function wasmListing(file) {
    const buffer = await (await read(file.path)).arrayBuffer();
    const image = new Uint8Array(buffer);
    const module = await WebAssembly.compile(buffer);
    const lines = ['WebAssembly binary · ' + image.byteLength + ' bytes', 'Magic: 00 61 73 6d · version 1', '', 'Imports:'];
    WebAssembly.Module.imports(module).forEach(item => lines.push('  ' + item.module + '.' + item.name + ' (' + item.kind + ')'));
    lines.push('', 'Exports:');
    WebAssembly.Module.exports(module).forEach(item => lines.push('  ' + item.name + ' (' + item.kind + ')'));
    lines.push('', 'First ' + Math.min(256, image.length) + ' bytes (hex):');
    for (let at = 0; at < Math.min(256, image.length); at += 16) {
      lines.push(at.toString(16).padStart(6, '0') + '  ' + [...image.slice(at, at + 16)].map(byte => byte.toString(16).padStart(2, '0')).join(' '));
    }
    return lines.join('\n');
  }
  async function loadFile() {
    const current = ++revision;
    const cell = selected.targets[view];
    const file = view === 'source' ? {path: selected.source} : cell.files[Number(files.value) || 0];
    download.href = file.path;
    download.textContent = 'Download ' + file.path.split('/').pop() + ' ↓';
    download.hidden = false;
    code.textContent = 'Loading output…';
    try {
      const text = view === 'wasm' ? await wasmListing(file) :
        await (await read((view === 'zx' || view === 'zir') ? (file.inspection || cell.inspection).path : file.path)).text();
      if (current === revision) code.textContent = text;
    } catch (error) { if (current === revision) code.textContent = error.message; }
  }
  function select(example, nextView, updateHash = true) {
    stop();
    revision++;
    selected = example;
    view = nextView;
    live.textContent = '';
    document.getElementById('inspect-heading').textContent = example.title;
    document.getElementById('matrix-result').textContent = example.stdout ? 'Printed output verified' : 'Expected result: ' + example.expected;
    document.getElementById('matrix-view-description').textContent = descriptions[view];
    document.getElementById('output-view').setAttribute('aria-labelledby', 'view-' + view);
    tabs.forEach(tab => { const active = tab.dataset.view === view; tab.setAttribute('aria-selected', String(active)); tab.tabIndex = active ? 0 : -1; });
    document.querySelectorAll('.matrix-cell').forEach(cell => {
      if (cell.dataset.example === example.id && cell.dataset.target === view) cell.setAttribute('aria-current', 'true');
      else cell.removeAttribute('aria-current');
    });
    const cell = example.targets[view];
    const supported = view === 'source' || cell.status === 'ok';
    fileLabel.hidden = true;
    runButton.hidden = !(view === 'wasm' && supported);
    document.getElementById('matrix-details').hidden = view === 'source';
    download.hidden = !supported;
    if (cell) {
      document.getElementById('matrix-emission').textContent = ms(cell.emission_ms);
      document.getElementById('matrix-execution').textContent = ms(measurement(example, view)?.execution_ms);
      const measured = measurement(example, view);
      document.getElementById('matrix-sample-range').textContent = measured ?
        data.platforms[platform].label + ' · ' + measured.batch_samples_ms.length + ' measured batches × ' +
        measured.batches.toLocaleString() + ' calls · per-call range ' +
        ms(Math.min(...measured.sample_statistics.included_samples_ms)) + '–' + ms(Math.max(...measured.sample_statistics.included_samples_ms)) +
        ' · ' + measured.sample_statistics.excluded_indices.length + ' outliers excluded (1.5×IQR)' :
        'Checked IR is a shared compiler stage; it has no platform execution samples.';
      document.getElementById('matrix-size').textContent = bytes(cell.bytes);
      document.getElementById('matrix-verified').textContent = view === 'zir' ? 'Checked' : example.stdout ? 'Output matches' : cell.result;
      document.getElementById('matrix-command').textContent = cell.command + (measurement(example, view)?.command ? '\n\n' + measurement(example, view).command : '');
    }
    if (supported) {
      files.replaceChildren();
      if (view !== 'source') cell.files.forEach((file, index) => {
        const option = new Option(file.path.split('/' + view + '/')[1], String(index));
        files.add(option);
      });
      fileLabel.hidden = view === 'source' || cell.files.length < 2;
      loadFile();
    } else {
      code.textContent = cell.diagnostic;
      document.getElementById('matrix-view-description').textContent = 'This program is outside the WebAssembly subset. The compiler rejected it with this diagnostic; no artifact or timing is reported.';
    }
    if (updateHash) history.replaceState(null, '', '#' + example.id + '/' + view);
  }
  document.querySelectorAll('.matrix-cell').forEach(cell => cell.addEventListener('click', event => {
    if (event.metaKey || event.ctrlKey || event.shiftKey || event.altKey) return;
    event.preventDefault();
    select(byId.get(cell.dataset.example), cell.dataset.target);
    document.getElementById('inspect').scrollIntoView({behavior: matchMedia('(prefers-reduced-motion: reduce)').matches ? 'auto' : 'smooth', block: 'start'});
  }));
  tabs.forEach(tab => {
    tab.addEventListener('click', () => select(selected, tab.dataset.view));
    tab.addEventListener('keydown', event => {
      if (!['ArrowLeft', 'ArrowRight', 'Home', 'End'].includes(event.key)) return;
      event.preventDefault();
      const index = tabs.indexOf(tab);
      const next = event.key === 'Home' ? 0 : event.key === 'End' ? tabs.length - 1 : (index + (event.key === 'ArrowRight' ? 1 : -1) + tabs.length) % tabs.length;
      tabs[next].focus(); tabs[next].click();
    });
  });
  files.addEventListener('change', loadFile);
  function refreshMatrix() {
    const metric = document.getElementById('matrix-metric').value;
    const snapshot = data.platforms[platform];
    document.querySelectorAll('.matrix-cell').forEach(link => {
      const example = byId.get(link.dataset.example), target = link.dataset.target;
      const cell = example.targets[target];
      link.querySelector('.matrix-value').textContent = metric === 'size' ? bytes(cell.bytes) :
        metric === 'emission' ? ms(cell.emission_ms) : ms(measurement(example, target)?.execution_ms);
      link.querySelector('.matrix-state').textContent = target === 'zir' ? 'Shared stage' : 'Verified';
    });
    document.querySelectorAll('[data-runtime]').forEach(label => {
      label.textContent = snapshot.runtimes[label.dataset.runtime].split(' · ').slice(1).join(' · ');
    });
    document.getElementById('matrix-metric-note').textContent = metric === 'size' ? 'Emitted file bytes, including C headers and imported modules; excludes linked executables and external runtimes. Size and emission are independent of the platform selector.' : metric === 'emission' ? 'Measured on the desktop build host: load, check, link and emit. Downstream C and Go builds are excluded. The selected platform changes execution measurements.' : snapshot.methodology + ' Checked IR is a shared stage with no execution timing.';
    document.getElementById('matrix-provenance').textContent = snapshot.label + ' · ' + snapshot.host + ' · measured ' + snapshot.measured_at.slice(0, 10) + ' · ' + snapshot.samples + ' measured batches · compiler base ' + data.revision.slice(0, 10) + (data.compiler_dirty ? ' with local compiler edits (exact hashes in the snapshot).' : '.');
    const snapshotLink = document.createElement('a');
    snapshotLink.href = 'generated/targets/matrix.json';
    snapshotLink.textContent = 'Download all samples and provenance ↓';
    document.getElementById('matrix-provenance').append(' ', snapshotLink);
  }
  document.getElementById('matrix-metric').addEventListener('change', refreshMatrix);
  platformSelect.addEventListener('change', () => {
    platform = platformSelect.value;
    const url = new URL(location.href);
    url.searchParams.set('platform', platform);
    history.replaceState(null, '', url);
    refreshMatrix();
    select(selected, view, false);
  });
  runButton.addEventListener('click', () => {
    stop();
    try {
      worker = new Worker('target-wasm-worker.js');
      runButton.disabled = true;
      live.textContent = 'Loading and running the WebAssembly artifact…';
      const current = worker;
      const fail = message => { if (worker !== current) return; stop(); live.textContent = message; };
      deadline = setTimeout(() => fail('Run stopped after 10 seconds. Try again.'), 10000);
      current.onerror = () => fail('The WebAssembly worker could not start. Reload to try again.');
      current.onmessage = event => {
        if (worker !== current) return;
        const result = event.data;
        stop();
        live.textContent = result.ok ? 'Verified in your browser: ' + (result.stdout || result.result) + ' · ' + result.elapsed.toFixed(2) + ' ms including download, compilation and execution. This live run is separate from the saved benchmark.' : result.message;
      };
      current.postMessage({path: selected.targets.wasm.files[0].path, expected: selected.expected, stdout: selected.stdout});
    } catch (_) { stop(); live.textContent = 'This browser could not start the WebAssembly runner.'; }
  });
  document.querySelector('.matrix-views').hidden = false;
  document.querySelector('.matrix-metric').hidden = false;
  document.querySelector('.matrix-platform').hidden = false;
  refreshMatrix();
  function fromHash() {
    const [id, target] = location.hash.slice(1).split('/');
    if (byId.has(id) && Object.hasOwn(labels, target)) select(byId.get(id), target, false);
    else select(selected, view, false);
  }
  window.addEventListener('hashchange', fromHash);
  fromHash();
})();
