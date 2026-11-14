# ZX performance comparison receipts

`bench/assembly_compare.zi` is the reusable, measurement-free report processor
for the eventual ZX before/after campaign. It reads raw receipts produced on one
physical host and atomically writes a Markdown report with absolute medians,
per-case ratios, regressions, missing cells, raw samples, and audited outlier
indices.

This is the reporting and collection infrastructure for the strict five-core
before/after campaign, not that final benchmark result. The full ASSEMBLY
implementation is incomplete and the strict campaign has not run. A separately
labeled portable-engine comparison has run on physical Alpha; it does not
measure the missing new-native cell or establish a native ZX speedup.

## Invocation

```sh
build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY \
    build/bin/ziran run --target=c --root bench --module-path . \
    bench/assembly_compare.zi -- INPUT.json OUTPUT.md
```

The processor does not invoke a compiler, VM, workload, timer, or remote host.
`bench/assembly_collect.zi` is the separate collector. Its measurement-free plan
validation may run locally:

```sh
build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY \
    build/bin/ziran run --target=c --root bench --module-path . \
    bench/assembly_collect.zi -- --validate PLAN.json
```

Actual collection must run only in the canonical checkout on Alpha's physical
host:

```sh
build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY \
    build/bin/ziran run --target=c --root bench --module-path . \
    bench/assembly_collect.zi -- PLAN.json bench/results/CAMPAIGN/raw.json
```

The collector checks its plan completely before execution, then attests the
fresh hostname, exact `lscpu` model name, `uname -srmo` result, and
`systemd-detect-virt` result `none`. The plan must name physical host `alpha`
and an `alpha...` hostname. A failed attestation occurs before input hashing,
cache changes, or workload commands. This is a host guard, not permission to
run the command from another machine with copied or edited provenance.

### Collector plan, version 1

The plan root has exactly ten members: `schema_version` (exactly `1`),
`campaign`, `physical_host`, `hostname`, `cpu`, `os`, absolute
`working_directory`, absolute `collector_work`, `required_cells`, and `jobs`.
`collector_work` must be a child of the working directory. Required cells use
the same four-member schema documented below and may include modes that have no
job; those remain visible as missing cells in the generated report.

Each job has exactly these 24 members:

| Member | Contract |
| --- | --- |
| `receipt_id`, `case`, `engine`, `selector` | Unique receipt/cell identity and an exact supported engine/selector pair. |
| `phase`, `metric`, `unit` | One documented phase triple: duration in `ns`, throughput in `ops/s`, memory in `KiB`, or size in `bytes`. |
| `command` | Nonempty argv array. The absolute executable must be in `runtime_closure`; no shell parses it. |
| `verification` | Optional argv array run outside the measured interval. Compile and size jobs require it. |
| `runtime_closure` | Nonempty ordered list of `{path, sha256}` objects. It must contain every executable, runtime, header/module manifest, linked dependency, and helper needed to reproduce the selected baseline; every listed digest is checked before and after collection. |
| `workload`, `input_path`, `input_checksum` | Exact workload identity, regular input/artifact path, and `sha256:` digest. |
| `semantic_settings`, `correctness_expected` | Canonical common semantics including `process=fresh-child-per-sample`, and the exact `sha256:` of expected stdout. |
| `iterations`, `repeats`, `warmups`, `timeout_ms` | Positive work per sample, 3–4096 raw samples, phase-checked warmups, and a bounded child timeout. |
| `cache_policy`, `cache_directory` | `private-cold-each-sample`, `private-warm-across-samples`, or `immutable-artifact-no-engine-cache`. Private cache paths must be children of `collector_work`; only those paths are reset. |
| `environment` | Ordered explicit `NAME=value` settings or bare names to remove. Display variables are always removed and desktop recovery is disabled. |
| `artifact` | Measured build/size artifact under `collector_work`, or `-` when the phase does not produce one. |
| `rss_tool` | Absolute GNU/BusyBox time executable for memory jobs, declared in the closure, or `-` for every other phase. |

