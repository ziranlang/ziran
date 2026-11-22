# ZX / native C / embedded Wasm comparison

This is a benchmark harness, not an architecture migration. It compares the portable Ziran
engine, native ZX and control engines on synthetic CPU batches. It does not measure Kryon
rendering, input latency or frame throughput.

Measured receipts (files under `bench/results/`):

| Receipt | File |
| --- | --- |
| Interim comparison 2 (M3), Alpha physical host | `zx-compare-alpha-physical-native-2026-11-15.json` |
| Second-machine cohort, Omega | `zx-compare-omega-native-2026-11-15.json` |
| M0 baseline of the removed old VM and TinyCC tiers | `zx-compare-alpha-physical-2026-11-12.json` |
| M1 final pointer-dispatch cohort | `zx-compare-alpha-physical-pointer-dispatch-2026-11-13.json` |
| Moto APK baseline | `zx-compare-moto-apk.json` |
| Moto ABBA follow-up summary | `zx-compare-moto-arm-r0r1-summary.json` |
| Moto visual baseline / follow-up | `zx-compare-moto-visual.json`, `zx-compare-moto-arm-r0r1-visual.json` |

The tables and interpretation live in [Interim comparison
2](#interim-comparison-2-native-zx-on-linux-x86-64-2026-11-15) and the [Omega
cohort](#omega-cohort-at-pushed-master-2026-11-15). Older receipts are summarized in [Older
receipts](#older-receipts); their tiers no longer exist.

## Harness

The checked `zx_compare.zi` receives seed, iteration count and workload through three
runtime s64 -> s64 host calls. The scalar workload uses integer recurrence, branches and
calls. The structured workload updates 32 records in a fixed array, with runtime-selected
strings of different lengths and event-like selection/state updates. Every canonical compute
entry runs 4,096 iterations; the separate 128-iteration receipts are identified explicitly.
A separate Python oracle checks every timed batch; the executable also crosschecks the
handwritten C control.

The shared native adapter runs the real portable engine, raw BundleInstantiate TinyCC native
tier, handwritten optimized C control and the WAMR 2.4.5 AOT-only runtime. WAMR executes the
unmodified `zx2wasm` output after host-side target-specific AOT compilation. Its C API
supplies the original mutable budget global and host ABI; no imports or checks were
stripped. Interpreter and JIT are disabled. This is an embedded engine experiment, not a
production native Wasm provider.

## Reproduce

Comparison 2 runs from the repository root through the build queue with the `zx_compare.zi`
driver; the exact command is in [Interim comparison
2](#interim-comparison-2-native-zx-on-linux-x86-64-2026-11-15) and `README.md`. Run it in
the measured host's checkout at a pushed revision for the measured receipt. The older
receipts came from `zx_compare.py` and `zx_compare_apk/` with the removed TinyCC and old-VM
tiers and cannot be re-measured.

## Measurement boundaries and provenance

- Each target has 72 timed samples: two workloads x four variants x nine changing interleaved
  orders. Each entry performs 4,096 iterations; a timed batch has 32 entries on Alpha or 16 on
  Moto, with different seeds, and the result is consumed and checksum verified.
- Per instance: record a first call, run at least 500 ms of warmup with changing inputs, then
  record the timed batch. Per-entry time is batch elapsed time divided by batch size, not per
  iteration. Median and nearest-rank p95 use nine trials, so p95 is the observed maximum.
  Warmup checksums are not timed-batch oracle samples.
- Load, instantiate and first call are separate raw fields. Moto load/instantiate/first-call
  medians: VM 0.694/3.644/48.942 ms, TinyCC 0.748/0.511/0.323 ms, WAMR AOT 0.444/0.173/0.331
  ms. These are fresh-instance observations, not cold OS-cache guarantees. ZX load is file I/O
  plus checked-IR validation; WAMR load is file I/O plus AOT loading (target AOT compiled
  offline); native compilation or cache retrieval is inside instantiate. Do not rank complete
  secure startup across these unequal preparation contracts. Offline fixture translation/AOT
  and build wall times live in preparation metadata, outside runtime measurements.
- C uses -O3 without LTO; the Ziran archive is the actual local checkout; TinyCC is vendored
  0.9.28rc. The APK runtime archive is rebuilt with -O2 -fPIC and function/data sections; the
  prior archive was preserved in ignored backup before the first forced PIC rebuild, and later
  builds check normal upstream dependencies. WAMR is AOT-only: interpreter, JIT, fast-JIT,
  WASI, SIMD and reference types disabled; wamrc opt-level 3, size-level 3, x86-64 on Alpha,
  ARMV7/eabi cortex-a9 -neon on Moto. CPU-specific `.aot` packaging is not portable `.wasm`
  execution support in the product. No usable published Android runtime binary exists for this
  WAMR release, so unchanged release sources were built into the APK. Wasmtime's optimizing
  backend was not viable on ARMv7; no mock or browser result was substituted for embedded
  phone measurements.
- Receipts embed fixture/product/archive/tool digests, source revision plus dirty-source
  identities, commands/flags, compiler versions, cache markers, device identity, CPU and
  battery snapshots. Alpha runs record hostname `alpha` and `systemd-detect-virt=none`. The
  Moto receipt reports ARMv7 hardware; the USB-powered device stayed at 30.0 C across all 72
  battery observations (battery temperature is not CPU die temperature nor proof of no
  throttling), and the collector stops its own package at 40 C. No CPU affinity, governor
  override, OS cache flush, cooling reset or owner-app shutdown was used; shared host load can
  inflate outliers. Compiler receipts confirm changed benchmark sources were rebuilt and
  linked; installed asset hashes are checked against the built APK inputs.
- The final Moto run finished/recreated its own Activity after the first sample: PID
  unchanged, new UI disabled during the existing worker, exactly one complete compute suite.
  That lifecycle transition and periodic adb battery reads are part of the recorded
  environment.
- ABBA baseline and candidate APK hashes are distinct in
  `results/zx-compare-moto-arm-r0r1-summary.json`; invalid installation/cache attempts and
  their reasons are retained there and excluded from the valid pairs.

## Supported intersection and exclusions

The fixture covers integer arithmetic, loop/branch/call behavior, a fixed array of simple
records and runtime choice/length of fixed owned strings. Different string lengths prevent
replacing the text operation by a common constant; it is not a dynamic text
allocation/concatenation benchmark. The handwritten C control expresses the same work and
checksum and serves as an optimized native ceiling; it is not generated Ziran native code.

Excluded: pointers, borrowed resources, arbitrary callbacks/host signatures, slices/dynamic
vectors, general string manipulation, asynchronous I/O, threads, SIMD, GPU rendering, full
application assets and Kryon layout/rendering integration. The Wasm host surface here is the
supported scalar s64 ABI and mutable budget global. Unsupported product lowering remains an
explicit open Gap and cannot count as Wasm support; this benchmark makes no claim that such
cases acquired it.

Historical `zx-compare-moto.json` (shell), smoke and preliminary receipts remain separate;
earlier equal-length fixtures are not pooled. The smoke receipts preserve the real Thumb
loader helper-symbol failure (`zx-compare-moto-thumb-smoke.json`) and the native
scratch-directory failure (`zx-compare-moto-arm-smoke.json`). The unflushed receipt keeps 72
completed samples with stderr-truncated native markers and is explicitly incomplete. The
final APK fixed private scratch paths and checked unbuffered stderr, retaining all 18 actual
native instantiation markers.

## Moto results

Nine interleaved trials in one native APK process on the USB Moto e6 (Android 9/API 28,
armeabi-v7a), warm median / p95 in milliseconds:

| Workload | Old VM | TinyCC native | C control | WAMR AOT |
| --- | ---: | ---: | ---: | ---: |
| Scalar | 18.064841 / 24.544870 | 0.197038 / 0.358870 | 0.016481 / 0.019308 | 0.136250 / 0.139361 |
| Structured layout/state | 77.285481 / 90.429563 | 0.363019 / 0.363663 | 0.073909 / 0.074654 | 0.501591 / 0.504005 |

WAMR AOT was about 1.45x faster for scalar, TinyCC about 1.38x faster for structured work,
and optimized handwritten C was fastest. This comparison does not establish a universal
winning engine.

Native kernel numbers do not describe current live Kryon components:
`BundleComponentOpen`/`Replace` force the portable engine. Moto component trials verified
migrated integer state, rejection rollback, continued old state after rejection, and stale
handles. This is a tiny counter fixture, not full application hot reload, and there is no
Wasm component replacement implementation in this harness.

The final Moto comparison used balanced ABBA installation order old/new/new/old with two
valid APK pairs per variant. Every run completed, passed the external oracle, had one
lifecycle event, and cleared the native cache on variant switch. Mean-of-run-median
improvements were 2.58% scalar and 2.37% structured; two APK pairs are directional evidence,
not a statistically significant gain claim.

## Native visual interaction test

`visual_collect.py` passed on the installed Moto APK after the compute suite. The same
NativeActivity Bitmap/Canvas renderer draws an active ZX-native/WAMR-AOT selector and a
button with press/release feedback, a tap counter, checksum and result-derived state. An
actual selected guest executes 128 structured iterations per accepted tap. Switching guests
preserves the host's counter/state without a process restart; it does not migrate Wasm
component state.

Nine repeated taps per engine, in alternating orders, matched the independent Python oracle.
The regression also verified button drag-out cancellation, rejected outside-to-selector
drags, canceled selector-to-selector drags, unchanged state on selection, and same-PID
background/resume with state intact. Activity destruction/recreation during the compute
suite was checked separately. The process performs setup and starts its benchmark at most
once, and guest interaction stays disabled until that worker finishes, including across
Activity recreation.

Baseline repeated accepted-tap median / p95 in milliseconds:

| Engine | Event-to-buffer-post | Guest adapter + correctness check | Host draw/post |
| --- | ---: | ---: | ---: |
| ZX native | 43.131 / 72.379 | 11.747 / 23.863 | 8.282 / 12.293 |
| WAMR AOT | 31.528 / 34.239 | 0.0557 / 0.0685 | 8.474 / 9.611 |

The follow-up receipt `results/zx-compare-moto-arm-r0r1-visual.json` holds complete,
oracle-verified nine-tap medians: ZX native event-to-buffer-post 30.634 ms, guest adapter
plus correctness check 0.049385 ms, draw/post 8.026 ms; WAMR AOT event-to-buffer-post 32.465
ms, guest adapter plus correctness check 0.055538 ms, draw/post 8.273 ms.

First lazy guest events are retained separately. These interactive timings include Android
event delivery/queueing, host adapter, correctness check and buffer posting; they are not
the warmed kernel measurement above, and their short workloads and interactive scheduling do
not isolate guest execution. Both receipts use the engine's retained instance and
per-interaction `BundleInstanceRun`; invocation lifecycle did not change. On the follow-up's
measured retained-call path (validated stack-bounds cache, see [Older
receipts](#older-receipts)), the native handler median fell from 11.747462 to 0.049385 ms
(about 237.9x), while whole event-to-buffer-post only improved from 43.130563 to 30.633933
ms (1.408x, or 29.0%). Draw/post excludes guest computation. Event-to-buffer-post is neither
physical input-to-photon nor a full frame presentation metric, and this Android
Bitmap/Canvas fixture is not the full Kryon renderer: it supports no photon, FPS or
renderer-parity claim. Actual drag cancellation was injected; separate hardware
ACTION_CANCEL and multi-touch injection were not verified. Raw baseline and follow-up events
and limitations are in the two visual JSON files above. The baseline app was left open; the
follow-up collector later stopped its benchmark process and verified no collection producers
were running.

## Interim comparison 2: native ZX on Linux x86-64 (2026-11-15)

This is the M3 interim receipt from the [ASSEMBLY milestones](../ASSEMBLY/milestones.md),
not the final step-10 comparison. The receipt is
`results/zx-compare-alpha-physical-native-2026-11-15.json` (SHA-256
`c0a61f9f411a4d7dcb83a718c8a1f345cab87ecf762e33a83df3fd3068db771b`), written by the
`zx_compare.zi` driver (see `README.md`, "ZX runtime comparison 2 driver"):

```sh
build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY \
  build/bin/ziran run --target=c --root bench --module-path . \
  bench/zx_compare.zi -- --engine --native-cold --native-warm --aot \
  --c-control --m0-receipts bench/results --samples 9 \
  --out bench/results/zx-compare-alpha-physical-native-2026-11-15.json
```

The run used Alpha's own checkout fast-forwarded to pushed revision `31d291f3` (no clone or
copy): hostname `alpha`, machine ID `56391af3c57a496aba7e9f1f24cce732`, Intel Core i7-8700,
Linux 7.0.0-43-generic, governor `powersave`. Outer virtualization was measured as `none`
before entering the build queue and is recorded in the attestation block; `/proc/cpuinfo`
has no `hypervisor` flag. The Harmony scoped-git transport that set
`HARMONY_ALPHA_GIT_SCOPED` no longer exists, so the receipt records `scoped_ok: false` and
the driver's `--alpha` gate was not used; hostname, machine ID and virtualization checks all
pass. The host was shared with running VMs and another lane's queued test: load average 4.34
/ 3.69 / 3.39 before and 3.83 / 3.62 / 3.37 after (12 CPUs). The receipt hashes the driver,
helper and control sources (identical to `31d291f3`), `build/libziran.a` and every
fixture/helper/AOT artifact, and its integrity block re-verified all of them after the loop.

All 72 samples completed: 9 interleaved trials x 2 workloads x (engine, native helper, C
control, AOT ELF), 4,096 rounds and 16 batches per helper run. Every engine, native and C
checksum equals the oracle's sum over the 16 batch seeds, every native row reports backend
`NativeX86_64` with native evidence, and both AOT ELFs print the oracle value; an
independent Python recomputation of the oracle, the symmetric 1.5xIQR exclusions, medians
and nearest-rank p95 agrees with all ten summary cells. The old VM and TinyCC rows are
copied verbatim from the M0 receipt `results/zx-compare-alpha-physical-2026-11-12.json`
(SHA-256 `25a4b044…6349`, checked): all four summary entries and their 36 raw values are
byte-identical. They were not re-measured, because TinyCC and the old VM are removed.

Per-entry median / nearest-rank p95 in milliseconds after the symmetric 1.5xIQR rule:

| Workload | M0 old VM | M0 TinyCC native ZX | Portable engine | Native ZX cold | Native ZX warm | C control |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Scalar | 2.318291 / 2.328762 | 0.017111 / 0.017181 | 0.533915 / 0.577813 | 0.571038 / 0.597398 | 0.048285 / 0.049265 | 0.004596 / 0.004809 |
| Structured | 8.457550 / 8.620380 | 0.042724 / 0.043462 | 1.020441 / 1.076762 | 0.577248 / 0.648444 | 0.063309 / 0.065637 | 0.006251 / 0.006555 |

Native cold is one fresh process's load + instantiate + first entry, including native
compilation; warm is per entry after a 500 ms in-process warmup. The `zx2native` AOT ELF
control is whole-process wall time with startup at the base seed only: 4.925177 / 5.121921
ms scalar and 4.972000 / 5.112358 ms structured. It is dominated by process startup and is
not comparable per entry.

Warm native ZX is 11.06x faster than the portable engine on scalar work and 16.12x on
structured work, and 48.0x / 133.6x faster than the M0 old VM. It is not yet as fast as the
M0 TinyCC native tier: 2.82x slower on scalar and 1.48x slower on structured work, and about
10.5x / 10.1x slower than the handwritten C control. The C control (0.004596 / 0.006251 ms)
is close to the M0 receipt's C control on the same host (0.0048 / 0.0066 ms). This closes
M3's interim comparison item; it is not parity with TinyCC, and native code-quality work
continues in later milestones.

### Omega cohort at pushed master (2026-11-15)

The same driver and flags also ran on Omega, in a clean clone of pushed `699eb20c` with its
own build, writing `results/zx-compare-omega-native-2026-11-15.json` (SHA-256
`24305d24c47b50312dd7a8f50bc11a16ba2cad2e59fcf45c17d7e2694b4d8806`). Omega is an AMD Ryzen 9
9950X (machine ID `f4fdf763de024f88b69641dcaa99e52b`, virtualization `none`, governor
`powersave`), and it was busy: load average 17.67 / 12.27 / 11.93 before and 15.96 / 12.42 /
12.00 after on 32 threads, with I/O pressure above 70% while the run queued. All 72 samples
passed the same oracle, backend, IQR and M0 byte-identity checks as the Alpha receipt. Omega
is not the measurement host and its numbers are not comparable with the Alpha M0 TinyCC
rows; they show the cohorts' order on a second machine.

| Workload (Omega) | Portable engine | Native ZX cold | Native ZX warm | C control | AOT ELF (whole process) |
| --- | ---: | ---: | ---: | ---: | ---: |
| Scalar | 0.186089 / 0.187527 | 0.334988 / 0.479878 | 0.023957 / 0.026260 | 0.003780 / 0.003813 | 0.526038 / 0.555308 |
| Structured | 0.403903 / 0.453665 | 0.388488 / 0.455708 | 0.019796 / 0.020402 | 0.004354 / 0.004364 | 0.559788 / 0.657307 |

On Omega warm native ZX is 7.8x faster than the engine on scalar work and 20.4x on
structured work, and 6.3x / 4.5x slower than the C control.

## Older receipts

### Interim comparison 1: new portable engine (2026-11-12)

`zx-compare-alpha-physical-2026-11-12.json` holds this comparison and is the M0 baseline of
the removed old VM and TinyCC tiers: 90 oracle-verified samples (9 interleaved trials,
4,096 rounds, 16 batches) on Alpha's physical host (Intel Core i7-8700, GCC 15.2, pushed
revision `04fadbbc`, host load about 4 on 12 CPUs). The then-current portable engine was
2.5x slower than the old VM on scalar work and 5.5x on structured work. It predates the
optimized executor, is retained as its direct physical-host baseline, and cannot be
re-measured.

### Final pointer-dispatch cohort (2026-11-13)

`zx-compare-alpha-physical-pointer-dispatch-2026-11-13.json` is the accepted final M1
portable-engine cohort (SHA-256
`0ca4a2d2ffc0d234a375cfec31ba10217329daaf5b8194148ec9ee48d20d7d03`; source pin
`93a62fde00d245021d9b76ec871a8ff0a3458383c509562795ec9382d1119ad6`, revision
`01ad5ebbc6cb8969596e3a6607195c256580b155`). Against interim comparison 1 it improved the
portable engine 59.805936% on scalar (2.487930x) and 81.142894% on structured work
(5.303041x); against the old VM in the same receipt it was 2.398946% slower on scalar
(1.023989x) and 7.929537% slower on structured work (1.079295x). Its executor adds
flattened immutable instructions, cached frame bases and pointer-based immutable
instruction/edge dispatch to word-backed slots, plan facts, safe direct edge copies and
dead-source aggregate `Insert` aliasing; the rejected same-location edge-argument cache is
absent from the measured sources. It remains preliminary (`complete: false`,
`measurement_loop_finished: true`).

### Historical Moto/TinyCC tuning

This tuning kept typed ABI, capability, gas, stack, migration and rollback semantics while
adding a per-thread cache of validated stack bounds with Linux RLIMIT invalidation
retained, specialized TinyCC x86_64 and ARM full 64-bit gas decrement/negative-flag
emission with unchanged budget semantics, removal of ARM r0/r1 dead C-output spills with
every gas charge preserved, and a parallel header guard collision fix. The retained native
invocation moved its stack-bound lookup to that cache, avoiding repeated
`pthread_getattr_np` discovery while retaining per-call guards. This does not freeze new
LIR executor work.
