# ZX / native C / embedded Wasm comparison

This is a benchmark harness, not an architecture migration. The current direct
physical-Alpha optimized data is in
`results/zx-compare-alpha-physical-pointer-dispatch-2026-11-13.json`; the earlier
2026-11-13 optimized receipt is retained as an intermediate cohort and the
pre-optimization Alpha baseline is
`results/zx-compare-alpha-physical-2026-11-12.json`. Baseline Moto data is in
`results/zx-compare-moto-apk.json`. These datasets passed independent oracle and
real-engine evidence checks.

The checked `zx_compare.zi` receives seed, iteration count and workload through
three runtime s64 -> s64 host calls. Its scalar workload uses integer recurrence,
branches and calls. Its structured workload updates 32 records in a fixed array,
with runtime-selected strings of different lengths and event-like selection/state
updates. Every canonical compute entry runs 4,096 iterations; the separate
128-iteration receipt below is identified explicitly. These are synthetic CPU
batches, not Kryon rendering, input latency or frame throughput. The separate
Python oracle checks every timed batch, in addition to the executable's C
crosscheck.

The shared native adapter runs the real portable VM, raw BundleInstantiate TinyCC
native tier, handwritten optimized C control and WAMR 2.4.5 AOT-only runtime.
WAMR executes the unmodified zi2wasm output after host-side target-specific AOT
compilation. Its C API supplies the original mutable budget global and host ABI;
no imports or checks were stripped. Interpreter and JIT are disabled. This is an
embedded engine experiment, not a production native Wasm provider.

On the actual USB Moto e6 play (Android 9/API 28, armeabi-v7a), the same nine
interleaved trials in one native APK process gave warm median / p95 milliseconds:

- Scalar: VM 18.064841 / 24.544870; TinyCC 0.197038 / 0.358870; C 0.016481 /
  0.019308; WAMR AOT 0.136250 / 0.139361.
- Structured layout/state: VM 77.285481 / 90.429563; TinyCC 0.363019 / 0.363663;
  C 0.073909 / 0.074654; WAMR AOT 0.501591 / 0.504005.

On Moto, WAMR AOT was about 1.45x faster for scalar, while TinyCC was about
1.38x faster for structured work. Optimized handwritten C was fastest. This
comparison therefore does not establish a universal winning engine.

## Historical frozen Moto/TinyCC follow-up

Historical Moto/TinyCC tuning in this section is frozen; this does not freeze
new LIR executor work. The follow-up validated a per-thread cache of
validated stack bounds with Linux RLIMIT invalidation retained; specialized
TinyCC x86_64 and ARM full 64-bit gas decrement/negative-flag emission for
speed without changing the original correct budget semantics; removed ARM r0/r1
dead C-output spills while preserving every gas charge; fixed a parallel header
guard collision; and kept the existing retained native invocation while moving
its stack-bound lookup to that cache, avoiding repeated `pthread_getattr_np`
discovery while retaining per-call guards. Typed ABI, capability, gas, stack,
migration and rollback semantics were retained.

The final Moto comparison used balanced ABBA installation order
old/new/new/old with two valid APK pairs per variant. Every run completed,
passed the external oracle, had one lifecycle event, and cleared the native
cache on variant switch. Mean-of-run-median improvements were 2.58% scalar and
2.37% structured. Only two APK pairs were measured: this is directional evidence,
not a statistically significant gain claim.

Native kernel numbers do not describe current live Kryon components:
BundleComponentOpen/Replace force the portable VM. Moto component trials
verified migrated integer state, rejection rollback, continued old state after
rejection, and stale handles. This is a tiny counter fixture, not full
application hot reload. There is no Wasm component replacement implementation
in this harness.

## Reproduce

Run source builds directly on Alpha's physical host with Harmony's verified
physical-host transport. The transport checks hostname and virtualization,
copies the current tree without a clone or worktree, and preserves exact source
hashes:

