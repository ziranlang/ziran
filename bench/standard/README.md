# Standard scalar language benchmarks

Run on Alpha's physical host, from the canonical checkout, through its queue:

```
env -u DISPLAY -u WAYLAND_DISPLAY ZIRAN_LEARN=off build-queue -- \
  python3 bench/wasm_tier.py --standard --host-gate --samples 11 \
  --output bench/results/standard/diagnostic.json
```

The registered entry point is `bench/bench.zi -- wasm-tier --standard` (using
`build/bin/ziran run --target=c` as documented in bench/README.md).
Add `--standard-full` for the Benchmarks Game performance sizes. The default
is a smaller diagnostic set, explicitly identified in its receipt; results at
different sizes must never be compared as if they performed the same work.

| Original workload | Diagnostic input | Game performance input |
| --- | ---: | ---: |
| Recursive Fibonacci | 35 | 35 |
| n-body | 500,000 steps | 50,000,000 steps |
| spectral-norm | 500 | 5,500 |
| mandelbrot | 1,000 square | 16,000 square |
| fannkuch-redux | 9 | 12 |
| binary-trees | 14 | 21 |
| fasta | 1,000,000 | 25,000,000 |
| nqueens | 12 | 12 |
| integer matrix multiplication | 192 square | 192 square |
| quicksort | 200,000 integers | 200,000 integers |
| string seeded FNV-1a hash | 100,000 passes | 100,000 passes |

The [workload catalog](../standard_cases.zi) lists all 29 cases, their probe
and diagnostic sizes, and the larger preset sizes where available.

For cooperative queue use, `--standard-case-limit 1` saves a partial receipt
and exits successfully after each completed workload. Continue with
`--standard-resume --standard-case-limit 1` and the same output path and size
preset. Resume refuses changes to workload sources or measured artifacts. It retains
the original compiler revision and tool/runtime hashes from artifact creation.
The native runner statically includes its runtime (its only shared library is
libc), and the Wasm module is precompiled, so subsequent checkout/compiler
updates cannot affect the retained executables. Never reset the checkout while
a row is actively measuring. Each invocation runs
through build-queue independently, allowing other queued work between rows.
Only a receipt containing all workloads is published. Partial receipts
are never accepted by the renderer.

The separate `standard-compile` driver measures one complete build of the whole
suite per column:

```
env -u DISPLAY -u WAYLAND_DISPLAY build-queue -- \
  build/bin/ziran run --target=c bench/bench.zi -- standard-compile
```

Run `standard-compile --validate` on Alpha first. The five columns are C
(`cc -O2`), ordinary Go, generated C including `cc -O2` and linking, native ZX,
and Wasm. Native timing includes the frontend, native host compilation/linking,
and instantiation that actually compiles the native program. Wasm timing includes
the frontend and module emission; V8 compilation is excluded. Every artifact
runs a C-verified probe after its build, outside build timing. All 29 probes
run on the final artifact in each column. One warmup precedes
11 samples with rotating column order and the shared IQR policy. Generated C
outputs are removed before each sample; a source comment changes before each Go
build to prevent application-cache hits, and Go runs with `GOMAXPROCS=2`.
Toolchain and filesystem caches stay warm. Source/tool hashes are checked before and after the run. There is no fast
compiler mode column because the current compiler provides no such switch.

The Game kernels follow its published [task descriptions](https://benchmarksgame-team.pages.debian.net/benchmarksgame/description/summary.html):
[n-body](https://benchmarksgame-team.pages.debian.net/benchmarksgame/description/nbody.html),
[spectral-norm](https://benchmarksgame-team.pages.debian.net/benchmarksgame/description/spectralnorm.html),
[mandelbrot](https://benchmarksgame-team.pages.debian.net/benchmarksgame/description/mandelbrot.html),
[fannkuch-redux](https://benchmarksgame-team.pages.debian.net/benchmarksgame/description/fannkuchredux.html),
[binary-trees](https://benchmarksgame-team.pages.debian.net/benchmarksgame/description/binarytrees.html),
[fasta](https://benchmarksgame-team.pages.debian.net/benchmarksgame/description/fasta.html).
These are scalar compute comparisons, not submissions to the Game's leaderboard:
formatting and writing large output files are replaced with digest accumulation.
No hand-tuned parallel or SIMD implementation is selected in any column.
The supplemental workloads, including reverse-complement and k-nucleotide,
use explicit deterministic inputs at the sizes recorded in the catalog.

`kernels.zi` dispatches to the Ziran implementations in this directory.
`entry.zi` obtains both workload and
size through a host capability. C and Go references independently implement the
same algorithms and read both arguments at runtime. The native and Wasm hosts
supply the same values through the capability; nothing embeds a fixed answer.
The former compile-time `Calls = Fib(20)` row was removed from wasm-tier;
recursive Fibonacci is now measured here with a runtime input.

The native host explicitly requires the native backend and rejects an engine
fallback. Native compilation/instantiation and Wasm/V8 compilation happen before
warmup. C is compiled with `cc -O2`, without LTO; Go uses ordinary `go build`.
Checks and learning are off. All targets run sequentially in one build-queue slot.
After at least 150 ms warmup, exactly 11 single-call samples are collected.
Process startup is excluded for every column. Samples use the shared symmetric
1.5×IQR policy with linearly interpolated inclusive quartiles. Raw samples and
excluded indices are retained. Tiny probe inputs are checked before the measured
size. Every invocation's result, including warmup and every timed sample, is
compared with the C oracle; a mismatch stops the suite.

The receipt retains the execution host for provenance. The measured commit names the
compiler (`compiler_dirty` records whether its sources were clean); source,
tool, runtime and artifact hashes identify the suite and implementation that
ran. The site renders only receipts whose commit matches the other published
tables (`site/generated/targets/matrix.json`), so every table on /targets
shares one compiler.

Retained results in `bench/results/standard/`: `diagnostic.json` and
`compile.json` at the published commit;
`diagnostic-96c23c7d.json` and the five-row `game-performance-96c23c7d.partial.json`
from the earlier compiler, with `sources-96c23c7d.zip` holding the suite sources
they measured (the C and Go references were later reformatted only), and
`gaps.md`, the ranked native-versus-C gaps.

Output contracts:

* Fibonacci returns the integer result; nqueens returns solution count.
* n-body returns final energy rounded to nine decimal places; spectral-norm
  returns the norm rounded to nine decimals, as signed integer nanounits.
* Mandelbrot computes the standard 50-iteration PBM body, including last-byte
  padding, then accumulates `h = (31*h + byte) mod 1000000007`.
* FASTA generates the ALU repeat (2n), IUB (3n), then Homo sapiens (5n), using
  seed 42 and the specified LCG and cumulative probability search. It digests
  sequence bytes with the same polynomial, excluding headers/newlines.
* Fannkuch returns `1000*alternating_checksum + maximum_flips` (both values
  remain represented for these input ranges).
* Binary trees allocate every leaf and interior node, retain the long-lived
  tree until the final check, and sum stretch, iteration and long-lived node
  counts. C/Ziran free per node; Go uses its default GC.
* Matrix multiplication uses the same i/k/j order and integer values, returning
  the sum of every output cell. Quicksort sorts the same 31-bit LCG values and
  digests the sorted result. Hash uses wrapping 64-bit FNV-1a multiplication with the existing wasm-tier seed
  1469598103934665603,
  returning its unsigned remainder modulo 1000000007.
