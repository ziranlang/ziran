# ZX performance comparison receipts

`bench/assembly_compare.zi` is the reusable, measurement-free report processor
for the eventual ZX before/after campaign. It reads raw receipts produced on one
physical host and atomically writes a Markdown report with absolute medians,
per-case ratios, regressions, missing cells, raw samples, and audited outlier
indices.

This is reporting infrastructure, not a benchmark result. The full ASSEMBLY
implementation is incomplete, the existing collectors do not yet emit this
schema, and the physical-host Alpha campaign has not run. Therefore there is no
measured ZX speedup to report yet.

## Invocation

```sh
build/bin/ziran run --target=c --root bench --module-path . \
    bench/assembly_compare.zi -- INPUT.json OUTPUT.md
```

The processor does not invoke a compiler, VM, workload, timer, or remote host.
Benchmark compilation and measurement remain an Alpha physical-host integration
task.

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
- raw samples in original order, excluded indices, and retained medians.

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
  host. Record fresh host/virtualization evidence; do not fall back to this
  desktop or the personal VM if Alpha is unavailable.
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

`bench/zx_compare.py` and `bench/zx_execution.zi` are older collectors and were
not changed. A future collector integration must emit the schema above and must
run the actual before/after workloads only on Alpha's physical host. After the
full ASSEMBLY engines are ready, that campaign can supply real receipts to this
processor. Until then, tests use synthetic statistical fixtures only, and this
document makes no performance claim.