```sh
cd /mnt/storage/Projects/harness/harmony
python3 scripts/alpha_git.py sync ziranlang/ziran
python3 scripts/alpha_git.py run ziranlang/ziran --synced -- \
  python3 bench/zx_compare_apk/build.py
python3 scripts/alpha_git.py run ziranlang/ziran --synced -- \
  python3 bench/zx_compare_apk/collect.py desktop \
  --results bench/results/zx-compare-alpha-physical.json
```

`alpha_git.py run` admits the whole command through the shared build queue; do
not nest another `build-queue` invocation inside it. Run these USB transport
commands on the machine physically attached to the authorized phone,
substituting its explicit serial rather than selecting an arbitrary device:

```sh
python3 bench/zx_compare_apk/collect.py android --serial SERIAL --lifecycle --results bench/results/zx-compare-moto-apk.json
python3 bench/zx_compare_apk/visual_collect.py --serial SERIAL --results bench/results/zx-compare-moto-visual.json
```

The compute collector installs/replaces only its own package, force-stops that
package to obtain a fresh process, and starts NativeActivity with `benchmark=true`.
Normal launcher starts show the interactive fixture immediately. Final APK path:
`build/benchmarks/zx-compare-android/zx-compare.apk`. Desktop executable path:
`build/benchmarks/zx-compare-embedded/zxcompare`.

Prerequisites: canonical Ziran native tools/archive, Python 3, CMake, a C compiler,
Android NDK r28c (28.2.13676358), JDK/keytool, aapt plus Android framework-res,
apksigner, and USB adb on the transport machine. `build.py` downloads hash-pinned
unmodified WAMR 2.4.5 source and its published Ubuntu x86_64 wamrc release into
`build/deps`; it does not clone a repository. WAMR license is embedded in the APK.
The test debug signing key is not part of maintained artifacts.

## Measurement boundaries and provenance

Each target has 72 timed samples: two workloads, four variants, nine changing
interleaved orders. Each entry performs 4,096 iterations; a timed batch has 32
entries on Alpha or 16 on Moto, with different seeds. The result is consumed and
checksum verified. Each instance first records a first call, then executes at
least 500 ms of warmup with changing inputs, then records its timed batch. Reported
per-entry time is batch elapsed time divided by batch size, not per iteration.
Median and nearest-rank p95 use nine trials; p95 is therefore the observed maximum.
Warmup checksums are not timed-batch oracle samples. The Python oracle recomputes
every timed batch independently; the executable also crosschecks handwritten C.

Load, instantiate and first call are separate raw fields. Across both Moto kinds,
load/instantiate/first-call medians were 0.694/3.644/48.942 ms for VM,
0.748/0.511/0.323 ms for TinyCC, and 0.444/0.173/0.331 ms for WAMR AOT.
These are fresh-instance observations, not cold OS-cache guarantees. ZX load
includes file I/O plus checked-IR validation; WAMR load includes file I/O and AOT
loading, with target AOT compilation already completed offline. Native compilation
or cache retrieval is inside instantiate; each native sample retains its actual
runtime timing/cache evidence. Do not rank complete secure startup from these
unequal preparation contracts. Offline fixture translation/AOT and build command
wall times are retained in preparation metadata, outside runtime measurements.

C uses -O3 without LTO. Ziran archive is the actual local checkout, TinyCC is
vendored 0.9.28rc. The APK runtime archive is rebuilt with -O2 -fPIC and function/
data sections; the prior archive was preserved in ignored backup before the first
forced PIC rebuild. Subsequent builds check normal upstream dependencies. WAMR is
AOT-only: interpreter, JIT, fast-JIT, WASI, SIMD and reference types disabled.
Wamrc uses opt-level=3, default size-level=3, x86-64 on Alpha; ARMV7/eabi,
cortex-a9, -neon on Moto. CPU-specific `.aot` packaging is not portable `.wasm`
execution support in the product. This release lacks a usable published Android
runtime binary, so unchanged release sources were built into the APK. Wasmtime's
optimizing backend was not a viable ARMv7 target; no mock or browser result was
substituted for embedded phone measurements.

