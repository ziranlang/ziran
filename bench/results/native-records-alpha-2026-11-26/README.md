# Native record scalar replacement on Alpha

Measured on physical Alpha (Intel Core i7-8700, Linux x86-64), using the
unchanged `bench/zx_compare.zi` fixture and generated C control. These are
runtime kernels, not Hello World rendering, application startup or build times.
Module layout and the benchmark algorithm are unchanged.

The [baseline](../hello-world-alpha-2026-11-26/zx-runtime-final.json) uses
`0f3cb63c9c658ed8d6b9484fae9ff4b52a11db51`.
[Pointer snapshots](zx-runtime-pointer.json) use
`64140c1e6168b4e34f857b967890a9773fc224d0`; that change alone did not improve
the indexed-record workload. [Array snapshots](zx-runtime-array.json) use
`bb0323cef6c7b144eeea5ad1670ccf7a3fbde253`, which also scalarizes records
read from and written into aggregate members.

| Warm kernel, 100,000 rounds | Baseline native, ms | Pointer native, ms | Array native, ms | Array zi2c + cc -O2, ms | Array native/C |
| --- | ---: | ---: | ---: | ---: | ---: |
| Integer arithmetic | 0.119732 | 0.119255 | 0.121319 | 0.114298 | 1.061× |
| Indexed record updates | 0.548411 | 0.553496 | 0.273058 | 0.166265 | 1.642× |

The array change halves observed native record runtime (50.2% lower than the
baseline, 2.01× throughput). Integer performance is similar. Native execution
is still 64% slower than generated C on the record kernel; general native/C
parity is not established by these two kernels.

Cold native record load + instantiate + first entry falls from 1.059320 ms
to 0.781077 ms. This does not measure desktop application startup.

## Protocol and validation

Each version uses nine samples per kernel, 128 entry calls per sample,
100,000 rounds per entry, and at least 500 ms warmup. Runtime inputs vary by
trial and batch. Native warm times exclude loading, compilation and startup;
cold and warm observations come from the same fresh helper process. The C
control is emitted by zi2c and compiled with `cc -O2`. Cohorts are interleaved
in the same order on the same physical host.

All native samples explicitly select `NativeX86_64`. Every paired native/C
sample has matching checksums and batch counts. The three fixture source
SHA-256 values match across all reports and the current tree. Every reported
median and excluded index was independently recomputed using symmetric
1.5×IQR filtering with inclusive interpolated quartiles. Raw samples,
excluded indices, compiler/library fingerprints, host identity and shared
host load/governor observations remain in the JSON reports. The central
benchmark driver verifies physical Alpha; the optional environment-based
scoped-git attestation was not requested and remains false in the metadata.

The implementation keeps small record fields in scalar virtual registers,
including across block parameters, and loads their values at the original
read so later mutations cannot change a saved snapshot. Array writes store
the scalar fields directly, preserving a parent copy when separate storage
is required. Checked aggregate accesses retain their existing validation.

Local validation on the final compiler revision: all 269 retained checks,
the 88-root self-reproducing bootstrap, cross-target machine-IR record
coverage (x86-64, AArch64 and ArmV7), and native snapshot/checksum probes
with and without runtime checks. Installed compiler binaries match the
verified current-tree build.
