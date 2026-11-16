# Native widget investigation on Omega

Latest checkpoint: see `widget-native-alpha-2026-11-16.md` for the pushed
field-copy fix, fresh same-host physical Alpha before/after results and completed
integrated local validation. The sections below retain the earlier investigation
stages and their status at the time.

Local diagnostic host: Omega, AMD Ryzen 9 9950X; physical host (virtualization `none`).
The owner explicitly selected Omega for this task.

## Warm baseline

Command (through the build queue, learning off and default manual safety):

```sh
env -u DISPLAY -u WAYLAND_DISPLAY ZIRAN_LEARN=off ZIRAN_BENCH_HOST=omega build-queue -- build/bin/ziran run --target=c bench/bench.zi -- library-targets --project /mnt/storage/Projects/kryonlabs/kryon --manifest /mnt/storage/Projects/kryonlabs/kryon/tests/widget_benchmarks.json --samples 11
```

The complete pre-change receipt is `widget-omega-2026-11-16-before.json`.
C / Go / native ZX, nanoseconds per two-frame scenario:

| Scenario | C | Go | Native ZX |
| --- | ---: | ---: | ---: |
| Button | 2030.5 | 3411.7 | 626462.9 |
| Text | 2206.0 | 2467.6 | 422693.2 |
| Input | 2869.2 | 4792.4 | 764081.6 |

These compare targets on the same host; the older published Alpha table is not a same-host before/after comparison.

`bench/site_target_host.c` loads and instantiates once, verifies one call, warms for 150 ms, calibrates a batch and times repeated `BundleInstanceRun` calls. Load, compilation, initialization and process startup are outside the timer. Sessions persist across calls. There is no cold-work timing bug in this widget column.

The Button bundle has zero host capabilities: the fixture providers are bound ordinary Ziran functions, so `NativeHostInvoke` is absent from this measured path.

## Warm SIGPROF evidence

A bounded SIGPROF sampler reads RIP from `ucontext_t` with `setitimer(ITIMER_PROF)` at a 1 ms CPU interval. Sampling starts after loading, initialization and the correctness call, and stops after a 3 second warm loop. The native diagnostic links the current loader with an added dump of its actual `MachineImage.function_offsets`, code base and patched text. Native disk cache is disabled for this diagnostic. The normal 150 ms harness remains unchanged.

The completed native run had 749 samples. Its exact map reports both LIR checks and machine checks **0**, and the runner reports **zero host capabilities**. The load took 48.70 ms outside sampling and timing. Native functions with the most samples:

| Function | Samples | Share |
| --- | ---: | ---: |
| TreeSubmit | 123 | 16.42% |
| TreeFindOld | 59 | 7.88% |
| TreeFinish | 56 | 7.48% |
| TreeIsBuilding | 34 | 4.54% |
| TreeLayerAt | 33 | 4.41% |

`TreeIsBuilding` allocates a `0x205d0`-byte frame and copies an entire **132,523-byte** session record onto it, before loading one `building` byte. Of its 34 samples, 18 hit text offset `0x7c8c` and 16 hit `0x7c86` in that copy loop. The loop copies 132,512 bytes in 16-byte chunks, then copies the remaining 11 bytes. Its retained disassembly is `widget-button-omega-2026-11-16-tree-building.asm`.

Generated C expresses the same function as:

```c
int32_t value_0 = SessionIndex(session);
return (trees[value_0]).building;
```

The C compiler reads the needed field directly. This is a general aggregate load/extract optimization gap in Ziran, not host veneers, cold timing, enabled checks, or a Kryon fixture defect. Many other hot tree functions also operate on this record. The aggregate/backend owner received the raw map, hot addresses and this diagnosis; the requested fix must preserve snapshot semantics across intervening writes.

The C runner sampled the same 3 second warm loop and produced 746 samples; its largest function was DefaultTheme (134), while TreeSubmit had 23. This is a CPU-share comparison, not a per-function elapsed-time measurement. Raw native/C samples, exact JIT map, summary JSON and diagnostic artifact hashes are retained beside this report. The earlier all-process sampler had overwritten raw output and is superseded by this warm-only run.

## Native entry fix

`native_stack_limit()` called `pthread_getattr_np` on every outermost warm invocation. Linux may scan `/proc` for the main thread. A per-thread cache now validates the current frame against the cached range and checks the current `RLIMIT_STACK`; a different stack or changed limit re-queries the bounds. The per-entry depth cap, reserve and uncached fallback remain.

