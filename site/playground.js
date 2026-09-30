(function () {
  var root = document.querySelector('.hero-code');
  if (!root) return;
  if (document.documentElement.hasAttribute('data-mini')) {
    var fullPlayground = root.querySelector('.example-link');
    fullPlayground.href = '/';
    fullPlayground.target = '_blank';
    fullPlayground.rel = 'noopener';
    fullPlayground.textContent = 'Open full playground ↗';
  }
  var toolbar = root.querySelector('.playground-toolbar');
  var runButton = root.querySelector('.playground-run');
  var stopButton = root.querySelector('.playground-stop');
  var resetButton = root.querySelector('.playground-reset');
  var status = root.querySelector('.playground-status');
  var tabs = root.querySelector('.example-tabs');
  var flow = document.querySelector('.source-flow');
  var worker = null;
  var timer;
  var runningPanel;
  var stage = 'write';
  var started;
  var examples = new Map();

  function active() { return root.querySelector('.example-panel:not([hidden])'); }
  function setFlow(current, failed) {
    stage = current;
    var at = {write: 0, loading: 0, check: 1, build: 2, run: 2, done: 3}[current];
    Array.from(flow.children).forEach(function (item, index) {
      item.dataset.state = index < at ? 'done' : index === at ? (failed ? 'failed' : 'active') : '';
    });
  }
  function stop() {
    clearTimeout(timer);
    if (worker) worker.terminate();
    worker = null;
    runButton.disabled = false;
    stopButton.hidden = true;
    root.removeAttribute('aria-busy');
  }
  function showOutput(panel, text, error) {
    panel.querySelector('.code-output code').textContent = text;
    panel.querySelector('.output-label').textContent = error ? 'Compiler / runtime message' : 'Output';
    panel.querySelector('.code-output').classList.toggle('has-error', !!error);
  }
  function changed(panel) {
    if (worker) stop();
    showOutput(panel, 'Run to see the result.', false);
    if (panel === active()) {
      status.textContent = 'Changed · Run to update';
      setFlow('write');
    }
  }
  function highlight(text, code) {
    var expression = /\/\/[^\n]*|"(?:\\.|[^"\\])*"|#[A-Za-z_]\w*|\b(?:struct|enum|return|if|else|for|while|true|false|defer|cast)\b|\b(?:s8|s16|s32|s64|u8|u16|u32|u64|float32|float64|bool|string)\b|\b\d+(?:\.\d+)?\b|\b[A-Za-z_]\w*(?=\s*(?:::|\())/g;
    var fragment = document.createDocumentFragment();
    var last = 0;
    var match;
    while ((match = expression.exec(text))) {
      fragment.appendChild(document.createTextNode(text.slice(last, match.index)));
      var span = document.createElement('span');
      var token = match[0];
      var tail = text.slice(expression.lastIndex);
      span.className = token.startsWith('//') ? 'c-comment' : token.startsWith('"') ? 'c-string' : /^\d/.test(token) ? 'c-number' : /^(?:s\d+|u\d+|float\d+|bool|string)$/.test(token) ? 'c-type' : /^\s*::/.test(tail) ? 'c-decl' : /^\s*\(/.test(tail) && !/^(?:if|while|for|cast)$/.test(token) ? 'c-call' : 'c-keyword';
      span.textContent = token;
      fragment.appendChild(span);
      last = expression.lastIndex;
    }
    fragment.appendChild(document.createTextNode(text.slice(last) + '\n'));
    code.replaceChildren(fragment);
  }
  root.querySelectorAll('.example-panel').forEach(function (panel) {
    var pre = panel.querySelector('pre');
    var code = pre.querySelector('code');
    var original = code.textContent;
    var originalOutput = panel.querySelector('.code-output code').textContent.replace(/^›\s*/, '');
    var wrap = document.createElement('div');
    wrap.className = 'code-editor-wrap';
    pre.replaceWith(wrap);
    wrap.appendChild(pre);
    pre.className = 'code-highlight';
    pre.setAttribute('aria-hidden', 'true');
    var editor = document.createElement('textarea');
    editor.className = 'code-editor';
    editor.value = original;
    editor.spellcheck = false;
    editor.autocomplete = 'off';
    editor.autocapitalize = 'off';
    editor.setAttribute('autocorrect', 'off');
    editor.setAttribute('wrap', 'off');
    editor.setAttribute('aria-label', 'Edit ' + panel.querySelector('.example-file').firstChild.textContent.trim());
    editor.setAttribute('aria-describedby', 'playground-help');
    wrap.appendChild(editor);
    function refresh() { highlight(editor.value, code); pre.scrollTop = editor.scrollTop; pre.scrollLeft = editor.scrollLeft; }
    editor.addEventListener('input', function () { refresh(); changed(panel); });
    editor.addEventListener('scroll', function () { pre.scrollTop = editor.scrollTop; pre.scrollLeft = editor.scrollLeft; });
    editor.addEventListener('keydown', function (event) {
      if (event.key === 'Enter' && (event.ctrlKey || event.metaKey)) {
        event.preventDefault();
        if (!worker) run();
      }
    });
    refresh();
    examples.set(panel.id, {editor: editor, refresh: refresh, original: original, originalOutput: originalOutput});
  });
  status.id = 'playground-help';
  toolbar.hidden = false;
  runButton.title = 'Run (Ctrl+Enter or ⌘+Enter)';
  function deadline(milliseconds) {
    clearTimeout(timer);
    timer = setTimeout(function () {
      var panel = runningPanel;
      stop();
      showOutput(panel, stage === 'loading' ? 'The runner took too long to load. Please try again.' : 'Run stopped after 10 seconds. Try a smaller program.', true);
      status.textContent = 'Stopped · edit and try again';
      setFlow(stage, true);
    }, milliseconds);
  }
  function run() {
    var panel = active();
    var source = examples.get(panel.id).editor.value;
    stop();
    if (source.length > 65536) {
      showOutput(panel, 'Keep the program under 64 KB.', true);
      status.textContent = 'Program is too large';
      return;
    }
    if (location.protocol === 'file:') {
      showOutput(panel, 'Open this page through a local web server to run code.', true);
      status.textContent = 'A web address is needed to run';
      return;
    }
    try { worker = new Worker('playground-worker.js'); }
    catch (_) {
      showOutput(panel, 'The runner could not start. Please try a browser with WebAssembly and Web Workers.', true);
      status.textContent = 'Unable to start';
      return;
    }
    var currentWorker = worker;
    runningPanel = panel;
    started = performance.now();
    runButton.disabled = true;
    stopButton.hidden = false;
    root.setAttribute('aria-busy', 'true');
    showOutput(panel, 'Loading the runner…', false);
    status.textContent = 'Loading…';
    setFlow('loading');
    deadline(30000);
    currentWorker.onmessage = function (event) {
      if (worker !== currentWorker) return;
      var result = event.data;
      if (result.type === 'stage') {
        setFlow(result.stage);
        status.textContent = {loading: 'Loading…', check: 'Checking source…', build: 'Building portable program…', run: 'Running…'}[result.stage];
        if (result.stage !== 'loading') deadline(10000);
      } else if (result.type === 'result') {
        stop();
        var message = [result.output, result.diagnostics].filter(Boolean).join('\n');
        showOutput(panel, message || (result.ok ? 'Program finished without output.' : 'The program could not run.'), !result.ok);
        if (result.ok) syncDiagrams(panel, source, result.output);
        status.textContent = result.ok ? 'Finished · ' + ((performance.now() - started) / 1000).toFixed(2) + 's' : 'Fix the error and run again';
        setFlow(result.ok ? 'done' : stage, !result.ok);
      }
    };
    currentWorker.onerror = function () {
      if (worker !== currentWorker) return;
      stop();
      showOutput(panel, 'Could not load the runner. Please try again.', true);
      status.textContent = 'Unable to run';
      setFlow(stage, true);
    };
    currentWorker.postMessage({source: source});
  }
  runButton.addEventListener('click', run);
  stopButton.addEventListener('click', function () {
    var panel = runningPanel;
    stop();
    showOutput(panel, 'Run stopped.', false);
    status.textContent = 'Stopped · ready to run';
    setFlow('write');
  });
  resetButton.addEventListener('click', function () {
    stop();
    var panel = active();
    var example = examples.get(panel.id);
    example.editor.value = example.original;
    example.refresh();
    showOutput(panel, example.originalOutput, false);
    panel.querySelector('.output-label').textContent = 'Example output';
    syncDiagrams(panel, example.original, example.originalOutput);
    status.textContent = 'Example restored · edit and Run';
    setFlow('write');
  });
  tabs.addEventListener('examplechange', function () {
    if (worker) {
      showOutput(runningPanel, 'Run stopped when the example changed.', false);
      stop();
    }
    status.textContent = 'Edit the code, then Run.';
    setFlow('write');
  });
  document.querySelectorAll('.diagram-controls, .diagram-edit').forEach(function (element) { element.hidden = false; });
  var valueInput = document.querySelector('.record-value-input');
  var bonusInput = document.querySelector('.record-bonus-input');
  function recordValues() {
    var valid = valueInput.validity.valid && bonusInput.validity.valid && Number.isFinite(valueInput.valueAsNumber) && Number.isFinite(bonusInput.valueAsNumber);
    var value = valueInput.valueAsNumber;
    var bonus = bonusInput.valueAsNumber;
    document.querySelector('.record-value').textContent = valid ? value : '—';
    document.querySelector('.record-bonus').textContent = valid ? bonus : '—';
    document.querySelector('.record-total').textContent = valid ? value + bonus : '—';
    document.querySelector('.record-summary').textContent = valid ? value + ' + ' + bonus + ' = ' + (value + bonus) : 'Use whole numbers from −999 to 999.';
    return valid;
  }
  valueInput.addEventListener('input', recordValues);
  bonusInput.addEventListener('input', recordValues);
  var textInput = document.querySelector('.text-value-input');
  var prefixInput = document.querySelector('.text-prefix-input');
  function foldASCII(value) { return value.replace(/[A-Z]/g, function (letter) { return letter.toLowerCase(); }); }
  function textValues() {
    var text = textInput.value;
    var prefix = prefixInput.value;
    var matches = foldASCII(text).startsWith(foldASCII(prefix));
    var preview = document.querySelector('.text-preview');
    preview.replaceChildren();
    if (matches && prefix.length) {
      var mark = document.createElement('b');
      mark.textContent = text.slice(0, prefix.length);
      preview.appendChild(mark);
      preview.appendChild(document.createTextNode(text.slice(prefix.length)));
    } else preview.textContent = text || '∅';
    var label = matches ? 'Prefix matched' : 'No prefix match';
    document.querySelector('.text-match').textContent = (matches ? '✓ ' : '× ') + label;
    document.querySelector('.text-summary').textContent = label;
  }
  function syncDiagrams(panel, source, output) {
    if (panel.id === 'example-score') {
      var fields = source.match(/Score\s*\.\s*\{\s*value\s*=\s*(-?\d+)\s*,\s*bonus\s*=\s*(-?\d+)\s*\}/);
      if (!fields || !/^-?\d+$/.test(output.trim())) return;
      var value = Number(fields[1]);
      var bonus = Number(fields[2]);
      if (Math.abs(value) > 999 || Math.abs(bonus) > 999) return;
      valueInput.value = value;
      bonusInput.value = bonus;
      recordValues();
      document.querySelector('.record-total').textContent = output.trim();
      if (output.trim() !== String(value + bonus)) document.querySelector('.record-summary').textContent = 'Editor result: ' + output.trim();
    }
    if (panel.id === 'example-text') {
      var strings = source.match(/StartsWithFoldASCII\(\s*("(?:\\.|[^"\\])*")\s*,\s*("(?:\\.|[^"\\])*")\s*\)/);
      if (!strings) return;
      try {
        textInput.value = JSON.parse(strings[1]);
        prefixInput.value = JSON.parse(strings[2]);
        textValues();
      } catch (_) {}
    }
  }
  textInput.addEventListener('input', textValues);
  prefixInput.addEventListener('input', textValues);
  document.querySelectorAll('.diagram-edit').forEach(function (button) {
    button.addEventListener('click', function () {
      var name = button.dataset.example;
      if (name === 'score' && !recordValues()) return;
      var example = examples.get('example-' + name);
      var source = example.original;
      if (name === 'score') source = source.replace('value = 40, bonus = 2', 'value = ' + valueInput.valueAsNumber + ', bonus = ' + bonusInput.valueAsNumber);
      if (name === 'text') source = source.replace('"Ziran", "zi"', JSON.stringify(textInput.value) + ', ' + JSON.stringify(prefixInput.value));
      document.getElementById('tab-' + name).click();
      example.editor.value = source;
      example.refresh();
      changed(active());
      root.scrollIntoView({behavior: matchMedia('(prefers-reduced-motion: reduce)').matches ? 'auto' : 'smooth', block: 'center'});
      example.editor.focus({preventScroll: true});
    });
  });
  document.querySelector('.target-select').addEventListener('change', function (event) {
    var target = event.target.value;
    var label = event.target.selectedOptions[0].textContent;
    document.querySelectorAll('.native-visual [data-target]').forEach(function (element) { element.classList.toggle('is-selected', element.dataset.target === target); });
    document.querySelector('.target-summary').textContent = 'Generate ' + label + ' output';
    var flags = target === 'c' || target === 'cpp' ? '' : ' --exe --entry source:main' + (target === 'go' ? ' --pkg main' : '');
    document.querySelector('.target-command code').textContent = 'ziran build --target=' + target + flags + ' --root . -o out/' + target + ' source.zi';
  });
})();
