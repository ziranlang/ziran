# Vectors regression: invariant hoisting and register pressure

The regression came from hoisting cheap constants and symbolic values into
loop preheaders. Their longer live ranges increased spill traffic in vector
growth loops. SSE2 was helpful: disabling it alone made the regressed program
slower. The fix retains useful invariant expressions and their dependencies,
while leaving independently cheap values near their uses.

## Matched Alpha comparison

Physical Alpha, CPU 0, 11 shuffled interleaved rounds, 32 checksum-validated
kernel calls per process. Wall times include process startup divided by 32;
child CPU time is recorded separately. Every series uses symmetric 1.5×IQR
filtering with inclusive interpolated quartiles. Raw samples, excluded indices,
binary identities and compiler object identities are retained in
[the final JSON](vector-profitability-alpha-2026-11-16-interleaved.json).

Both `zi2zx` and `zx2native` were relinked from Alpha's existing individual
compiler objects. The experimental compiler replaces only `compiler_opt_licm.o`
with C generated from the changed Ziran module. Both arms compile fresh source
bundles. The baseline reproduces the archived regressed native executable
byte for byte for all four kernels. Standard benchmark artifacts and the
canonical compiler objects/tools remained unchanged.

| Kernel | Fresh baseline, µs | Fixed, µs | Change |
| --- | ---: | ---: | ---: |
| Vectors | 85.823 | 42.497 | −50.5% |
| Sieve | 55.293 | 54.733 | −1.0% |
| Records | 38.641 | 38.220 | −1.1% |
| Hash | 35.239 | 34.603 | −1.8% |

Vectors child CPU time fell from 81.984 to 38.625 µs. The older executable
from before the loop/SIMD checkpoint measured 56.438 µs in this same cohort.
The fix therefore also improves on the earlier executable, by 24.7%.
Native image size fell from 2,944 to 1,728 bytes.

Fresh C reference programs compiled on Alpha with `cc -O2` and the same
32-call checksum wrapper measured 49.630 µs (Vectors), 48.690 (Sieve),
27.726 (Records) and 42.337 (Hash). These are process-based measurements;
they should not be combined with the older site's internal-timer C numbers.
All four checksum wrappers succeeded in every arm. This cohort isolates LICM;
it does not include the separate Records bounds-comparison change.

## Cause isolation

The initial pass-isolation tools preserve the original compiler objects and
use small diagnostic wrappers to bypass individual passes. Fresh source is
compiled for optimizer variants. Re-encoding the archived ZX reproduces the
regressed ELF byte for byte, establishing a backend control.

| Vectors variant | Wall µs | Child CPU µs |
| --- | ---: | ---: |
| Archived before checkpoint | 55.194 | 51.219 |
| Archived after checkpoint | 83.814 | 80.156 |
| Re-encoded after, identical ELF | 83.938 | 80.281 |
| After, SSE2 prefix disabled | 98.353 | 94.719 |
| Fresh source, LICM disabled | 41.711 | 38.062 |
| Fresh source, induction rewrite disabled | 78.390 | 74.562 |
| Fresh source, layout disabled | 89.404 | 85.625 |
| Fresh source, LICM/induction/layout disabled | 63.721 | 60.062 |

See [the isolation samples](vector-regression-alpha-2026-11-16-interleaved.json).
The earlier non-interleaved diagnostic report is retained as raw investigation
data; host drift even affected identical binaries, so it is not used for
causal timing comparisons.

An attempted post-hoisting fold/CSE trial re-encoded already optimized ZX.
Serialized `zx2native` input bypasses `LirOptBuild`, so that trial did not
exercise the optimizer change. Its
[negative-control samples](vector-cleanup-alpha-2026-11-16-interleaved.json)
are retained explicitly as a control, not evidence against that alternative.
The final fresh-source experiment corrects this by rebuilding both stages.

## Regression coverage and local validation

The retained LICM fixture checks that standalone constants stay in the loop,
constants required by useful hoisted chains move in dependency order, and
zero-trip execution, traps, source origins, fuel and work limits remain valid.
The existing vectorizer fixture now includes a VecPush growth/reduction loop
and checks its native allocation: spill/reload events fell from 53 to 6, with
an upper bound of 12 to catch recurrence. It also compares C and native output
for lengths 0–67 and the existing deterministic data, overlap, integer wrap,
ordered floating reduction and checked-mode cases.

Focused source checks, `tests/compiler_opt.sh`,
`tests/compiler_vectorize.sh` and `tests/zx_lowered_parity.sh` passed locally
through `build-queue` on Omega. The initial full run passed 281 of 283 local
tests, with only stale generated guide pages and a missing current-version
changelog entry failing. Those metadata issues were resolved. The final shared
gate, `build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY make -s -k check
check-bootstrap CHECK_JOBS=4`, passed 284/284 tests (284 executed, none reused)
and bootstrap matched. The four source/test files in this fix were unchanged
between the focused validation and that full gate. The full gate also included
the separate loop-rotation compile-time fix in the shared checkout.
No CI or deployment result is implied by these local tests.
