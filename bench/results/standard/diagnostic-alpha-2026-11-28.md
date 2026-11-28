# Physical Alpha benchmark table — 2026-11-28

Fresh measurements on **alpha**, **Intel(R) Core(TM) i7-8700 CPU @ 3.20GHz**, virtualization **none**.
Completed 2026-11-28T09:22:48.454625+00:00; compiler revision `51c746b3c8124830d690ad17624b0e7c9594aebc`;
compiler working tree dirty: **true**. The current tree includes an uncommitted Makefile build fix and an unused decimal module, copied from Omega before building.

All **29 standard diagnostic workloads**, four implementations, **11 measured calls per cell** after at least 150 ms warmup.
Times are **median microseconds per call; lower is faster**. Bold identifies the lowest filtered median per row; small differences can be clock noise.
ZX native/C is a time ratio: below 1 means ZX native ran faster than C.

| Workload | Input | C (µs) | Go (µs) | ZX native (µs) | Wasm/V8 (µs) | ZX native/C |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| fib | 35 | **15,478.611** | 36,940.393 | 37,394.418 | 40,587.579 | 2.416× |
| n-body | 500,000 | **29,302.026** | 35,034.203 | 61,708.176 | 39,050.082 | 2.106× |
| spectral-norm | 500 | **10,205.622** | 16,037.278 | 14,853.919 | 14,842.112 | 1.455× |
| mandelbrot | 1,000 | **69,184.588** | 71,805.959 | 138,816.329 | 73,060.058 | 2.006× |
| fannkuch-redux | 9 | 15,235.726 | 15,852.786 | **14,376.324** | 21,074.744 | 0.944× |
| binary-trees | 14 | 89,476.183 | 105,023.753 | **37,104.332** | 96,388.291 | 0.415× |
| fasta | 1,000,000 | **105,860.295** | 114,157.632 | 121,004.587 | 126,705.553 | 1.143× |
| nqueens | 12 | 5,512.450 | **5,478.478** | 6,632.731 | 6,252.030 | 1.203× |
| matmul | 192 | **3,903.588** | 9,187.917 | 9,996.567 | 9,291.783 | 2.561× |
| quicksort | 200,000 | **12,279.661** | 13,315.377 | 12,331.563 | 16,498.653 | 1.004× |
| string-hash | 100,000 | **5,256.416** | 5,347.978 | 5,324.328 | 5,397.819 | 1.013× |
| k-nucleotide | 1,000,000 | 2,137.218 | **2,115.416** | 3,345.476 | 2,808.438 | 1.565× |
| reverse-complement | 1,000,000 | 5,134.698 | **4,795.389** | 6,272.910 | 7,120.998 | 1.222× |
| pidigits | 1,000 | **33,951.130** | 34,376.834 | 48,241.481 | 48,060.473 | 1.421× |
| json-roundtrip | 16,000 | **3,692.688** | 4,354.354 | 6,457.003 | 6,498.351 | 1.749× |
| lz77 | 262,144 | **926.316** | 1,057.417 | 1,353.625 | 1,331.458 | 1.461× |
| sha256 | 262,144 | **1,167.047** | 1,399.013 | 3,114.688 | 2,225.613 | 2.669× |
| crc32 | 1,048,576 | 2,807.806 | **2,687.561** | 4,491.642 | 3,621.073 | 1.600× |
| ray-trace | 192 | **4,286.669** | 4,738.436 | 6,629.383 | 16,092.532 | 1.547× |
| sudoku | 9 | 20,816.750 | **17,941.934** | 22,013.671 | 24,099.582 | 1.057× |
| dijkstra | 48 | 4,077.662 | 5,203.246 | 7,990.626 | **3,919.174** | 1.960× |
| word-count | 200,000 | 5,280.353 | **5,070.412** | 8,160.015 | 6,162.904 | 1.545× |
| radix-sort | 262,144 | 9,518.089 | **7,528.082** | 10,126.391 | 8,997.450 | 1.064× |
| lu | 192 | **1,271.654** | 1,862.428 | 2,974.953 | 2,906.827 | 2.339× |
| fft | 14 | **712.080** | 842.848 | 1,326.782 | 1,115.653 | 1.863× |
| bytecode | 12,000 | 4,940.206 | **4,694.165** | 7,880.629 | 7,581.407 | 1.595× |
| heat-grid | 192 | **1,372.223** | 2,319.007 | 2,182.529 | 2,681.613 | 1.591× |
| particle-update | 16,384 | **3,436.687** | 4,363.679 | 5,995.908 | 5,629.397 | 1.745× |
| image-filter | 512 | **2,189.332** | 3,197.426 | 3,794.316 | 4,182.147 | 1.733× |

Geometric mean ZX native/C time ratio: **1.492×** across all 29 rows.
Lowest-median counts: C 18, Go 8, ZX native 2, Wasm/V8 1.
These counts do not establish statistical significance or a general language ranking.

Every warmup and measured result was checked against the independent C reference. All 116 cells passed the host gate: quiet-core pinning, clocks within 3%, at least 95% of the calibrated sustained clock, sibling activity at most 15%.
Symmetric 1.5×IQR filtering uses inclusive interpolated quartiles. Raw samples, excluded indices, and rejected host-gate attempts with their measurements remain in the receipt.
There were **98 IQR sample exclusions** and **8 rejected host-gate attempts out of 124 attempts**.

C uses `cc -O2`; Go uses ordinary `go build` and `GOMAXPROCS=1` during measurement. ZX explicitly selects native code; Wasm runs in V8 without a fuel budget.
Compilation, instantiation, loading and process startup are excluded. Binary trees include allocation and reclamation; Go uses its default GC.
Learning and native disk caches were off. Diagnostic inputs differ from the larger Benchmarks Game performance preset. These scalar algorithms are not leaderboard submissions.

Command in Alpha's canonical `/home/wao/Projects/ziranlang/ziran` checkout:

```sh
env -u DISPLAY -u WAYLAND_DISPLAY ZIRAN_LEARN=off ZIRAN_NATIVE_CACHE=off build/bin/ziran run --target=c bench/bench.zi -- wasm-tier --standard --host-gate --samples 11 --output bench/results/standard/diagnostic-alpha-2026-11-28.json
```

The current tree was rebuilt on physical Alpha with `/home/wao/.local/bin/make -s -j2 CC=/usr/bin/cc HOST_CC=/usr/bin/cc all`. The system compiler bypassed stale wrappers pointing to the removed Harmony build queue.

[Raw JSON receipt](diagnostic-alpha-2026-11-28.json) includes source, tool, runtime and frozen artifact hashes.
Receipt SHA-256: `87d9b6f3edf92f06c2ddb14f841eba61954acf8c3b528edc6b60f64197d31707`.

Local verification independently recalculated all 116 filtered medians and exclusion lists, matched workload names and input sizes to the catalog, and checked checksums, sample counts, host provenance and host-gate acceptance.
