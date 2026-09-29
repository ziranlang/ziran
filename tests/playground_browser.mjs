import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import http from 'node:http';
import os from 'node:os';
import path from 'node:path';
import {spawn} from 'node:child_process';
import {fileURLToPath} from 'node:url';

const repo = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const site = path.join(repo, 'site');
const profile = await fs.mkdtemp(path.join(os.tmpdir(), 'ziran-playground-browser-'));
const environment = {...process.env};
delete environment.DISPLAY;
delete environment.WAYLAND_DISPLAY;
const server = http.createServer(async (request, response) => {
  const pathname = decodeURIComponent(new URL(request.url, 'http://localhost').pathname);
  const file = path.resolve(site, '.' + (pathname === '/' ? '/index.html' : pathname));
  if (!file.startsWith(site + path.sep)) { response.writeHead(403).end(); return; }
  try {
    const data = await fs.readFile(file);
    const mime = {'.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css', '.wasm': 'application/wasm', '.svg': 'image/svg+xml', '.png': 'image/png', '.woff2': 'font/woff2'}[path.extname(file)];
    response.writeHead(200, {'Content-Type': mime || 'application/octet-stream'}).end(data);
  } catch (_) { response.writeHead(404).end(); }
});
await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
const url = 'http://127.0.0.1:' + server.address().port + '/';
const browser = spawn('xvfb-run', ['-a', 'chromium', '--headless', '--no-sandbox', '--disable-gpu', '--disable-dev-shm-usage', '--disable-background-networking', '--remote-debugging-port=0', '--user-data-dir=' + profile, 'about:blank'], {env: environment, detached: true, stdio: ['ignore', 'ignore', 'pipe']});
let browserLog = '';
browser.stderr.on('data', chunk => { browserLog = (browserLog + chunk).slice(-2000); });
let socket;
let id = 0;
const pending = new Map();
const exceptions = [];
const requests = [];
const delay = ms => new Promise(resolve => setTimeout(resolve, ms));
async function waitFor(test, message, milliseconds = 15000) {
  const deadline = Date.now() + milliseconds;
  while (Date.now() < deadline) {
    if (await test()) return;
    await delay(50);
  }
  throw new Error(message);
}
function cdp(method, params = {}) {
  return new Promise((resolve, reject) => {
    const next = ++id;
    const timer = setTimeout(() => { pending.delete(next); reject(new Error('CDP timeout: ' + method)); }, 15000);
    pending.set(next, {resolve, reject, timer});
    socket.send(JSON.stringify({id: next, method, params}));
  });
}
async function evaluate(expression) {
  const result = await cdp('Runtime.evaluate', {expression, returnByValue: true, awaitPromise: true});
  if (result.exceptionDetails) throw new Error(JSON.stringify(result.exceptionDetails));
  return result.result.value;
}
async function edit(source) {
  await evaluate(`(() => { const editor = document.querySelector('.example-panel:not([hidden]) .code-editor'); editor.value = ${JSON.stringify(source)}; editor.dispatchEvent(new Event('input')); })()`);
}
async function run() {
  await evaluate(`document.querySelector('.playground-run').click()`);
  await waitFor(() => evaluate(`!document.querySelector('.playground-run').disabled`), 'Program did not finish');
  return evaluate(`document.querySelector('.example-panel:not([hidden]) .code-output code').textContent`);
}
async function screenshot(file, full = false) {
  const params = {format: 'png', captureBeyondViewport: full};
  if (full) {
    const {cssContentSize} = await cdp('Page.getLayoutMetrics');
    params.clip = {x: 0, y: 0, width: cssContentSize.width, height: cssContentSize.height, scale: 1};
  }
  const {data} = await cdp('Page.captureScreenshot', params);
  await fs.writeFile(file, Buffer.from(data, 'base64'));
}