The JSON receipts embed exact fixture/product/archive/tool digests, source
revision plus dirty-source identities, commands/flags, compiler versions, cache
markers, device identity, CPU and battery snapshots. Direct Alpha runs record
hostname `alpha` and `systemd-detect-virt=none`. Moto reports ARMv7 hardware in
the receipt and stayed at
30.0 C in all 72 battery observations; it was USB powered. Battery temperature is
not CPU die temperature or a proof of no throttling. The collector stops its own
package at 40 C. No CPU affinity, governor override, OS cache flush, cooling reset
or owner-app shutdown was used. Shared host load can inflate outliers. Compiler
receipts confirm changed benchmark sources were rebuilt and linked, and installed
asset hashes are checked against the built APK inputs.

The final Moto run also finished/recreated its own Activity after the first
sample. The PID remained unchanged, the new UI stayed disabled during the existing
worker, and exactly one complete compute suite finished. That one lifecycle
transition and periodic adb battery reads are part of the recorded environment.

The Moto ABBA baseline and candidate APK hashes are distinct in
`results/zx-compare-moto-arm-r0r1-summary.json`; invalid installation/cache
attempts and their reasons are retained there and excluded from the valid pairs.

## Validation status

Local `make check` completed 516 unique Ziran tests after minimal fixture
corrections and focused reruns, and `make check-bootstrap` passed. The final
Kryon component tests and benchmark passed. Full Kryon `make test` validation is
unresolved: it failed on an import/export mismatch, with that issue under
separate repair. At measurement/review time the performance feature set was
uncommitted and unpushed, and no GitHub checks had run. These benchmark receipts
do not substitute for that unresolved integration validation.

## Supported intersection and exclusions

The fixture covers integer arithmetic, loop/branch/call behavior, a fixed array
of simple records and runtime choice/length of fixed owned strings. Different
string lengths prevent replacing the text operation by a common constant. It is
not a dynamic text allocation/concatenation benchmark. The handwritten C control
expresses the same work and checksum, serving as an optimized native ceiling;
it is not generated Ziran native code.

Excluded: pointers, borrowed resources, arbitrary callbacks/host signatures,
slices/dynamic vectors, general string manipulation, asynchronous I/O, threads,
SIMD, GPU rendering, full application assets and Kryon layout/rendering integration.
The Wasm host surface here is the supported scalar s64 ABI and mutable budget
global. Unsupported product lowering remains an explicit open Gap and cannot
count as Wasm support; this benchmark makes no claim that such cases acquired it.

Historical `zx-compare-moto.json` (shell), smoke and preliminary receipts remain
separate. Earlier equal-length text fixtures are not pooled into the final
numbers. `zx-compare-moto-thumb-smoke.json` preserves the
real Thumb loader helper-symbol failure; `zx-compare-moto-arm-smoke.json` preserves
the subsequent native scratch-directory failure. The unflushed receipt preserves
72 completed samples whose final native markers were truncated by stderr buffering;
it is explicitly incomplete. The final APK fixed private scratch paths and checked,
unbuffered stderr, retaining all 18 actual native instantiation markers.

## Native visual interaction test

Baseline `visual_collect.py` passed on the installed Moto APK after the compute
suite.
The same NativeActivity Bitmap/Canvas renderer draws an active ZX-native/WAMR-AOT
selector and a button with press/release feedback, tap counter, checksum and
result-derived state. An actual selected guest executes 128 structured iterations
per accepted tap. Switching guests preserves the host's counter/state without a
process restart; it does not migrate Wasm component state.

Nine repeated taps per engine, in alternating orders, matched the independent
Python oracle. The regression also verified button drag-out cancellation, rejected
outside-to-selector drags, canceled selector-to-selector drags, unchanged state
on selection, and same-PID background/resume with state intact. Activity destruction/
recreation during the compute suite was checked separately. The process performs
setup and starts its benchmark at most once, and guest interaction stays disabled
until that worker finishes, including across Activity recreation.

Baseline repeated accepted-tap median / p95 observations in milliseconds were:

