const assert = require('node:assert/strict');
const createPlayground = require(process.env.ZIRAN_PLAYGROUND_RUNTIME || '../site/assets/playground-runtime.js');

async function main() {
  let diagnostics = [];
  const runtime = await createPlayground({printErr: line => diagnostics.push(line)});
  runtime.FS.mkdir('/sources');
  runtime.FS.mkdir('/library');
  runtime.FS.writeFile('/library/math.zi', '#program_export\nDouble :: (value: s32) -> s32 { return value * 2; }');
  function build(source, entry = 'Answer') {
    diagnostics = [];
    runtime.FS.writeFile('/sources/app.zi', source);
    return runtime.ccall('BuildBundle', 'number', Array(6).fill('string'),
      ['/sources', '/std\nexample=/library', '/sources/app.zi', 'app', entry, '/result.zx']);
  }
  assert.equal(build('#import "example/math"\nAnswer :: () -> s32 { return Double(21); }'), 0, diagnostics.join('\n'));
  const first = runtime.FS.readFile('/result.zx');
  assert.ok(first.length > 100);
  assert.equal(build('#import "example/math"\nAnswer :: () -> s32 { return Double(21); }'), 0);
  assert.deepEqual(runtime.FS.readFile('/result.zx'), first, 'Repeat compilations produce the same bundle');
  assert.notEqual(build('Answer :: () -> s32 { return missing_value; }'), 0);
  assert.match(diagnostics.join('\n'), /missing_value/);
  assert.throws(() => runtime.FS.readFile('/result.zx'), 'A failed build must remove the old output');
  assert.notEqual(build('Other :: () -> s32 { return 1; }'), 0);
  assert.equal(build('Answer :: () -> s64 { return 9007199254740993; }'), 0, diagnostics.join('\n'));
  assert.equal(build('#import "std/text"\nAnswer :: () -> s32 { return cast(s32)StartsWithFoldASCII("自然", "自"); }'), 0, diagnostics.join('\n'));
  // Exercise the public six-string ABI in real Wasm, beyond both old caps.
  const roots = Array.from({length: 273}, (_, i) => `/root${i}`);
  for (const [i, root] of roots.entries()) {
    runtime.FS.mkdir(root);
    runtime.FS.writeFile(`${root}/Marker${i}.zi`, `Marker :: ${i};`);
  }
  runtime.FS.writeFile(`${roots[64]}/R64.zi`, 'Value64 :: () -> s32 { return 20; }');
  runtime.FS.writeFile(`${roots[272]}/R272.zi`, 'Value272 :: () -> s32 { return 21; }');
  runtime.FS.writeFile(`${roots[0]}/Winner.zi`, 'First :: () -> s32 { return 1; }');
  runtime.FS.writeFile(`${roots[272]}/Winner.zi`, 'First :: () -> s32 { return wrong_root; }');
  runtime.FS.mkdir('/inputs');
  const inputs = Array.from({length: 273}, (_, i) => `/inputs/Input${i}.zi`);
  for (let i = 0; i < inputs.length; i++)
    runtime.FS.writeFile(inputs[i], `Unused${i} :: ${i};`);
  runtime.FS.writeFile(inputs[272], '#import "R64"\n#import "R272"\n#import "Winner"\nAnswer :: () -> s32 { return Value64() + Value272() + First(); }');
  function many(search = roots, files = inputs) {
    diagnostics = [];
    return runtime.ccall('BuildBundle', 'number', Array(6).fill('string'),
      ['/sources', search.join('\n'), files.join('\n'), 'Input272', 'Answer', '/result.zx']);
  }
  assert.equal(many(), 0, diagnostics.join('\n'));
  const large = runtime.FS.readFile('/result.zx');
  assert.ok(large.length > 100);
  // Empty lines retain their existing separator semantics.
  assert.equal(many(['', ...roots, ''], ['', ...inputs, '']), 0, diagnostics.join('\n'));
  assert.deepEqual(runtime.FS.readFile('/result.zx'), large);
  const fails = (search, files, diagnostic) => {
    assert.notEqual(many(search, files), 0);
    if (diagnostic) assert.match(diagnostics.join('\n'), diagnostic);
    assert.throws(() => runtime.FS.readFile('/result.zx'));
    assert.equal(many(), 0, diagnostics.join('\n'));
    assert.deepEqual(runtime.FS.readFile('/result.zx'), large);
  };
  for (let repeat = 0; repeat < 3; repeat++) {
    fails([...roots, '/missing-root'], inputs, /module search path is unavailable/);
    fails(roots, [...inputs, '/inputs/missing.zi'], /missing/);
    fails([...roots].reverse(), inputs, /wrong_root/);
    fails(roots, []);
  }
  if (process.env.ZIRAN_BUNDLE_OUTPUT)
    require('node:fs').writeFileSync(process.env.ZIRAN_BUNDLE_OUTPUT, large);
  console.log('Web BuildBundle: 273 roots and inputs, last-root/last-input resolution, first-root ordering, empty separators, diagnostics, stale-output removal, repeated error recovery passed');
  console.log('Browser bundle compiler: named imports, deterministic output, diagnostics, 64-bit values, UTF-8, stale-output removal, and recovery passed');
}
main().catch(error => { console.error(error); process.exitCode = 1; });
