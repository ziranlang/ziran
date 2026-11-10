# ZX / native C / embedded Wasm comparison

This is a benchmark harness, not an architecture migration. Baseline Alpha data
is in `results/zx-compare-alpha-embedded.json`; baseline Moto data is in
`results/zx-compare-moto-apk.json`. Both baseline datasets passed the independent
oracle and real native-tier evidence checks. The frozen follow-up receipts named
below are also complete and independently oracle-verified.

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

On Alpha Personal KVM (Intel Core i7-8700 reported by the guest), the baseline's
nine interleaved
trials gave these per-entry warm median / nearest-rank p95 milliseconds:

- Scalar: VM 2.37148 / 6.49833; TinyCC 0.051277 / 0.066492; C 0.005109 /
  0.005942; WAMR AOT 0.009457 / 0.010062.
- Structured layout/state: VM 8.87491 / 14.06563; TinyCC 0.071007 / 0.096292;
  C 0.009690 / 0.010958; WAMR AOT 0.031296 / 0.034446.

On the actual USB Moto e6 play (Android 9/API 28, armeabi-v7a), the same nine
interleaved trials in one native APK process gave warm median / p95 milliseconds:

- Scalar: VM 18.064841 / 24.544870; TinyCC 0.197038 / 0.358870; C 0.016481 /
  0.019308; WAMR AOT 0.136250 / 0.139361.
- Structured layout/state: VM 77.285481 / 90.429563; TinyCC 0.363019 / 0.363663;
  C 0.073909 / 0.074654; WAMR AOT 0.501591 / 0.504005.

WAMR AOT was about 5.4x/2.3x faster than TinyCC for these Alpha scalar/structured
kernels. On Moto it was about 1.45x faster for scalar, while TinyCC was about
1.38x faster for structured work. Optimized handwritten C was fastest in both.
This comparison therefore does not establish a universal winning engine.

## Frozen follow-up results

Performance tuning is frozen. The follow-up validated a per-thread cache of
validated stack bounds with Linux RLIMIT invalidation retained; specialized
TinyCC x86_64 and ARM full 64-bit gas decrement/negative-flag emission for
speed without changing the original correct budget semantics; removed ARM r0/r1
dead C-output spills while preserving every gas charge; fixed a parallel header
guard collision; and kept the existing retained native invocation while moving
its stack-bound lookup to that cache, avoiding repeated `pthread_getattr_np`
discovery while retaining per-call guards. Typed ABI, capability, gas, stack,
migration and rollback semantics were retained.

The final 4,096-iteration Alpha receipt is
`results/zx-compare-alpha-embedded-stack-cache-final-minimal.json`. Its nine
warm ZX-native / WAMR-AOT medians were 0.017293 / 0.008966 ms for scalar and
0.036422 / 0.029478 ms for structured work. Relative to the baseline ZX medians,
ZX improved by 2.965x and 1.950x. Using the concurrent final WAMR medians, ZX
remained 1.929x slower for scalar and 1.236x slower for structured work; do not
divide final ZX by the earlier Wasm receipt.

`results/zx-compare-alpha-structured128-stack-cache-final-minimal.json` is a
separate complete 128-iteration structured receipt, not a normalized or pooled
4,096-iteration result. Its nine ZX-native / WAMR-AOT medians were 0.001854 /
0.001158 ms. It corroborates the optimized retained-call path at the visual
workload's batch size, shares the final receipt's checked archive and binary
hashes, and records the optimized runtime source hash.

The final Moto comparison used balanced ABBA installation order
old/new/new/old with two valid APK pairs per variant. Every run completed,
passed the external oracle, had one lifecycle event, and cleared the native
cache on variant switch. Mean-of-run-median improvements were 2.58% scalar and
2.37% structured. Only two APK pairs were measured: this is directional evidence,
not a statistically significant gain claim.

Native kernel numbers do not describe current live Kryon components:
BundleComponentOpen/Replace force the portable VM. Nine real component trials on
each target verified migrated integer state, rejection rollback, continued old
state after rejection, and stale handles. Alpha replacement/rollback medians
were 0.003601/0.003143 ms. This is a tiny counter fixture, not full application
hot reload. There is no Wasm component replacement implementation in this harness.

## Reproduce

Run source builds inside the managed queue on Alpha **Personal**, not the physical
Alpha host. The canonical checkout is shared with that guest. The verified route
is `builder@192.168.130.10` via `wao@alpha.local`. An admitted outer queue can wrap
SSH; propagate its allocation to the guest without bypassing admission.

The baseline build commands used an outer `build-queue -- ssh`, inside the guest:

```sh
cd /mnt/storage/Projects/ziranlang/ziran
python3 bench/zx_compare_apk/build.py
python3 bench/zx_compare_apk/collect.py desktop --results bench/results/zx-compare-alpha-embedded.json
python3 bench/zx_compare_apk/build.py --android
```

Only the source-build commands require the queue. Run these USB transport commands
on the machine physically attached to the authorized phone, substituting its
explicit serial rather than selecting an arbitrary device:

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
The standard guest-local debug signing key is not part of maintained artifacts.

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
markers, device identity, CPU and battery snapshots. Alpha reports an Intel Core
i7-8700 at 3.20 GHz under KVM; guest scheduling/frequency and physical thermal state
were not controlled. Moto reports ARMv7 hardware in the receipt and stayed at
30.0 C in all 72 battery observations; it was USB powered. Battery temperature is
not CPU die temperature or a proof of no throttling. The collector stops its own
package at 40 C. No CPU affinity, governor override, OS cache flush, cooling reset
or owner-app shutdown was used. Shared host load and VM scheduling can inflate
outliers. Shared-mount clock-skew warnings occurred; compiler receipts confirmed
changed benchmark sources were rebuilt and linked, and installed asset hashes
were checked against the built APK inputs.

The final Moto run also finished/recreated its own Activity after the first
sample. The PID remained unchanged, the new UI stayed disabled during the existing
worker, and exactly one complete compute suite finished. That one lifecycle
transition and periodic adb battery reads are part of the recorded environment.

The full Alpha receipts record baseline source revision
`d4ef3bb7dd15b5fe715cbdbdd34df8fa6592e3e4` and follow-up revision
`b51c06bb9cbbb9751eff56eec4303b242b2a2eca`. The 128-iteration minimal receipt
carries the follow-up archive, native-source and binary hashes. The Moto ABBA
baseline and candidate APK hashes are distinct in
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
global. Unsupported product lowering still falls back to the VM; this benchmark
makes no claim that such cases acquired Wasm support.

Historical `zx-compare-alpha.json` (Node), `zx-compare-moto.json` (shell), smoke and
preliminary receipts remain separate. Earlier equal-length text fixtures are not
pooled into the final numbers. `zx-compare-moto-thumb-smoke.json` preserves the
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
