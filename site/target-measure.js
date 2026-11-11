(function () {
  function measure(invoke, expected, samples) {
    const answer = Number(expected);
    function batch(count) {
      let checksum = 0;
      const started = performance.now();
      for (let i = 0; i < count; i++) checksum += Number(invoke());
      const elapsed = performance.now() - started;
      if (checksum !== answer * count) throw new Error('Benchmark result differs from its oracle.');
      return elapsed;
    }
    if (Number(invoke()) !== answer) throw new Error('First result differs from its oracle.');
    const started = performance.now();
    let warmup = 0;
    do { batch(1); warmup++; } while (performance.now() - started < 150);
    const warmupMs = performance.now() - started;
    let batches = 1;
    while (batches < 65536 && batch(batches) < 10) batches *= 2;
    const times = [];
    for (let i = 0; i < samples; i++) times.push(batch(batches));
    return {result: String(expected), batches, warmup_ms: warmupMs, warmup_count: warmup, batch_samples_ms: times};
  }
  globalThis.matrixMeasure = measure;
  if (typeof module !== 'undefined') module.exports = measure;
})();
