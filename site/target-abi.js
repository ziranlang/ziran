/* zi2wasm's budget and builtin-output imports. Shared by Node and workers. */
(function (root) {
  function floatText(value, single) {
    if (Number.isNaN(value)) return 'nan';
    if (!Number.isFinite(value)) return value < 0 ? '-inf' : 'inf';
    let scientific;
    for (let precision = 1; precision <= 17; precision++) {
      scientific = value.toExponential(precision - 1);
      if (single ? Math.fround(Number(scientific)) === value : Number(scientific) === value) break;
    }
    if (Object.is(value, -0)) scientific = '-' + scientific;
    const [coefficient, power] = scientific.split('e');
    const negative = coefficient.startsWith('-');
    const digits = coefficient.replace(/[-.]/g, '').replace(/0+$/, '') || '0';
    const point = Number(power) + 1;
    const text = point <= 0 ? '0.' + '0'.repeat(-point) + digits
      : point >= digits.length ? digits + '0'.repeat(point - digits.length)
        : digits.slice(0, point) + '.' + digits.slice(point);
    return ((negative ? '-' : '') + text).slice(0, 63);
  }
  function create(output) {
    let memory;
    const encoder = new TextEncoder();
    const bits = new DataView(new ArrayBuffer(8));
    const budget = new WebAssembly.Global({value: 'i64', mutable: true}, 10000000n);
    return {
      imports: {zx: {budget, overrun() { throw Error('Execution budget exceeded'); },
        host_i64(index, value) {
          let text;
          switch (index >>> 0) {
            case 0xffffffff: {
              const handle = BigInt.asUintN(64, value);
              const address = Number(handle & 0xffffffffn), length = Number(handle >> 32n);
              if (!memory || address + length > memory.buffer.byteLength) throw Error('Invalid output string');
              output(new Uint8Array(memory.buffer, address, length));
              return 0n;
            }
            case 0xfffffffe: text = BigInt.asIntN(64, value).toString(); break;
            case 0xfffffffd: text = BigInt.asUintN(64, value).toString(); break;
            case 0xfffffffc: bits.setBigInt64(0, value, true); text = floatText(bits.getFloat64(0, true), false); break;
            case 0xfffffffb: bits.setUint32(0, Number(value & 0xffffffffn), true); text = floatText(bits.getFloat32(0, true), true); break;
            case 0xfffffffa: text = value ? 'true' : 'false'; break;
            default: throw Error('Unknown host capability');
          }
          output(encoder.encode(text));
          return 0n;
        }}},
      attach(instance) { memory = instance.exports.memory; },
      resetBudget() { budget.value = 10000000n; },
    };
  }
  root.matrixABI = create;
  if (typeof module !== 'undefined') module.exports = create;
})(globalThis);