- ZX native: event-to-buffer-post 43.131 / 72.379; guest adapter plus correctness
  check 11.747 / 23.863; host draw/post 8.282 / 12.293.
- WAMR AOT: event-to-buffer-post 31.528 / 34.239; guest adapter plus correctness
  check 0.0557 / 0.0685; host draw/post 8.474 / 9.611.

The follow-up visual receipt is
`results/zx-compare-moto-arm-r0r1-visual.json`. Its complete, oracle-verified
nine-tap medians were: ZX native event-to-buffer-post 30.634 ms, guest adapter
plus correctness check 0.049385 ms, draw/post 8.026 ms; WAMR AOT
event-to-buffer-post 32.465 ms, guest adapter plus correctness check 0.055538
ms, draw/post 8.273 ms.

First lazy guest events are retained separately. These interactive timings include
Android event delivery/queueing, host adapter, correctness check and buffer posting;
they are not the warmed kernel measurement above. Their short workloads and
interactive scheduling do not isolate guest execution. Both receipts use the
engine's retained instance and per-interaction `BundleInstanceRun`; invocation
lifecycle did not change. The follow-up caches validated stack bounds, avoiding
repeated `pthread_getattr_np` discovery while retaining per-call guards. On that
measured retained-call path, the native handler median fell from 11.747462
to 0.049385 ms (about 237.9x), while whole event-to-buffer-post only improved
from 43.130563 to 30.633933 ms (1.408x, or 29.0%). Draw/post excludes guest
computation. Event-to-buffer-post is neither
physical input-to-photon nor a full frame presentation metric. This Android
Bitmap/Canvas fixture is not the full Kryon renderer and supports no photon, FPS
or renderer-parity claim. Actual drag cancellation was injected; separate
hardware ACTION_CANCEL and multi-touch injection were not verified. Raw baseline
and follow-up events and limitations are in the two visual JSON files above. The
baseline app was left open; the follow-up collector later stopped its benchmark
process and verified no collection producers were running.

## Interim comparison 1: new portable engine (2026-11-12)

This is the first interim receipt from the [ASSEMBLY milestones](../ASSEMBLY/milestones.md),
not the final ZX comparison. Building and measuring ran on Alpha's physical host
(hostname `alpha`, `systemd-detect-virt` = `none`, Intel Core i7-8700, GCC 15.2),
with virtualization rejected. The source tree was the pushed `04fadbbc` revision, synced
into Alpha's checkout. The host was shared with running VMs (load average about 4
on 12 CPUs). The receipt is `results/zx-compare-alpha-physical-2026-11-12.json`.
It holds 90 oracle-verified samples (9 interleaved trials, 4,096 rounds, 16
batches), TinyCC execution evidence on every native sample, raw values and
excluded indices.

Warm per-entry medians and nearest-rank p95 in milliseconds after the symmetric
1.5×IQR rule:

| Workload | Old VM | Old TinyCC native | New LIR portable engine | C control | Wasm (node) |
| --- | ---: | ---: | ---: | ---: | ---: |
| Scalar | 2.318 / 2.329 | 0.0171 / 0.0172 | 5.820 / 5.871 | 0.0048 / 0.0048 | 0.0131 / 0.0134 |
| Structured | 8.458 / 8.620 | 0.0427 / 0.0435 | 46.52 / 47.72 | 0.0066 / 0.0070 | 0.0386 / 0.0394 |

This receipt measured the new portable engine at 2.5× slower than the old VM on
the scalar workload and 5.5× slower on the structured one. It predates the
optimized executor below and is retained as its direct physical-host baseline.

### Intermediate optimized portable-engine follow-up (2026-11-13)

The follow-up receipt is
`results/zx-compare-alpha-physical-2026-11-13.json` (SHA-256
`149f88325cd6f85ba074c7a0ead1694e64c60fe51b8efcce08c60c3df2fa69e5`).
It uses the same 9 interleaved trials, 4,096 rounds and 16 batches on the same
physical Alpha. The receipt authenticates machine ID
`56391af3c57a496aba7e9f1f24cce732`, outer virtualization `none`, and the
managed execution namespace separately. All 90 engine/workload trials completed
with no unavailable engine cell.

