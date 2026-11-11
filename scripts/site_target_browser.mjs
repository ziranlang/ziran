// Collect real browser measurements on a private display. This does not use
// or attach to the owner's browser, profile, display or input devices.
import fs from 'node:fs/promises';
import http from 'node:http';
import path from 'node:path';
import {spawn} from 'node:child_process';
import {fileURLToPath} from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const site = path.join(root, 'site');
const [manifest, output] = process.argv.slice(2);
if (!manifest || !output) throw Error('Pass manifest and output paths');
const directory = path.join(root, 'build/site-target-platforms');
await fs.mkdir(directory, {recursive: true});
const profile = await fs.mkdtemp(path.join(directory, 'browser-'));
const environment = {...process.env, HARMONY_DESKTOP_RECOVERY: '0', YUE_DESKTOP_RECOVERY: '0'};
for (const key of ['DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'DBUS_SESSION_BUS_ADDRESS']) delete environment[key];
const server = http.createServer(async (request, response) => {
  const pathname = decodeURIComponent(new URL(request.url, 'http://localhost').pathname);
  if (pathname === '/') { response.writeHead(200, {'Content-Type': 'text/html'}).end('<!doctype html><title>Ziran target measurements</title>'); return; }
  if (pathname === '/matrix-benchmark.json') { response.writeHead(200, {'Content-Type': 'application/json'}).end(await fs.readFile(manifest)); return; }
  const file = path.resolve(site, '.' + pathname);
  if (!file.startsWith(site + path.sep)) { response.writeHead(403).end(); return; }
  try {
    const data = await fs.readFile(file);
    response.writeHead(200, {'Content-Type': ({'.js': 'text/javascript', '.wasm': 'application/wasm', '.json': 'application/json'})[path.extname(file)] || 'application/octet-stream'}).end(data);
  } catch (_) { response.writeHead(404).end(); }
});
await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
const browser = spawn('xvfb-run', ['-a', process.env.CHROMIUM || 'chromium', '--headless', '--no-sandbox', '--disable-gpu', '--disable-dev-shm-usage', '--disable-background-networking', '--remote-debugging-port=0', '--user-data-dir=' + profile, 'about:blank'], {env: environment, detached: true, stdio: ['ignore', 'ignore', 'pipe']});
let log = '';
browser.stderr.on('data', chunk => { log = (log + chunk).slice(-2000); });
let socket;
let sequence = 0;
const pending = new Map();
const delay = ms => new Promise(resolve => setTimeout(resolve, ms));
function cdp(method, params = {}) {
  return new Promise((resolve, reject) => {
    const id = ++sequence;
    const timer = setTimeout(() => { pending.delete(id); reject(Error('Browser measurement timed out: ' + method)); }, 240000);
    pending.set(id, {resolve, reject, timer});
    socket.send(JSON.stringify({id, method, params}));
  });
}
try {
  let port;
  const deadline = Date.now() + 20000;
  while (!port && Date.now() < deadline) {
    try { port = Number((await fs.readFile(path.join(profile, 'DevToolsActivePort'), 'utf8')).split('\n')[0]); } catch (_) {}
    if (!port) await delay(100);
  }
  if (!port) throw Error('Private benchmark browser did not start: ' + log);
  const pages = await (await fetch('http://127.0.0.1:' + port + '/json/list')).json();
  socket = new WebSocket(pages.find(page => page.type === 'page').webSocketDebuggerUrl);
  await new Promise((resolve, reject) => { socket.addEventListener('open', resolve, {once: true}); socket.addEventListener('error', reject, {once: true}); });
  socket.addEventListener('message', event => {
    const message = JSON.parse(event.data);
    const item = pending.get(message.id);
    if (!item) return;
    pending.delete(message.id); clearTimeout(item.timer);
    if (message.error) item.reject(Error(JSON.stringify(message.error))); else item.resolve(message.result);
  });
  await cdp('Page.enable');
  await cdp('Runtime.enable');
  await cdp('Page.navigate', {url: 'http://127.0.0.1:' + server.address().port + '/'});
  let ready = false;
  for (let attempt = 0; attempt < 100 && !ready; attempt++) {
    try {
      const state = await cdp('Runtime.evaluate', {expression: 'document.readyState === "complete" && location.pathname === "/"', returnByValue: true});
      ready = state.result.value;
    } catch (_) {}
    if (!ready) await delay(100);
  }
  if (!ready) throw Error('Private benchmark page did not become ready');
  const result = await cdp('Runtime.evaluate', {awaitPromise: true, returnByValue: true, expression: `(async () => {
    const response = await fetch('/matrix-benchmark.json');
    if (!response.ok) throw Error('Could not load benchmark manifest');
    const data = await response.json();
    const result = {browser: navigator.userAgent, examples: {}};
    const jobs = data.examples.flatMap(example => ['c', 'go', 'zx', 'wasm'].map(target => ({example, target})));
    // A deterministic mixed order avoids always warming one runtime last.
    let seed = 42;
    for (let i = jobs.length - 1; i > 0; i--) { seed = (seed * 1664525 + 1013904223) >>> 0; const j = seed % (i + 1); [jobs[i], jobs[j]] = [jobs[j], jobs[i]]; }
    for (const job of jobs) {
      const measured = await new Promise((resolve, reject) => {
        const worker = new Worker('/target-benchmark-worker.js');
        const timer = setTimeout(() => { worker.terminate(); reject(Error('Worker timeout: ' + job.example.id + '/' + job.target)); }, 30000);
        worker.onerror = event => { clearTimeout(timer); worker.terminate(); reject(Error(job.example.id + '/' + job.target + ': ' + event.message)); };
        worker.onmessage = event => { clearTimeout(timer); worker.terminate(); if (event.data.ok) resolve(event.data); else reject(Error(job.example.id + '/' + job.target + ': ' + event.data.message)); };
        worker.postMessage({...job, samples: data.samples});
      });
      (result.examples[job.example.id] ||= {})[job.target] = measured;
    }
    return result;
  })()`});
  if (result.exceptionDetails) throw Error(JSON.stringify(result.exceptionDetails));
  await fs.writeFile(output, JSON.stringify(result.result.value, null, 2) + '\n');
  console.log('Private browser: all 20 platform cells executed and their batch checksums verified');
} finally {
  if (socket && socket.readyState === WebSocket.OPEN) {
    try { await cdp('Browser.close'); } catch (_) {}
    socket.close();
  }
  try { process.kill(-browser.pid, 'SIGTERM'); } catch (_) {}
  server.closeAllConnections();
  await new Promise(resolve => server.close(resolve));
  await fs.rm(profile, {recursive: true, force: true, maxRetries: 5, retryDelay: 200});
}