try {
  let port;
  await waitFor(async () => {
    try { port = Number((await fs.readFile(path.join(profile, 'DevToolsActivePort'), 'utf8')).split('\n')[0]); return !!port; }
    catch (_) { return false; }
  }, 'Private browser did not start: ' + browserLog);
  const pages = await (await fetch('http://127.0.0.1:' + port + '/json/list')).json();
  socket = new WebSocket(pages.find(page => page.type === 'page').webSocketDebuggerUrl);
  await new Promise((resolve, reject) => { socket.addEventListener('open', resolve, {once: true}); socket.addEventListener('error', reject, {once: true}); });
  socket.addEventListener('message', event => {
    const message = JSON.parse(event.data);
    if (message.method === 'Runtime.exceptionThrown') exceptions.push(message.params.exceptionDetails);
    if (message.method === 'Network.requestWillBeSent') requests.push(message.params.request.url);
    if (message.id && pending.has(message.id)) {
      const item = pending.get(message.id);
      pending.delete(message.id);
      clearTimeout(item.timer);
      if (message.error) item.reject(new Error(JSON.stringify(message.error)));
      else item.resolve(message.result);
    }
  });
  await cdp('Page.enable');
  await cdp('Runtime.enable');
  await cdp('Network.enable');
  await cdp('Emulation.setDeviceMetricsOverride', {width: 1440, height: 1080, deviceScaleFactor: 1, mobile: false});
  await cdp('Page.navigate', {url});
  await waitFor(() => evaluate(`document.readyState === 'complete' && document.fonts.status === 'loaded'`), 'Page did not load');
  assert.equal(await evaluate(`document.querySelectorAll('.code-editor').length`), 3);
  assert.equal(requests.some(request => request.includes('playground-runtime')), false, 'The runtime must load only when Run is pressed');
  assert.equal(await run(), '1', 'The default imported text program runs');
  await evaluate(`document.getElementById('tab-hello').click()`);
  await edit('main :: () { print("Hello, edited Ziran!\\n"); }');
  await evaluate(`document.querySelector('#example-hello .code-editor').focus()`);
  await cdp('Input.dispatchKeyEvent', {type: 'keyDown', key: 'Enter', code: 'Enter', modifiers: 2, windowsVirtualKeyCode: 13});
  await cdp('Input.dispatchKeyEvent', {type: 'keyUp', key: 'Enter', code: 'Enter', modifiers: 2, windowsVirtualKeyCode: 13});
  await waitFor(() => evaluate(`!document.querySelector('.playground-run').disabled`), 'Keyboard-triggered program did not finish');
  assert.equal(await evaluate(`document.querySelector('#example-hello .code-output code').textContent`), 'Hello, edited Ziran!');
  await evaluate(`document.getElementById('tab-text').click(); document.getElementById('tab-hello').click()`);
  assert.match(await evaluate(`document.querySelector('#example-hello .code-editor').value`), /edited Ziran/);
  await edit('Answer :: () -> s32 { return missing_value; }');
  assert.match(await run(), /missing_value/);
  assert.equal(await evaluate(`document.querySelector('#example-hello .code-output').classList.contains('has-error')`), true);
  await edit('main :: () { print("<img src=x onerror=alert(1)>\\n"); }');
  assert.equal(await evaluate(`document.querySelectorAll('.hero-code img').length`), 0, 'Source highlighting must not interpret HTML');
  assert.equal(await run(), '<img src=x onerror=alert(1)>');
  await evaluate(`document.querySelector('.playground-reset').click()`);
  assert.match(await evaluate(`document.querySelector('#example-hello .code-editor').value`), /Hello, World!/);
  await evaluate(`document.querySelector('.playground-run').click(); document.querySelector('.playground-stop').click()`);
  assert.equal(await evaluate(`document.querySelector('#example-hello .code-output code').textContent`), 'Run stopped.');
  await delay(200);
  assert.equal(await evaluate(`document.querySelector('#example-hello .code-output code').textContent`), 'Run stopped.', 'Late worker messages must not replace stopped output');
  await evaluate(`document.querySelector('.playground-run').click(); document.getElementById('tab-score').click()`);
  assert.equal(await evaluate(`document.querySelector('.playground-run').disabled`), false);
  await evaluate(`(() => { const value = document.querySelector('.record-value-input'); value.value = '9'; value.dispatchEvent(new Event('input')); const bonus = document.querySelector('.record-bonus-input'); bonus.value = '4'; bonus.dispatchEvent(new Event('input')); })()`);
  assert.equal(await evaluate(`document.querySelector('.record-total').textContent`), '13');
  await evaluate(`document.querySelector('[data-example="score"]').click()`);
  assert.equal(await run(), '13', 'Diagram values become executable source');
  await edit('Score :: struct { value: s32; bonus: s32; }\nTotal :: (score: Score) -> s32 { return score.value + score.bonus; }\nAnswer :: () -> s32 { score: Score = Score.{value = 9, bonus = 6}; return Total(score); }');
  assert.equal(await run(), '15');
  assert.equal(await evaluate(`document.querySelector('.record-total').textContent`), '15', 'A successful editor run updates the diagram');
  assert.equal(await evaluate(`document.querySelector('.record-bonus-input').value`), '6');
  await evaluate(`(() => { const prefix = document.querySelector('.text-prefix-input'); prefix.value = 'za'; prefix.dispatchEvent(new Event('input')); })()`);
  assert.equal(await evaluate(`document.querySelector('.text-summary').textContent`), 'No prefix match');
  await evaluate(`document.querySelector('[data-example="text"]').click()`);
  assert.equal(await run(), '0');
  await evaluate(`(() => { const value = document.querySelector('.text-value-input'); value.value = 'Ä'; value.dispatchEvent(new Event('input')); const prefix = document.querySelector('.text-prefix-input'); prefix.value = 'ä'; prefix.dispatchEvent(new Event('input')); })()`);
  assert.equal(await evaluate(`document.querySelector('.text-summary').textContent`), 'No prefix match', 'ASCII folding must not fold non-ASCII characters');
  await evaluate(`(() => { const target = document.querySelector('.target-select'); target.value = 'rust'; target.dispatchEvent(new Event('change')); })()`);
  assert.match(await evaluate(`document.querySelector('.target-command').textContent`), /--target=rust/);
  assert.equal(await evaluate(`document.querySelector('.native-visual .is-selected').dataset.target`), 'rust');
  await evaluate(`document.querySelector('.playground-reset').click(); window.scrollTo(0, 0); document.activeElement.blur()`);
  await screenshot('/tmp/ziran-interactive-desktop.png');
  await screenshot('/tmp/ziran-interactive-full.png', true);
  for (const width of [320, 390, 650, 768, 1024, 1440]) {
    await cdp('Emulation.setDeviceMetricsOverride', {width, height: 1000, deviceScaleFactor: 1, mobile: false});
    const documentWidth = await evaluate(`document.documentElement.scrollWidth`);
    assert.ok(documentWidth <= width, 'Horizontal overflow at ' + width + ': ' + documentWidth);
  }
  await cdp('Emulation.setDeviceMetricsOverride', {width: 390, height: 844, deviceScaleFactor: 1, mobile: false});
  await screenshot('/tmp/ziran-interactive-mobile.png', true);
  await evaluate(`document.querySelector('.menu-button').click(); document.querySelector('.theme-toggle').click(); document.querySelector('.menu-button').click()`);
  assert.equal(await evaluate(`document.documentElement.dataset.theme`), 'dark');
  await screenshot('/tmp/ziran-interactive-dark.png', true);
  await cdp('Emulation.setScriptExecutionDisabled', {value: true});
  await cdp('Page.navigate', {url});
  await waitFor(() => evaluate(`document.readyState === 'complete'`), 'No-JavaScript page did not load');
  assert.equal(await evaluate(`document.querySelector('.playground-toolbar').hidden`), true);
  assert.equal(await evaluate(`document.querySelector('#example-text').hidden`), false);
  assert.equal(await evaluate(`document.querySelectorAll('.code-editor').length`), 0);
  assert.deepEqual(exceptions, []);
  console.log('Playground browser: real edited runs, imports, compiler errors, safe highlighting, tab preservation, reset, stop, cancellation, live diagrams, source transfer, target commands, six widths, dark theme, and no-JavaScript fallback passed');
} finally {
  if (socket && socket.readyState === WebSocket.OPEN) {
    try { await cdp('Browser.close'); } catch (_) {}
    socket.close();
  }
  try { process.kill(-browser.pid, 'SIGTERM'); } catch (_) {}
  server.closeAllConnections();
  await new Promise(resolve => server.close(resolve));
}
