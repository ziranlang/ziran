const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const createPlayground = require('../site/assets/playground-runtime.js');

async function main() {
  let output = [], diagnostics = [], stages = [];
  global.postMessage = message => stages.push(message.stage);
  const runtime = await createPlayground({
    print: line => output.push(line),
    printErr: line => diagnostics.push(line)
  });
  function run(source) {
    output = []; diagnostics = []; stages = [];
    runtime.FS.writeFile('/playground.zi', source);
    const status = runtime.ccall('RunSource', 'number', [], []);
    return {status, output: output.join('\n'), diagnostics: diagnostics.join('\n'), stages};
  }
  const examples = [
    ['hello', 'Hello, World!'],
    ['score', '42'],
    ['text_demo', '1'],
    ['fizzbuzz', null],
    ['functions', null]
  ];
  for (const [name, expected] of examples) {
    const result = run(fs.readFileSync(path.join(__dirname, '../site/examples', name + '.zi'), 'utf8'));
    assert.equal(result.status, 0, name + ': ' + result.diagnostics);
    if (expected !== null) assert.equal(result.output, expected, name);
    assert.deepEqual(result.stages, ['check', 'build', 'run']);
  }
  assert.equal(run('Answer :: () -> s64 { return 9007199254740993; }').output, '9007199254740993');
  assert.equal(run('VALUE :: #run 1099511627776;\nAnswer :: () -> s64 { return VALUE; }').output, '1099511627776');
  assert.equal(run('main :: () { print("自然\\n"); }').output, '自然');
  assert.equal(run('main :: () { print("%\\n", 3.14159); }').output, '3.14159');
  const invalid = run('Answer :: () -> s32 { return missing_value; }');
  assert.notEqual(invalid.status, 0);
  assert.match(invalid.diagnostics, /missing_value/);
  assert.equal(invalid.output, '');
  const noEntry = run('Other :: () -> s32 { return 1; }');
  assert.notEqual(noEntry.status, 0);
  assert.match(noEntry.diagnostics, /entry procedure/);
  const bounded = run('main :: () { while true { } }');
  assert.notEqual(bounded.status, 0);
  assert.match(bounded.diagnostics, /portable execution failed/);
  assert.equal(run('Answer :: () -> s32 { return 17; }').output, '17', 'A failed run must not poison the next run');
  console.log('Playground runtime: examples, imports, 64-bit integers, compile-time values, UTF-8, floats, diagnostics, execution limits, and recovery passed');
}
main().catch(error => { console.error(error); process.exitCode = 1; });
