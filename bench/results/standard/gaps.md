# Where ZX native is slower than C

Standard suite at diagnostic sizes (`diagnostic-6a066abd.json`, measured before
the host gate), physical Alpha (Intel Core i7-8700), compiler
6a066abdb26d1579bcb1e786ccc42f50fa40f970, checks and learning off, 11 samples,
symmetric 1.5×IQR medians. The gated cohort at a later commit is in `diagnostic.json`. Times are µs per run.
The earlier compiler's receipt (`diagnostic-96c23c7d.json`) measured the same
Ziran sources; C medians agree between the two runs to within about 7%.

| Rank | Benchmark · input | C | Go | ZX native | Wasm | Native/C | Native/C at 96c23c7d | Wasm/C | Wasm/C at 96c23c7d |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 1 | n-body · 500,000 | 28,858 | 35,595 | 375,984 | 346,182 | 13.03× | 13.06× | 12.00× | 12.02× |
| 2 | spectral-norm · 500 | 10,073 | 15,985 | 118,649 | 48,446 | 11.78× | 10.85× | 4.81× | 4.30× |
| 3 | matmul · 192 | 3,818 | 9,053 | 24,695 | 14,966 | 6.47× | 4.48× | 3.92× | 3.86× |
| 4 | fib · 35 | 18,379 | 37,365 | 72,548 | 69,826 | 3.95× | 3.30× | 3.80× | 3.56× |
| 5 | fasta · 1,000,000 | 106,329 | 113,786 | 329,309 | 157,773 | 3.10× | 2.54× | 1.48× | 1.34× |
| 6 | fannkuch-redux · 9 | 14,944 | 14,951 | 34,899 | 26,545 | 2.34× | 1.50× | 1.78× | 1.35× |
| 7 | binary-trees · 14 | 90,023 | 81,111 | 208,999 | 98,584 | 2.32× | 2.30× | 1.10× | 0.97× |
| 8 | mandelbrot · 1,000 | 69,507 | 71,665 | 132,858 | 208,593 | 1.91× | 1.48× | 3.00× | 1.49× |
| 9 | nqueens · 12 | 5,109 | 5,407 | 8,581 | 7,193 | 1.68× | 1.73× | 1.41× | 1.56× |
| 10 | quicksort · 200,000 | 12,786 | 13,492 | 19,853 | 21,350 | 1.55× | 1.67× | 1.67× | 1.49× |
| 11 | string-hash · 100,000 | 5,456 | 5,469 | 6,222 | 6,342 | 1.14× | 1.63× | 1.16× | 1.80× |

## What the numbers establish

- **The native backend, not the program, is behind on five rows.** The same
  ZX bundle run as Wasm under V8 is much closer to C on spectral-norm (4.8×
  against 11.8×), fasta (1.5× against 3.1×), binary-trees (1.1× against 2.3×),
  matmul (3.9× against 6.5×) and fannkuch-redux (1.8× against 2.3×). Both run
  the same lowered program, so V8's code generation for it is the difference.
- **96c23c7d to 6a066abd regressed native on six rows** with unchanged Ziran
  sources: matmul 4.48× to 6.47×, fannkuch-redux 1.50× to 2.34×, mandelbrot
  1.48× to 1.91×, fib 3.30× to 3.95×, fasta 2.54× to 3.10× and spectral-norm
  10.85× to 11.78×. Wasm mandelbrot doubled (1.49× to 3.00×). string-hash,
  quicksort and nqueens improved. The range includes SSA argument repair and
  guarded loop rotation (a1299214), learned integer idioms (36e6caf9), record
  member placement and float constant folding (e991274a) and LIR loop/SIMD
  optimization (18fb5aeb). The loop-vectorizer lane separately isolated LICM
  hoisting cheap duplicate constants into long-lived values as the Vectors
  kernel's regression (85.8 µs to 42.5 µs with its profitability fix).
- **n-body is equally slow in native and Wasm (12–13×).** Ziran's `std/math`
  `Sqrt` is software (exponent split, six Newton steps, rescale), while C and
  Go use the hardware square root; n-body takes one per body pair per step.
  This source difference is established; its exact share needs a profile.

## First native backend A/B