Every sample starts a fresh child process. Warm phases require at least one
discarded warmup and preserve their private cache across warmups and samples.
Cold private caches are reset before every measured sample. Compile and size
jobs remove only their declared artifact before each sample, then run their
separate correctness command. Duration uses monotonic nanoseconds; throughput
requires `timing=internal-kernel-ns`, reads a positive kernel-nanosecond value
from the measured command, and divides exact iterations by that internal
interval, so process startup is not mislabeled as sustained work. Throughput
also requires a separate untimed correctness command. Memory uses the child
launcher's peak-RSS KiB; size uses the freshly produced regular file. Correctness
stdout must match the expected digest for every sample. Any command, timeout,
checksum, correctness, or post-run closure failure aborts without replacing the
raw receipt file.

The receipt's `toolchain` preserves the selector, cache/process policy, complete
ordered runtime-closure identities, verification argv, and explicit environment;
`flags` preserves the measured command argv after its executable. The collector
emits only the strict version-1 four-member document accepted by the processor.

Current honest selectors are:

| Engine | Selector | Status |
| --- | --- | --- |
| `old-portable-vm` | `zx-vm` | Only a preserved pre-cutover `ziran` with `run --vm`; the current tree has no VM, and the M0 receipts hold its measurements. |
| `new-portable` | `lowered-portable` | Supported with explicit `zi2asm --run`, the engine that runs every `.zx`. |
| `direct-aot` | `direct-aot` | Supported only by executing the exact hashed ELF artifact. It is never relabeled as native ZX. |
| `control-c`, `control-go`, `control-wasm` | matching selector | Supported with an exact absolute command and complete hashed closure. |
| `old-c-tinycc-native` | — | Missing: the TinyCC route is removed, and its pre-cutover CLI default never gave native-only selection proof. |
| `new-native` | — | Missing: there is no native ZX engine until milestone M3. |

Requests for either missing execution mode fail plan validation. They may and
should remain in `required_cells`, where the processor reports them as missing
rather than manufacturing fallback receipts.

## Exact input schema, version 1

The root is an object with exactly these four members:

| Member | Type | Contract |
| --- | --- | --- |
| `schema_version` | integer | Exactly `1`. |
| `campaign` | string | Nonempty campaign identity, at most 256 bytes. |
| `required_cells` | array | 1–512 required cell objects. |
| `receipts` | array | 1–2048 raw receipt objects. |

Each required cell is an object with exactly four nonempty string members:

| Member | Meaning |
| --- | --- |
| `case` | Stable comparison-case identity. This is not the workload checksum. |
| `phase` | Measurement phase. |
| `metric` | Metric name, which determines ratio direction when recognized. |
| `unit` | Exact unit spelling expected from every engine in this cell. |

Every required cell requires one receipt from each core engine:

- `old-portable-vm`
- `new-portable`
- `old-c-tinycc-native`
- `new-native`
- `direct-aot`

Missing core receipts do not make the input malformed. They produce an
incomplete status, a visible `missing` matrix entry, and an explicit item in the
missing-cells section. `control-c`, `control-go`, and `control-wasm` are optional
and are compared when present.

Each receipt is an object with exactly these 21 members:

| Member | Type | Contract |
| --- | --- | --- |
| `receipt_id` | string | Unique receipt identity, at most 256 bytes. |
| `case` | string | Stable comparison-case identity. |
| `physical_host` | string | Explicit physical-machine identity. |
| `host_mode` | string | Must be `physical`. |
| `vm` | string | Must be `none`. |
| `cpu` | string | Exact CPU identity captured by the collector. |
| `os` | string | Exact OS/kernel identity captured by the collector. |
| `toolchain` | string | Exact compiler/runtime toolchain identity for this engine. |
| `flags` | string array | Ordered explicit flags, at most 64 entries and 256 bytes each. |
| `workload` | string | Exact workload identity. |
| `input_checksum` | string | Checksum identity of the workload input/artifact. |
| `engine` | string | Core engine, optional control, or an explicitly named future engine. |
| `phase` | string | Measurement phase. |
| `metric` | string | Metric name. |
| `unit` | string | Raw sample and median unit. |
| `semantic_settings` | string | Overflow, precision, feature, seed, and other semantic settings in canonical collector form. |
| `correctness_expected` | string | Expected output/oracle identity. |
| `correctness_actual` | string | Observed output identity; it must equal the expected identity. |
| `iterations` | positive integer | Exact work performed by every sample. |
| `repeats` | integer | 3–4096 and exactly equal to the raw sample count. |
| `samples` | number array | Positive finite JSON numbers in original measurement order. |