An independent constant-return entry measured seven batches of 100,000 calls: median **8,883.12 ns before / 77.99 ns after**, no IQR exclusions. This removes generic entry overhead but does not eliminate the dominant aggregate copies.

Local test: `env -u DISPLAY -u WAYLAND_DISPLAY build-queue -- make check CHECK_FILTER=native` passed **17/17**, including changed stack limits after warmup, separate threads, an alternate stack and cross-instance host reentry. A separate existing same-instance reentry mismatch reproduced with the original stack query was reported outside this change.

## Current after measurement

After rebuilding the shared native stack, the same 11-sample command completed successfully. Full receipt: `widget-omega-2026-11-16-after.json`. Values are nanoseconds per warm two-frame scenario; checks and learning are off.

| Scenario | C | Go | Native ZX before | Native ZX current |
| --- | ---: | ---: | ---: | ---: |
| Button | 2029.2 | 3318.7 | 626462.9 | 612345.4 |
| Text | 2169.1 | 2698.3 | 422693.2 | 478862.3 |
| Input | 2882.4 | 4712.1 | 764081.6 | 791135.2 |

Shared compiler changes and system load differ between these receipts. No broad widget speedup is claimed from this pair. The aggregate fix and final measurements remain pending.

## Harness tier correction

The previous kernel driver called its ZX column the instruction engine but used `BundleInstantiate` (Auto). On Linux Auto selects native. New kernel receipts must use explicit Engine policy; new widget receipts use explicit Native. Historical receipt numbers cannot be relabeled as engine measurements. The new site generator labels that historical column accurately until replacement receipts are supplied.

## Local verification and remaining work

- `env -u DISPLAY -u WAYLAND_DISPLAY ZIRAN_LEARN=off build-queue -- sh tests/site_targets.sh build/bin/ziran`: **exit 0**. Source/output hashes, receipt statistics, generated widget/kernel HTML, checked IR and executable published artifacts verified.
- `python3 scripts/site_kernel_targets.py --check` through the build queue: **exit 0**.
- `env -u DISPLAY -u WAYLAND_DISPLAY ZIRAN_LEARN=off build-queue -- sh -c 'python3 scripts/site_kernel_targets.py && python3 scripts/site_kernel_targets.py --check && sh tests/site_pages.sh build/bin/ziran && sh tests/site_targets.sh build/bin/ziran'`: **exit 1** at site_pages, reporting stale generated modules/usage/zx/audio/telegram pages from other lane changes. The native aggregate-lowering rejection that previously blocked this test is repaired.

Full integrated compiler suite/bootstrap are running in the owning SSA/records lane. The main aggregate fix, fresh explicit-engine Alpha receipt, final widget measurements, complete site_pages validation, commit, push and actual site deployment remain pending. No commit or deployment is claimed by this report.

## Aggregate memory-view fix and independent review

Readonly aggregate loads used only by member extraction in the same block now
use the source address. Every nested view must finish reading before any write,
allocation, resize, call or insert; other uses retain the original snapshot.
Checked programs retain their original lowering. Existing frame-based shared
view addresses are reset when adding memory views, including changed views.
Independent review found no demonstrated correctness defect. Added regressions
cover nested reads across stores, cross-block snapshots with reversed block
storage order, distinct pointer bases and changed nested aggregates.

Local tests passed:

```sh
env -u DISPLAY -u WAYLAND_DISPLAY ZIRAN_LEARN=off build-queue -- sh tests/machine_ir.sh build/bin/ziran
env -u DISPLAY -u WAYLAND_DISPLAY ZIRAN_LEARN=off build-queue -- sh -c 'make -j4 all build/bin/native-load-test && sh tests/native_load.sh build/bin/ziran build/bin/native-load-test && ZIRAN_BENCH_HOST=omega build/bin/ziran run --target=c bench/bench.zi -- library-targets --project /mnt/storage/Projects/kryonlabs/kryon --manifest /mnt/storage/Projects/kryonlabs/kryon/tests/widget_benchmarks.json --samples 11'
```

Both commands exited 0. The second command retained the unchanged runtime
hash throughout all measurements. Full receipt:
`widget-omega-2026-11-16-fields-after.json`. All 11 samples for each target
verified the expected result, with checks and learning off.

| Scenario | C ns | Go ns | Native ZX ns | Native speedup vs original | Native / Go |
| --- | ---: | ---: | ---: | ---: | ---: |
| Button | 2260.1 | 3484.4 | 5406.0 | 115.9× | 1.55× |
| Text | 2454.3 | 3279.9 | 6262.2 | 67.5× | 1.91× |
| Input | 2884.3 | 4640.3 | 7427.7 | 102.9× | 1.60× |

