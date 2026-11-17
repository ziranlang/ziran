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

## Scalar conversion dependency A/B

The [scalar conversion change](https://github.com/ziranlang/ziran/commit/d647525b07629691f83ea5836349494f4c8d4730)
was measured against its immediate published parent. The
[raw receipt](native-conversion-dependency-alpha.json) retains eleven
interleaved C/before/after samples, the standard host gate, all raw samples and
symmetric inclusive 1.5×IQR exclusions. The same frozen ZX bundle used by the
call setup experiment runs on both revisions; learning and checks are off,
and all 165 timed results match C. As above, the private driver uses the
standard workloads and host to alternate the two native runtimes.

| Benchmark | C µs | Native before µs | Native after µs | After/C |
| --- | ---: | ---: | ---: | ---: |
| n-body | 29,131 | 62,155 | 62,570 | 2.15× |
| spectral-norm | 10,008 | 69,923 | 60,243 | 6.02× |
| matmul | 3,839 | 20,957 | 18,824 | 4.90× |
| fib | 18,393 | 66,763 | 67,785 | 3.69× |
| mandelbrot | 69,969 | 118,559 | 116,022 | 1.66× |

Spectral-norm takes 13.84% less time. Its legacy `cvtsi2sd` instruction
preserves the unused upper lanes of its destination and can therefore wait
on an earlier floating-point value. A preceding `xorps` clears those lanes
without changing the scalar result or integer condition flags. This applies
to integer conversions to both float32 and float64, including both paths of
the unsigned-64 conversion. The exact encoder fixtures cover high registers,
measurement/write agreement and output limits; execution parity checks signed
and unsigned rounding boundaries and random integers against an integer
round-to-nearest, ties-to-even reference. The full local suite passed 284/284
in 213.9 seconds and bootstrap comparisons matched before the source push.
GitHub CI was not inspected.

Matmul also measures faster in this cohort, but its integer hot loop has no
integer-to-float conversion. That timing difference is retained without
attributing it to the conversion dependency fix; code layout and run-to-run
variation need separate isolation. The smaller differences on the other rows
are not evidence of improvements to their hot loops.

Assembly research at the conversion revision identified larger remaining problems:

- Spectral-norm retained calls to `A` and spills around them. In representative
  standalone output at that revision, the CFG inliner copies the original `Multiply` into
  `Spectral`, retaining its `A` calls, while separately expanding `A` inside
  the original `Multiply`. The pass explicitly expands one level from input
  callees. Optimizing callees before copying them is a separate optimizer
  change, reported to that lane. The subsequent
  [single-block inliner fix](https://github.com/ziranlang/ziran/commit/4a488a44f32e01f7cc9e9cdd90b6efff9272e139)
  and [callee-first expansion](https://github.com/ziranlang/ziran/commit/1ac1653faa89ad65966ef395ad68a0b706f1dc4c)
  address that ordering. A later whole-source Alpha cohort must measure their
  effect; the frozen-bundle native A/B results here do not measure those
  frontend changes.
- Native matrix multiplication repeatedly spills general registers in the
  inner loop and reloads `a[i*n+k]`, while GCC keeps that reused element in a
  register and advances row pointers. Loop liveness, invariant-load legality
  and induction-variable selection remain open.
- Fib now returns its simple base case before the frame, but still makes both
  recursive calls. GCC's recursive accumulation and partial inlining reduce
  that overhead further; they require a distinct optimizer transformation.

Primary implementation references:
[LLVM's dependency-breaking example](https://llvm.org/doxygen/TargetInstrInfo_8h_source.html)
and [x86 implementation](https://llvm.org/doxygen/X86InstrInfo_8cpp_source.html)
explain zeroing before scalar conversions.
[GCC's optimization documentation](https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html)
separately describes prologue placement, induction-variable optimization and
using registers that a known callee does not overwrite. These are engineering
leads; only the matched measurements above establish the changes' effect here.

## Rejected tied argument-register preference

A fully locally validated [allocator candidate](https://github.com/ziranlang/ziran/commit/b9b66c1b8a7c44f815c073175192fd803f1a77d2)
propagated a result's fixed argument-register preference into its tied input
copy, only when that register stayed free for the full result interval. The
[Alpha receipt](native-tied-argument-alpha.json) retains eleven interleaved
C/before/after samples at the immediate published parent and candidate, using
the same frozen ZX bundle, standard host/gate and private interleaving driver
as above. Learning and checks are off; all 165 timed results match C.

| Benchmark | C µs | Native before µs | Candidate µs | Candidate/C |
| --- | ---: | ---: | ---: | ---: |
| n-body | 29,550 | 62,154 | 62,030 | 2.10× |
| spectral-norm | 10,047 | 60,740 | 58,228 | 5.80× |
| matmul | 3,877 | 19,014 | 18,780 | 4.84× |
| fib | 19,017 | 71,051 | 80,428 | 4.23× |
| mandelbrot | 68,869 | 116,606 | 116,029 | 1.68× |

Standalone fib assembly confirmed that both argument moves disappeared:
`lea n-1,rax; mov rax,rdi` became `lea n-1,rdi`, and the second decrement
also computed directly into RDI. Nevertheless, fib took 13.20% more time in
the matched cohort. Spectral-norm took 4.14% less time, and the other rows
changed little. Fewer instructions alone did not establish an improvement;
the experiment does not isolate the microarchitectural cause of the fib
regression. The candidate's allocator, fixture and Unreleased line were
removed, restoring the prior allocation policy. The negative result is kept
to prevent the same unmeasured change being reintroduced.

Before the candidate push, the full local suite passed 284/284 in 217.5
seconds, bootstrap comparisons matched, and a regression fixture failed with
the old allocator and passed with the candidate, including an occupied
argument-register case. These correctness tests did not predict throughput.

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

## Spill propagation through unused blocks (timing pending)

The allocator keeps a spilled value in memory across a block that never reads
it, only when the preceding allocated segment reaches that block through its
sole incoming edge. Adjacency in file layout is insufficient. Joins retain
their existing allocation, including the cold-growth/header/reduction
regression in both block orders.

A retained machine-IR fixture carries a floating-point value through zero to
three blocks containing integer calls before using it again. With one such
block, the old allocator emits two spills and two reloads; the new allocator
emits one of each. The fixture passes the allocated-machine verifier. This
removes unnecessary memory transfers in that case; it establishes no
whole-program speedup. Alpha timing remains pending.

Focused local tests (`regalloc`, `zx_native`, `zx_lowered_parity`) pass, as do
the full local suite (284/284 in 257.6 seconds) and bootstrap comparisons for
the retained Linux and Darwin modules and SIMD headers. Source fingerprints
match before and after the final full run. Learning was off and desktop
display variables were removed. GitHub CI was not inspected.

## Independent x86-64 additions (timing pending)

The machine lowering pass no longer ties x86-64 integer additions to their
left input. The emitter uses LEA when both inputs remain live, or an ordinary
ADD when the result shares an input register. Immediate additions use a
signed 32-bit displacement, and narrow results retain their existing
normalization. Other arithmetic and target architectures retain their
constraints.

A retained allocation fixture reuses both inputs across three additions.
It fails the no-copy assertion with the old lowering; the new lowering
allocates it with zero moves and zero spills, and the allocated verifier
confirms the live inputs survive. Encoder cases cover separate and shared
result registers, a negative immediate, and signed-byte normalization.
This removes the artificial copy before reused address-index additions;
matmul's invariant-load and pointer-induction gaps still remain. Alpha
whole-program timing is pending, so no speedup is claimed here.

Focused local tests (`x86_encoder`, `regalloc`, `zx_native`,
`zx_lowered_parity`) pass. The final full local run passed 284/284 in
278.0 seconds, and Linux/Darwin and SIMD bootstrap comparisons matched.
Source fingerprints match before and after the gate. Learning was off and
desktop display variables were removed. GitHub CI was not inspected.
