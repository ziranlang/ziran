# C teacher: Omega findings and validation

The final comparison covers Sieve, Records, Hash, Vectors, Calls, Printing,
Button, Text, Input and hello against GCC 14.2 and Clang 19.1.7 at `-O2`.
All ten workloads completed: 1,539 ranked function comparisons and zero failed
workloads. The compiler binaries remained unchanged throughout the run.
See [the final ranked report](c-teacher-omega-2026-11-16-final.md).

These measurements ran on physical Omega under the task's original explicit
host instruction. The parent's later instruction moves all future benchmarks
to physical Alpha, serialized through its build queue; builds and local tests
remain on Omega. No Alpha result is claimed here.

| Rank against GCC | Function | Native / C instructions | Sampled function CPU share | Main findings / owner |
| --- | --- | ---: | ---: | --- |
| 1 | Records | 407 / 70 | 73 / 73 | 4 stack reads and 260 writes versus zero; aggregate/regalloc and SSA lanes |
| 2 | Sieve | 90 / 34 | 120 / 120 | 22 branches versus six; surrounding loop layout and address folding |
| 3 | Hash | 62 / 26 | 149 / 149 | 18 branches versus two; loop layout and invariant candidates |
| 4 | Vectors | 149 / 101 | 133 / 153 | stack traffic, value induction, scalar reduction and loop structure |

The earlier [stable stage-two report](c-teacher-omega-2026-11-16-stage2.md)
counted 929 native Records instructions, 260 stack reads and 520 writes;
the integrated aggregate work reduced these static counts to 407, 4 and 260.
That work belongs to its owning lane. The teacher's report measures the
resulting assembly; it does not establish a causal runtime speedup.

Static back-edge envelopes contain branches and cold paths, so their size is
not an executed instruction count per iteration. In Sieve, the sampled native
envelope spans 40 static instructions, but its marking loop at text offsets
`0x1ce..0x1dc` executes four instructions: byte store, add induction value,
compare and branch. GCC's corresponding loop also executes four. The remaining
Sieve gap is in its surrounding loops, including a separate load/compare where
C uses a memory compare, joins, trampolines and repeated values.

Vectors has more direct gaps. The capacity-available native push path executes
28 instructions, with five stack reloads, five stack writes and one element
store; GCC executes 17 with no stack traffic and one element store. Native
recomputes the element value with multiply, while GCC carries it with add.
The native reduction executes eight instructions per element, including an
accumulator stack reload/write. GCC uses five per two elements with a 16-byte
load and packed addition, with an end pointer computed outside the loop.
These are manually followed paths in the exact disassemblies named by the
report, not counts inferred from its back-edge union.

The loop lane received these concrete paths and now owns LICM, induction and
strength reduction alongside SSA and rotation. The records lane owns vector
push and aggregate/allocation work; the vectorizer lane owns SIMD reduction.
Widget/host findings went to the widget
lane. Startup samples sometimes rank `SessionIndex` and theme creation highly;
these do not replace warm scenario measurements. Missing C symbols remain
unknown inlining/dead-code/name information. Stack operands include locals and
saved registers, vector-register uses include scalar floating point, loop
constants are candidates rather than proven invariants, and zero timer samples
remain unknown. Weighted instruction excess predicts neither cycles nor speedup.

This lane implemented shorter immediate-IMUL encoding, value-only LEA/shift
selection when flags are dead, and a bounded, separately tested integer leaf
inliner. An immediate in `[-128,127]` uses the one-byte immediate encoding,
saving three bytes per matching instruction. Positive scaled-add factors use
LEA; in-place powers of two use a shift after verification. No runtime speedup
is attributed to these changes by the measurements below.

`ziran learn verify` checks closed mined multiply proposals using 34 factors,
17 boundary inputs plus 2,048 deterministic random inputs, eight signed and
unsigned widths, exact C/native output agreement (70,210 lines), then the
retained ZX parity suite. Unknown semantics or any disagreement leave the
existing table unchanged. Repeated verification generated the same table.
See the [candidate evidence](c-teacher-omega-2026-11-16-candidates.json) and
[base-table verification certificate](c-teacher-omega-2026-11-16-rules-verification.json).

`ziran learn train` joins timing observations to current-format corpus records
by input, ZX, native ELF, map and compiler identities. It requires equal
checksums, learning off, manual safety, one host and one source epoch. It keeps
integer raw timings and symmetric inclusive-quartile 1.5×IQR exclusions,
compares complete workload cohorts and breaks ties by size then policy ID.
Identical ELF artifacts share one timing experiment so noise cannot select a
policy. Unsupported unroll/allocation strategies are rejected.