The [constant division and register-copy fix](https://github.com/ziranlang/ziran/commit/426c7329289c548537cb7f1f73d7727f5183a907)
was measured against its published baseline on physical Alpha. The
[raw receipt](native-constant-divide-alpha.json) retains eleven interleaved
samples of C, before and after, on the same pinned core with the host clock
and sibling gate, learning off and every checksum matching C. Both native
runners use the same frozen ZX bundle. A private copy of the standard host
accepts one timed sample per process so the revisions can alternate; each
process keeps the standard 150 ms warmup. Loading and compilation are excluded.

| Benchmark | C µs | Native before µs | Native after µs | After/C |
| --- | ---: | ---: | ---: | ---: |
| n-body | 30,088 | 379,382 | 329,766 | 10.96× |
| spectral-norm | 10,451 | 111,529 | 69,195 | 6.62× |
| matmul | 3,915 | 21,538 | 20,928 | 5.35× |
| fib | 18,733 | 74,487 | 74,253 | 3.96× |
| mandelbrot | 70,433 | 119,730 | 118,524 | 1.68× |

Native n-body is 13.1% faster and spectral-norm is 38.0% faster in this
comparison. The other rows show little change. Inspection found SSE2 scalar
doubles already in use, with no x87 arithmetic. Spectral-norm's `A(i,j)`
still calls out of the inner loop and spills live XMM values, but its division
by two now uses shifts and a signed bias instead of `idiv`; n-body's software
square root also divides its exponent by two. The allocator fix removes
invalid copies between unrelated layout-adjacent block parts and allows the
neighboring inliner to preserve floating-point results. Hardware square root,
call elimination, matrix induction and recursive-call overhead remain open.

## Hardware square root A/B

The [hardware square root change](https://github.com/ziranlang/ziran/commit/515449986e40fd9da7fb324f96cbf6d1c906a696)
was measured against the preceding native division fix. The
[raw receipt](native-hardware-sqrt-alpha.json) retains eleven interleaved
C/before/after samples on physical Alpha, with the standard host gate,
150 ms process warmup, learning and checks off, symmetric inclusive 1.5×IQR
exclusions, and every result matching C. This uses the standard workloads and
host through a private interleaving driver, rather than the literal
`bench/wasm_tier.py --standard` command. The native bundle was rebuilt for the
new square root intrinsic; the unchanged benchmark kernel source and both
bundle/tool fingerprints are retained in the receipt.

| Benchmark | C µs | Native before µs | Native after µs | After/C |
| --- | ---: | ---: | ---: | ---: |
| n-body | 29,676 | 324,196 | 46,806 | 1.58× |
| spectral-norm | 9,985 | 67,468 | 68,312 | 6.84× |
| matmul | 3,915 | 20,421 | 20,491 | 5.23× |
| fib | 18,304 | 73,836 | 72,675 | 3.97× |
| mandelbrot | 69,164 | 115,857 | 117,005 | 1.69× |

Replacing normalization and six Newton divisions with `sqrtsd` makes n-body
6.93× faster in this comparison. The other kernels do not use square root in
their hot loops and show little change. The earlier software-square-root
observation describes the baseline; the current compiler and standard math
module use the hardware operation.

## Recursive entry and address instruction A/B

The [native call setup change](https://github.com/ziranlang/ziran/commit/84a89f4c8ee2359d8677b2601d9708c84b5d38fd)
was measured against its immediate published parent, after the intervening
CFG inliner changes. The [raw receipt](native-call-setup-alpha.json) uses the
same interleaving procedure and eleven samples per target. Before and after
run one frozen ZX bundle, with all checksums matching C. The runtime and
compiler build metadata are retained separately for each revision.

| Benchmark | C µs | Native before µs | Native after µs | After/C |
| --- | ---: | ---: | ---: | ---: |
| n-body | 28,748 | 61,768 | 62,407 | 2.17× |
| spectral-norm | 10,011 | 69,955 | 69,980 | 6.99× |
| matmul | 3,894 | 21,426 | 20,809 | 5.34× |
| fib | 18,006 | 71,639 | 67,015 | 3.72× |
| mandelbrot | 69,451 | 115,573 | 117,013 | 1.68× |

Simple integer base cases return before saving callee-saved registers and
allocating a frame; effectful, checked and fueled paths retain their ordinary
entry. Adjacent integer register copies and constant additions/subtractions
use one `lea`. Fib is 6.46% faster, while its remaining recursive calls still
need frames and saved values. GCC also transforms recursive accumulation and
partially inlines recursion, which this backend change does not implement.

N-body is slower in this later cohort than in the hardware-square-root
cohort, including the before revision. The intervening compiler changes need
a separate matched comparison; these two cohorts do not identify which
change caused that regression. They must not be combined into a single
before/after claim for the call setup change.

Local validation before the source changes were pushed: `build-queue -- make
check CHECK_FILTER=zx_native`, encoder and lowered execution parity fixtures,
then the full `make check` (284/284) and `make check-bootstrap` (Linux and
Darwin bootstrap comparisons match), all with learning off and desktop display
variables removed. The final full-suite and bootstrap commands shared one
build-queue slot. These are local test results; GitHub CI was not inspected.

## Hypotheses to test (timing alone does not prove them)

1. **n-body, spectral-norm:** float-heavy inner loops. Beyond `Sqrt`, native
   scalar float register allocation and int-to-float conversion of loop
   indices (spectral-norm's `A(i,j)`) are candidates; Wasm's 4.8× on
   spectral-norm bounds what better native code could reach without SIMD.
2. **matmul, fannkuch-redux, mandelbrot:** array-indexed loops that regressed
   in this range. Candidates: LICM register pressure and spills (as found for
   Vectors), loop rotation layout, address strength reduction.
3. **fib, nqueens, quicksort:** recursion. gcc -O2 partially inlines and
   splits the recursive calls; native call/prologue cost is the candidate.
4. **fasta, binary-trees:** remainder in the LCG, cumulative probability scan
   and per-node allocation/free. Wasm being close to C points at native
   lowering of these operations rather than the allocator design.
5. **string-hash** is near parity (1.14×): FNV-1a is a serial multiply chain,
   so there is little left beyond instruction selection.

## Routing

- Loop rotation and LIR SSA: matmul, fannkuch-redux, mandelbrot, spectral-norm
  loops and the regression range above.
- Records and aggregates: n-body body records, binary-trees nodes.
- Loop vectorizer: the LICM regression on matmul, fannkuch-redux and
  mandelbrot, plus spectral-norm and mandelbrot as SIMD candidates (no column
  uses hand-written SIMD).
- C-teacher learning: every row, comparing gcc -O2 assembly with native
  output, starting with spectral-norm, fasta and binary-trees where Wasm
  already shows the native gap is code generation.