Warm median / nearest-rank p95 in milliseconds:

| Workload | Old VM | Old TinyCC native | Optimized LIR portable engine | C control | Wasm (node) |
| --- | ---: | ---: | ---: | ---: | ---: |
| Scalar | 2.227 / 2.268 | 0.0166 / 0.0171 | 3.631 / 3.648 | 0.0046 / 0.0047 | 0.0183 / 0.0194 |
| Structured | 8.016 / 8.097 | 0.0412 / 0.0422 | 14.04 / 14.44 | 0.0064 / 0.0066 | 0.0622 / 0.0638 |

Against the 2026-11-12 lowered-engine medians, scalar improved from 5.820 to
3.631 ms (37.61%) and structured improved from 46.52 to 14.04 ms (69.83%). The
changes use word-backed scalar slots, cached immutable plan facts, proven-safe
direct edge copies, single- and same-root multi-predecessor block-parameter slot
coalescing, and dead-content-use analysis for in-place aggregate `Insert`.
Overlap and cycle cases retain parallel-copy scratch semantics.

This is a substantial portable-engine improvement, not parity or final ASM
completion. The optimized path is still 1.63× slower than the old VM on scalar
and 1.75× slower on structured work in this receipt. No new native engine has
been measured: register allocation, machine-IR encoding and native ZX remain
milestones M2/M3. As with the baseline, the receipt's `complete: false` labels
this as preliminary shell comparison data rather than the strict five-core final
campaign.

### Final pointer-dispatch cohort (2026-11-13)

The accepted final receipt is
`results/zx-compare-alpha-physical-pointer-dispatch-2026-11-13.json` (SHA-256
`0ca4a2d2ffc0d234a375cfec31ba10217329daaf5b8194148ec9ee48d20d7d03`).
It was measured on synchronized source pin
`93a62fde00d245021d9b76ec871a8ff0a3458383c509562795ec9382d1119ad6`
at source revision `01ad5ebbc6cb8969596e3a6607195c256580b155`, with 9
interleaved trials, 4,096 rounds and 16 batches. The receipt attests physical
host `alpha`, outer virtualization `none`, managed execution namespace
`container-other`, and machine ID `56391af3c57a496aba7e9f1f24cce732`.
All 90 samples and all ten summary cells were independently recomputed against
the oracle and the symmetric 1.5×IQR rule.

Warm median / nearest-rank p95 in milliseconds:

| Workload | Old VM | Old TinyCC native | Final LIR portable engine | C control | Wasm (node) |
| --- | ---: | ---: | ---: | ---: | ---: |
| Scalar | 2.284455 / 2.292693 | 0.016973 / 0.016994 | 2.339258 / 2.394966 | 0.004810 / 0.004816 | 0.012860 / 0.014146 |
| Structured | 8.128334 / 8.184008 | 0.042245 / 0.042847 | 8.772873 / 8.825778 | 0.006548 / 0.006552 | 0.037549 / 0.038148 |

Against the 2026-11-12 lowered medians, scalar improved by 59.805936%
(2.487930× faster) and structured improved by 81.142894% (5.303041× faster).
Relative to the old VM measured in this same receipt, the final LIR engine was
2.398946% slower on scalar (1.023989×) and 7.929537% slower on structured
(1.079295×).

This cohort adds flattened immutable instructions, cached frame bases and
pointer-based immutable instruction/edge dispatch to the prior word-backed
slots, plan facts, safe direct edge copies and dead-source aggregate `Insert`
aliasing. The rejected same-location edge-argument cache is absent from the
measured sources. This remains a preliminary portable-engine comparison with
`measurement_loop_finished: true` and `complete: false`, not the strict
five-core final campaign or evidence for a new native ZX engine. Register
allocation/splitting/spilling, post-allocation verification, complete machine-IR
encoding and native ZX default integration remain M2/M3 work.
