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
    return runtime.ccall('BuildBundle', 'number', Array(7).fill('string'),
      ['/sources', '/std\nexample=/library', '/sources/app.zi', 'app', entry, '', '/result.zx']);
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
  runtime.FS.writeFile('/sources/capability.zi', 'host :: #system_library "host_api";\nCompute :: (value: s32) -> s32 #foreign host;\n#program_export\nAnswer :: () -> s32 { return Compute(40); }\n');
  const provider = 'interface Props { value: s32; }\nfunction Echo(props: Props): s32 { return props.value; }\nexport function Compute(value: s32): s32 {\n return <Echo value={value + s32(2)}/>;\n}\n';
  function bound(source) {
    diagnostics = [];
    runtime.FS.writeFile('/sources/provider.tsx', source);
    return runtime.ccall('BuildBundle', 'number', Array(7).fill('string'),
      ['/sources', '/std', '/sources/capability.zi\n/sources/provider.tsx', 'capability', 'Answer',
       'capability:Compute=provider:Compute', '/result.zx']);
  }
  assert.equal(bound(provider), 0, diagnostics.join('\n'));
  const typed = runtime.FS.readFile('/result.zx');
  assert.ok(typed.length > 100);
  assert.notEqual(bound('export function Compute(value: number): number { return value; }'), 0);
  assert.match(diagnostics.join('\n'), /signature mismatch/);
  assert.throws(() => runtime.FS.readFile('/result.zx'));
  assert.notEqual(bound(provider.replace('value={value', 'misspelled={value')), 0);
  assert.match(diagnostics.join('\n'), /provider\.tsx:4:\d+/);
  assert.match(diagnostics.join('\n'), /misspelled/);
  assert.throws(() => runtime.FS.readFile('/result.zx'));
  assert.equal(bound(provider), 0, diagnostics.join('\n'));
  assert.deepEqual(runtime.FS.readFile('/result.zx'), typed);
  // Exercise the public seven-string ABI in real Wasm, beyond both old caps.
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
    return runtime.ccall('BuildBundle', 'number', Array(7).fill('string'),
      ['/sources', search.join('\n'), files.join('\n'), 'Input272', 'Answer', '', '/result.zx']);
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
  console.log('Browser bundle compiler: checked TSX providers, signature rejection, authored diagnostics, named imports, deterministic output, 64-bit values, UTF-8, stale-output removal and recovery passed');
}
main().catch(error => { console.error(error); process.exitCode = 1; });