The real six-kernel calibration compared leaf thresholds zero and one against
C compiled at `-O2`, with eleven paired samples per observation. All six emitted
identical native artifacts and had zero eligible one-operation leaf calls.
The selected table therefore keeps leaf inlining off, block layout on, unroll
one and allocation zero. The [raw calibration](c-teacher-omega-2026-11-16-tuning.json)
and [deterministic selection receipt](c-teacher-omega-2026-11-16-tuning-selection.json)
retain all samples, exclusions and the final table digest. Training twice
produced byte-identical source. The rule certificate covers the base table;
the selection receipt binds the policy-only update to the final table.
Ordinary compilation reads only that checked-in, versioned Ziran table: no
corpus, clock or online training. Recording remains on by default outside
benchmarks; `ZIRAN_LEARN=off` disables it.

Kernel times below are microseconds per call, with eleven samples, learning off
and symmetric 1.5×IQR filtering. Five of six initial/stage-one native artifact
digests were identical, so those timing differences cannot be attributed to an
optimization. The final run includes other lanes' compiler changes and a busy
host (load average about 36); native samples also amortize one process startup
across 32 calls. These are observations, not isolated teacher speedups.

| Kernel | Initial native | Stage-one native | Final native | Initial C | Final C |
| --- | ---: | ---: | ---: | ---: | ---: |
| Sieve | 71.173 | 26.469 | 27.606 | 15.677 | 11.398 |
| Records | 46.891 | 20.442 | 19.234 | 12.972 | 9.290 |
| Hash | 31.488 | 35.585 | 33.365 | 25.037 | 22.550 |
| Vectors | 34.826 | 34.875 | 41.559 | 16.930 | 13.331 |
| Calls | 8.112 | 9.050 | 7.678 | 5.157 | 5.285 |
| Printing | 9.054 | 8.827 | 5.967 | 8.003 | 4.347 |

Raw values, exclusions and identities remain in the
[initial](c-teacher-omega-before.json),
[stage-one](c-teacher-omega-stage1-after.json) and
[final kernel receipts](c-teacher-omega-final.json).

The final warm native widget results below exercise public layout, interaction
and paint-command generation; every batch verifies its result. They exclude
pixel rasterization, system fonts and OS I/O. The earlier receipt from the
widget lane uses an older compiler and is contextual evidence, not a controlled
before/after experiment for this lane.

| Widget | Earlier native µs | Final native µs | Final C µs | Final Go µs |
| --- | ---: | ---: | ---: | ---: |
| Button | 612.345 | 3.696 | 1.966 | 3.165 |
| Text | 478.862 | 4.392 | 2.140 | 2.503 |
| Input | 791.135 | 5.543 | 2.765 | 4.885 |

See [the widget lane's earlier receipt](widget-omega-2026-11-16-after.json) and
[the final warm-widget receipt](c-teacher-omega-2026-11-16-widgets-final.json).
The gap to C and Go remains open.

Exact final commands, all through `build-queue` with learning and displays off:

```sh
make -j1 check
make -j1 check-bootstrap
build/bin/ziran run --target=c bench/bench.zi -- wasm-tier --samples 11 --label c-teacher-final --output bench/results/c-teacher-omega-final.json
build/bin/ziran run --target=c bench/bench.zi -- library-targets --project /mnt/storage/Projects/kryonlabs/kryon --manifest /mnt/storage/Projects/kryonlabs/kryon/tests/widget_benchmarks.json --samples 11
build/bin/ziran run --target=c bench/bench.zi -- c-teacher --output bench/results/c-teacher-omega-2026-11-16-final.md
```

All five commands returned zero: 281/281 local tests, bootstrap passing, six
kernels, three widgets and the ten-workload teacher report. The broader source
guard detected concurrent changes to `cmd/compiler_machine_aggregate.zi` and
`tests/machine_place_cases.zi`, so the combined
[validation receipt](c-teacher-omega-2026-11-16-final-validation.json) correctly
records `complete: false`. Compiler binaries were unchanged, but this run does
not validate that lane's newest constructor source; its next canonical
checkpoint owns that validation. The earlier stage-two guard covered 97 inputs
with no changes. No GitHub CI result or installation is claimed.

A subsequent canonical constructor checkpoint rebuilt successfully, passed
bootstrap and held all 3,409 guarded source/map inputs plus seven compiler
fingerprints unchanged. Its full suite passed 279/281: only `site_targets`
("Publish one measured host") and `site_pages` (a missing targets receipt)
failed. Those publication-data failures were resolved by the targets lane's
completed cohort and successful retained site tests. No compiler or constructor
test failed. A separate final site-test repeat records the integration verdict.

Final retained site repeat passed with exit zero:
`env -u DISPLAY -u WAYLAND_DISPLAY ZIRAN_LEARN=off HARMONY_BUILD_LOCAL=1 build-queue -- sh -c 'sh tests/site_pages.sh build/bin/ziran && sh tests/site_targets.sh build/bin/ziran'`.
The [integration receipt](c-teacher-omega-2026-11-16-integration-validation.json)
keeps the earlier full-suite and later canonical/rerun results distinct.
