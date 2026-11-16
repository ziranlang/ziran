# Physical Alpha shared loop and SIMD comparison, 2026-11-16

Sieve, Records and Hash improved in this sequential cohort; Vectors regressed.
All four native results remain slower than their C references. This measures
the combined shared-loop, scalar allocator and SIMD checkpoint; it does not
isolate rotation or any individual pass. The original SSA rotation is already
present in the baseline.

Baseline: [constructor integration](https://github.com/ziranlang/ziran/commit/898f58a5fb03295e9a0979355f4ac222de326e5c).
After: [shared loop and SIMD checkpoint](https://github.com/ziranlang/ziran/commit/18fb5aeb6c4ab1ea31b39320c1a4eb7120eaf512).

| Kernel | Native before (µs) | Native after (µs) | Native change | C before (µs) | C after (µs) | After native/C |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Sieve | 58.410 | 56.302 | -3.6% | 33.647 | 33.379 | 1.69x |
| Records | 57.083 | 43.637 | -23.6% | 16.541 | 15.483 | 2.82x |
| Hash | 63.617 | 57.014 | -10.4% | 27.323 | 26.808 | 2.13x |
| Vectors | 63.030 | 89.161 | +41.5% | 33.353 | 33.321 | 2.68x |

Both cohorts ran once on physical `alpha`, Intel Core i7-8700; virtualization
was rejected (`systemd-detect-virt`: none). Learning was disabled. Each cohort
uses the same measured harness and reference files, 11 raw samples per target,
and symmetric inclusive 1.5×IQR filtering. Raw samples and excluded indices
are retained for every row. C, native, instruction engine and Wasm were measured
for the existing five-kernel cohort, including Printing. Go was not measured
by this invocation. All reported results and output verification matched.

Native batches have untimed host snapshots before and after each sample on
both sides. These record all logical CPU sysfs frequencies, allowed affinity,
load and monotonic time. They do not identify the actual clock of a migrating
child process, control frequency, or establish the cause of a timing change.
No samples are excluded because of these observations. The matched controls
reduce comparison differences; one sequential cohort is not an isolated causal
experiment. Vectors needs investigation in its optimization lane.

Compilation and measurement ran in Alpha's `build-queue` with display/session
variables removed, `ZIRAN_LEARN=off` and `HARMONY_BUILD_LOCAL=1`. Commands in each
cohort were:

```sh
/home/wao/.local/bin/make -j4 all
/usr/bin/python3 bench/wasm_tier.py --samples 11 --observe-host --label LABEL --output REPORT
```

The before build's source was verified against its declared commit before and
after compilation and measurement; the same applies to the after build.
Compiler and harness fingerprints remained unchanged through each measurement.
The two cohorts use identical harness fingerprints. A normal master fast-forward
used a private Git index. Local plan/test and report overlaps were preserved,
as were the default index and all files in `bench/standard`, `build/standard`
and `bench/results/standard`. Initial retries stopped before measurement;
fast-forward retries resumed only the after stage and never replayed the
completed baseline.

The raw reports, provenance and matching measured-harness patch are retained:

- [Before raw report](loop-passes-alpha-2026-11-16-before.json)
- [After raw report](loop-passes-alpha-2026-11-16-after.json)
- [Source, tool, harness and archive provenance](loop-passes-alpha-2026-11-16-provenance.json)
- [Measured harness delta from the baseline commit](loop-passes-alpha-2026-11-16-harness.patch)

The provenance records command arguments, source commits and trees, compiler
and harness SHA-256 values, raw report hashes, host observations and the guarded
standard artifacts. Native/C/Wasm/engine intermediates for each side are
archived on Alpha under ignored `backup/loop-alpha-ab/{before,after}-wasm-tier-artifacts.tar.gz`;
archive hashes are in the provenance. These are recovery evidence, not alternate
compiler installations.

The compiler milestone's final Omega local validation is recorded separately
in [the validation receipt](loop-passes-omega-2026-11-16-validation.json):
`make check` executed all 282 retained tests, 281 passed, all compiler tests
passed, and the sole failure was stale site pages. `make check-bootstrap`
passed. The compiler inputs and binaries stayed unchanged; unrelated site/test
edits failed the broader source guard, so this is a compiler-scoped result,
not a claim of a fully immutable all-green repository run.

Saved-report local verification passed with:

```sh
env -u DISPLAY -u WAYLAND_DISPLAY ZIRAN_LEARN=off HARMONY_BUILD_LOCAL=1 build-queue -- python3 backup/validate-loop-alpha-reports.py
```

All 40 target/kernel sample sets passed: raw counts, inclusive IQR exclusions,
medians, output flags, native observer coverage, report hashes and reconstruction
of the measured harness. [The verification receipt](loop-passes-alpha-2026-11-16-verification.json)
retains the verdict and four-kernel table. No new performance measurements
were taken during this verification. CPU snapshot frequencies differed across
cohorts and remain a limitation on causal interpretation.