All strings are nonempty, contain no control bytes, and are at most 1024 bytes
unless a smaller bound is stated above. The complete input must be a regular
file from 2 bytes through 4 MiB. Across the document, no more than 65,536 raw
samples are accepted.

Duplicate required cells, duplicate `receipt_id` values, or duplicate
engine/case/phase/metric cells are rejected. Unknown object members are rejected
because every object has an exact member count and every required member is
type-checked.

## Comparability gates

The entire accepted campaign must have one identical `physical_host`,
`host_mode`, `vm`, `cpu`, and `os`. Every receipt must directly identify a
physical host and no VM. Receipts for the same case/phase/metric must also have
identical:

- workload and input checksum;
- iteration count;
- unit;
- semantic settings;
- expected correctness identity.

Each receipt's actual correctness identity must equal its expected identity.
Any changed host, VM, CPU, OS, workload, checksum, iteration count, unit,
semantics, or correctness identity rejects the document before output
replacement. Toolchains and flags remain visible per receipt but may differ
between engines by design.

The processor never pools duplicate or incompatible batches. A collector that
wants another batch must create a separately identified campaign input instead
of relying on implicit aggregation.

## Statistics and ratio direction

Each receipt is analyzed by the canonical
`bench/benchmark_statistics.zi::BenchmarkAnalyze` implementation:

- symmetric Tukey fences at 1.5×IQR;
- linearly interpolated inclusive first and third quartiles;
- both tails checked;
- a sample is excluded only when it is strictly outside a fence;
- at least three retained positive finite samples;
- the median is computed from retained samples;
- original raw order and zero-based excluded indices are reported.

Recognized phase/metric pairs and intended normalized favorable factors are:

| Phase | Metric | Better direction | Favorable factor |
| --- | --- | --- | --- |
| `coldcompile` | `duration` | lower | baseline / candidate |
| `coldstartup` | `duration` | lower | baseline / candidate |
| `warmstartup` | `duration` | lower | baseline / candidate |
| `warmthroughput` | `throughput` | higher | candidate / baseline |
| `memory` | `memory` | lower | baseline / candidate |
| `code-size` | `code-size` | lower | baseline / candidate |
| `bundle-size` | `bundle-size` | lower | baseline / candidate |

A favorable factor above 1 is an improvement, below 1 is a regression, and
exactly 1 is unchanged. Every comparable row separately reports candidate
metric change as `(candidate - baseline) / baseline × 100%`. For example, this
keeps a lower-is-better duration increase distinct from the reciprocal favorable
factor; it never presents factor change as elapsed-time or size change.

If division of positive finite medians underflows or overflows, the row is marked
`noncomparable: derived ratio outside finite range`; no infinity or NaN claim is
written. An unknown metric is retained with its absolute medians and raw audit
but its pair is explicitly marked `noncomparable: unknown metric direction`.
No overall or geometric mean is calculated, so missing cells cannot disappear
inside an aggregate.

The fixed primary comparisons for every available cell are:

1. `old-portable-vm` → `new-portable`
2. `old-c-tinycc-native` → `new-native`
3. `old-c-tinycc-native` → `direct-aot`

Optional C, Go, and Wasm controls are each compared against
`old-portable-vm` when their receipts exist.

## Output and failure behavior

The Markdown output contains:

- campaign completeness and the same-host environment;
- an absolute-median matrix for core and control engines;
- the fixed per-case comparisons with ratio and improvement/regression;
- every missing required engine/phase cell;
- per-receipt workload, checksum, iterations, semantics, toolchain, flags, and
  correctness identities;
