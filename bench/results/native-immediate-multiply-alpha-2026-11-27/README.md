# x86 immediate multiplication on physical Alpha

The accepted change lets immediate integer multiplication place its result in
another register when the input remains live. Powers of two retain the existing
shift path, and multiplication by 31 retains its shift/subtract path. Only
`cmd/compiler_machine_lower.zi` and `cmd/compiler_x86_64.zi` change.

Measured on physical Alpha, Intel Core i7-8700, Linux x86-64. Both runtime
libraries execute the same frozen ZX bundles, with learning and native disk
caching disabled. These are warm runtime kernels, excluding compilation,
startup and loading.

| Kernel, 100,000 rounds | Before, ms | Accepted, ms | Runtime change |
| --- | ---: | ---: | ---: |
| Indexed record updates, initial run | 0.275844 | 0.270376 | -1.98% |
| Indexed record updates, matched-core confirmation | 0.278106 | 0.272171 | -2.13% |
| Integer arithmetic, matched-core confirmation | 0.118551 | 0.117401 | -0.97% |

The record improvement is small and repeats across two runs. The integer
numbers remain close enough to clock variation that they do not establish a
separate integer speedup. Across all 29 standard workloads, the geometric mean
runtime changes by +0.013%: effectively unchanged. Individual standard results
range from -4.07% (CRC32) to +2.70% (Dijkstra). This is a targeted code-generation
change; it does not establish general native/C parity or a broad runtime gain.

## Measurement and raw data

- [Record confirmation](records.json): nine samples per kernel and variant,
  128 entry calls per sample, 100,000 rounds per entry, at least 500 ms warmup.
  Each shuffled pair starts on the same selected quiet CPU core. Record
  exclusions: before none; accepted indices 3 and 8. Integer exclusions: none.
- [Initial record run](records-screen.json): the same sample counts and warmup,
  selecting a quiet core before each job. Raw observations remain separate.
- [29-workload run](standard.json): five shuffled runs per workload and variant,
  11 samples per run, at least 150 ms warmup. Each job selects a quiet core.
  Every checksum matches the independent C reference collected by the original
  standard A/B screen, retained in [the C checksum catalog](standard-reference-screen.json).

Every job is pinned and passes the shared clock/sibling-load gate. Rejected
host-gate attempts and their measurements remain in the JSON. Timings use
symmetric 1.5xIQR fences with inclusive interpolated quartiles; raw samples,
excluded indices and included medians are retained. Standard results apply the
same rule to inner samples and then to the five run medians.

[Collection metadata](collection.json) retains source/library fingerprints,
commands and the temporary benchmark collector bodies for exact reproduction.
The collectors use the existing `HostGate`, `summarize_samples` and fixture
oracle. Host binaries link `bench/standard/host.c` or `bench/zx_compare_host.c`
with `cc -O3 -std=c11`, the matching frozen library, `-lm -pthread -ldl`.
For reproduction, copy `standard-reference-screen.json` to the collectors'
`build/benchmarks/native-opt/standard-screen.json` input. That exploratory
screen has fixed CPU affinity without clock gates; its timings are excluded
from the accepted claims, and its `after` label identifies the first rejected
unrestricted-multiply candidate. Host identity and recomputed IQR exclusions
were added without changing its raw timings or checksums.
The libraries came from a shared checkout containing concurrent frontend work;
the report identifies source hashes and immutable artifacts rather than
representing those states as clean historical commits.

## Rejected experiment and validation

An additional allocator preference for dying arithmetic inputs was rejected
and removed. Its [paired 29-workload run](rejected-register-hints.json) regressed
fannkuch-redux by 7.17%, bytecode by 4.86% and image-filter by 4.00%, despite a
5.53% word-count gain. That allocator change is absent from the accepted source.

The [temporary arithmetic check](arithmetic-validation.json) passed 48 native
invocations across 88 typed multiply functions, covering signed and unsigned
8/16/32/64-bit values, positive/negative inputs, narrow overflow, zero, one,
powers of two, factors 3/5/7/9/17/31/32 and negative signed factors. The temporary
check source is removed after execution; no retained test was added.

The final current-tree rebuild's `native-core.o` is byte-identical to the
accepted measured runtime object (SHA-256 in collection metadata).

Publication validation: all 269 local retained checks passed in 334.0 seconds
(269 executed, zero reused). `make -j1 update-bootstrap CHECK_JOBS=2 JOBS=1`
updated all 89 seed roots and proved that stage 2 reproduces stage 1. The
independent `make -j1 check-bootstrap JOBS=1` matched all 89 roots and reproduced
the same fixed point. Exact command receipts are in the collection metadata.