The dominant aggregate-copy gap is substantially reduced. Native remains slower
than Go in all three scenarios; the requested Go performance goal is not met.
These results include the current shared compiler work and do not isolate the
individual contribution of other lanes.

The source-driven site generator refreshed five stale pages. The exact command
below exited 0, including both retained site suites and 5,016 local links across
151 pages with zero broken links:

```sh
env -u DISPLAY -u WAYLAND_DISPLAY ZIRAN_LEARN=off build-queue -- sh -c 'python3 scripts/site_pages.py --ziran build/bin/ziran && python3 scripts/site_kernel_targets.py && sh tests/site_pages.sh build/bin/ziran && sh tests/site_targets.sh build/bin/ziran && python3 scripts/site_links.py'
```

The SSA integration lane reports a successful final bootstrap comparison and
278/281 tests in its preceding full local suite; remaining failures were the
explain diagnostic registry, stale site pages (now regenerated and retested),
and Go >=1.25 missing from that test environment. A fresh full-suite result and
explicit-engine Alpha receipt remain pending. No commit, push or deployment
has occurred for this feature set.

## Post-fix warm code confirmation

The existing instrumented loader object was relinked against the current
`libziran.a` for a diagnostic only. The ordinary 11-sample timing receipt
above uses the current unmodified loader. A 3-second warm SIGPROF run verified
result 0 and recorded 749 samples, zero host capabilities, LIR checks 0 and
machine checks 0. Its retained files use the prefix
`widget-button-omega-2026-11-16-fields-` (raw samples, JIT map, summary and
TreeIsBuilding disassembly).

`TreeIsBuilding` is now 108 bytes, allocates a 32-byte frame and reads the
single building byte directly with `movzbl`; the 132,523-byte copy loop is
gone. The new sample leaders are DefaultTheme (95), SessionIndex (82) and
DefaultShadows (77). These identify remaining profiling leads, not an isolated
cost estimate or a completed Go performance target. The original warm samples
and before disassembly are preserved.

## Integration continuation

Fresh local tests on the existing shared build completed successfully:

- `env -u DISPLAY -u WAYLAND_DISPLAY ZIRAN_LEARN=off build-queue -- sh -c 'sh tests/machine_ir.sh build/bin/ziran && sh tests/native_load.sh build/bin/ziran build/bin/native-load-test'` exited 0.
- `env -u DISPLAY -u WAYLAND_DISPLAY ZIRAN_LEARN=off build-queue -- sh -c 'python3 scripts/site_kernel_targets.py --check && sh tests/site_pages.sh build/bin/ziran && sh tests/site_targets.sh build/bin/ziran && python3 scripts/site_links.py'` exited 0: 151 pages, 5,016 local links, zero broken links. External links were not fetched.

The cumulative widget speedups above span different compiler revisions and
include other integrated compiler work; they do not isolate the fields pass.
The remaining warm constructor samples show nested aggregate materialization
and copies in `DefaultTheme` and `DefaultShadows`. That compiler work continues
in the aggregate specialist lane. The Go performance target remains unmet.

Alpha is reachable and reports virtualization `none`. Its canonical compiler
checkout was reset to pushed master after preserving its local changes under
its ignored `backup/widget-native-alpha-baseline/`. The baseline uses only a
benchmark harness overlay to select explicit Engine and Native policies;
receipt hashes must identify that overlay. GNU Make was absent and is now
available from an extracted Ubuntu package in that same ignored backup tree.
Baseline build and both 11-sample measurements completed successfully.
Receipts are `wasm-tier-alpha-2026-11-16-explicit-engine-baseline.json` and
`widget-alpha-2026-11-16-baseline.json`. The compiler is pushed master with
no dirty compiler sources; the benchmark harness overlay is recorded by hash.
The widget fixture revision is recorded separately from the compiler revision.

Alpha baseline nanoseconds per two-frame scenario:

| Scenario | C | Go | Native ZX |
| --- | ---: | ---: | ---: |
| Button | 2781.6 | 4833.8 | 1590729.4 |
| Text | 3172.7 | 3920.5 | 1165004.4 |
| Input | 4195.8 | 6877.1 | 1904851.1 |

These are pre-fix Alpha results, not a comparison to the Omega post-fix table.
The Alpha after measurement and publication remain pending.