- explicit sample counts, raw samples in original order, excluded indices, and
  retained medians.

All receipt-controlled Markdown and HTML metacharacters are encoded before
rendering, including pipes, backticks, backslashes, angle brackets, ampersands,
asterisks, underscores, brackets, and hashes.

The output follows input order and is byte-deterministic for identical input.
It is built completely in memory, then written through `ReplaceFile`, which
writes and syncs a private sibling before renaming it over the destination.
Malformed, statistically invalid, or incompatible receipts return failure
without replacing an existing report. An atomic write failure also returns
failure and removes its private sibling.

## Full comparison delivery gate

The owner's requested final comparison is the complete before/after campaign,
not a scalar microbenchmark or a selected best-case ratio. It remains pending
until the implementation and its local correctness gates are complete.

- Run every actual compilation and measurement directly on Alpha's physical
  host. Record fresh host/virtualization evidence; if Alpha is unavailable,
  leave the campaign pending instead of substituting another machine.
- Validate the baseline executable's complete runtime closure, standard
  modules, headers and dependency/toolchain identities before calling it an
  old-version baseline. The saved baseline binaries alone are not evidence
  that this closure has been preserved. Keep exact workload/input bytes and
  expected output checksums identical across comparable engines.
- Explicitly force each engine and reject silent interpreter/compiler/Wasm
  fallbacks. A missing new-native ZX path remains `missing`, not an AOT result
  relabeled as native ZX. Record optimization, CPU features, runtime budgets
  and cache policy per engine; do not imply unmetered AOT has identical safety
  costs to a metered interpreter.
- Cover scalar arithmetic, branches/loops, direct and indirect calls, packed
  records/arrays, checked dynamic indexing, global/field pointers and aliasing,
  allocation/collection/borrowed-view churn, text processing, host bridges and
  applicable concurrency/runtime workloads. Include the VM root-tracing fix
  and current linear token dispatch in relevant workload results. Unsupported
  engine/workload pairs must be explicit limitations, never zero-time samples.
- Measure cold compilation, cold startup, warm cached startup, warm sustained
  throughput, memory, native code size and bundle size separately. Define the
  cache/process boundary in each phase; never delete unrelated application or
  user caches to manufacture a cold run. Exclude compilation from steady-state
  throughput, but report it in the compilation phase instead of hiding it.
- Report every requested cell with absolute units, before/after medians,
  favorable factor, actual metric percent change, sample count, raw samples
  and symmetric outlier exclusions. Include regressions and missing cells as
  prominently as speedups. Keep C/Go/Wasm controls alongside ZX results.
- Correlate measured hot paths with the exact ELF digest, emitted assembly,
  byte ranges and source origins. Distinguish observed elapsed time or hardware
  counters from estimated instruction costs. Missing counters remain unknown.
- Retain the real collector inputs, raw receipts and the generated Markdown
  comparison under `bench/results/`. Synthetic reporter tests prove report
  behavior only and must never be presented as measured language performance.

## Current limitation and next integration step

`bench/assembly_collect.zi` supplies the strict collector prerequisite for
genuinely selectable modes, but cannot complete the five-core matrix: both
native cells described above remain unavailable. The TinyCC cell can no longer
be measured, since its route is removed; the M0 receipts are its only data.
Once a real new-native ZX integration exists, the same plan format can add
that job.

The older `bench/zx_compare.py` harness has now produced a labeled interim
portable-engine comparison on physical Alpha; see
[`bench/zx_compare.md`](../bench/zx_compare.md#final-pointer-dispatch-cohort-2026-11-13)
and the retained raw receipt. It shows 59.81% scalar and 81.14% structured
improvements over its 2026-11-12 lowered-engine baseline, while remaining
1.024×/1.079× slower than the old VM. That shell receipt is not the strict
five-core campaign, does not supply either missing native cell, and cannot close
this delivery gate. Synthetic fixtures continue to validate this report
processor independently of those measured interim results.
