const assert = require('node:assert/strict');
const createPlayground = require('../site/assets/playground-runtime.js');

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
      ['/sources', '/std\nexample=/library', '/sources/app.zi', 'app', entry, '/result.zib']);
  }
  assert.equal(build('#import "example/math"\nAnswer :: () -> s32 { return Double(21); }'), 0, diagnostics.join('\n'));
  const first = runtime.FS.readFile('/result.zib');
  assert.ok(first.length > 100);
  assert.equal(build('#import "example/math"\nAnswer :: () -> s32 { return Double(21); }'), 0);
  assert.deepEqual(runtime.FS.readFile('/result.zib'), first, 'Repeat compilations produce the same bundle');
  assert.notEqual(build('Answer :: () -> s32 { return missing_value; }'), 0);
  assert.match(diagnostics.join('\n'), /missing_value/);
  assert.throws(() => runtime.FS.readFile('/result.zib'), 'A failed build must remove the old output');
  assert.notEqual(build('Other :: () -> s32 { return 1; }'), 0);
  assert.equal(build('Answer :: () -> s64 { return 9007199254740993; }'), 0, diagnostics.join('\n'));
  assert.equal(build('#import "std/text"\nAnswer :: () -> s32 { return cast(s32)StartsWithFoldASCII("自然", "自"); }'), 0, diagnostics.join('\n'));
  console.log('Browser bundle compiler: named imports, deterministic output, diagnostics, 64-bit values, UTF-8, stale-output removal, and recovery passed');
}
main().catch(error => { console.error(error); process.exitCode = 1; });
