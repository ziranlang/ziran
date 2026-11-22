# i386 runtime performance results

The [direct branch argument copies](#direct-branch-argument-copies) section
records the latest installed runtime milestone. The [hot scalar blocks
comparison](#linuxi386-hot-scalar-blocks) includes the first whole-desktop
timings on physical Alpha.

## Withdrawn candidate

The interpreter candidate described below was withdrawn before installation: its
measurements did not establish an improvement. Only its own optimization hunks
were reversed. Other runtime work, parity tests and raw measurements were
preserved.

The final paired Alpha blend measurement was approximately 4% slower than
baseline, with a roughly 761× gap to the C export.
Initial scalar-loop and isolated division32 improvements did not reproduce.

Raw timing and outlier evidence:

- `rill-row-alpha-2026-11-21.json`: final comparison, inclusive 1.5×IQR
  statistics, all raw batch samples, exclusions, source and archive digests.
- `rill-row-paired.jsonl`: original baseline/current/current/baseline output.
- `rill-row-c.jsonl`: C export of the same final FNV fixture.
- `rill-compact-alpha-2026-11-21.json`: preliminary scalar-loop trials.
- Other `rill-row-*.jsonl` retain intermediate experiments. Earlier records
  with output zero use the original XOR fixture and are separate from the
  final FNV fixture, which returns 1622073797.
- `rill-emu-local-2026-11-21.json`: private-Xvfb compatibility test on Omega,
  not benchmark-of-record performance evidence.

Alpha was a physical host (`systemd-detect-virt` returned `none`), with load
10.13/9.98/10.18 before the final paired comparison. No quiet-host or end-to-end
Rill speedup is established.

## Build and local test commands

In `/mnt/storage/Projects/ziranlang/ziran`:

```sh
env -u DISPLAY -u WAYLAND_DISPLAY make check CHECK_FILTER=lowered_exec CHECK_JOBS=2 JOBS=2
env -u DISPLAY -u WAYLAND_DISPLAY make check CHECK_JOBS=4 JOBS=2
env -u DISPLAY -u WAYLAND_DISPLAY make check CHECK_FILTER=checks_granularity CHECK_JOBS=2 JOBS=2
env -u DISPLAY -u WAYLAND_DISPLAY make check-bootstrap JOBS=2

i386_cc=$(head -n 1 build/linux-386-engine/.compiler-flags)
env -u DISPLAY -u WAYLAND_DISPLAY make \
  BUILD_DIR=build/linux-386-engine CC="$i386_cc" \
  GEN_ZI2C=/mnt/storage/Projects/ziranlang/ziran/build/bin/zi2c \
  CFLAGS='-O2 -fno-pie -fno-pic -DZIRAN_COMPACT_EXEC_PLAN -DZIRAN_EXEC_FRAME_RESERVE_BYTES=6291456 -DZIRAN_EXEC_FRAME_RESERVE_MIN=1048576' \
  JOBS=2 build/linux-386-engine/libziran.a

env -u DISPLAY -u WAYLAND_DISPLAY build/bin/zi2zx \
  --root bench --entry rill_row:main -o build/rill-row.zx bench/rill_row.zi
env -u DISPLAY -u WAYLAND_DISPLAY build/bin/zi2c \
  --no-main --root bench -o build/rill-row-c bench/rill_row.zi
```

The i386 archive build completed successfully. The signed32 boundary and
compact/reference budget-parity local test passed. The first full local suite
passed 274/276; the own benchmark-registration failure was corrected and its
targeted local test passed. The other failure was in `site_pages`, involving
another lane's `std/rune_text.zi` constants.

The final full suite passed 274/276 in 303.6 seconds, including `lowered_exec`
and `bench_entry`. It still failed `site_pages`. It also failed
`checks_granularity_cases.zi` when a `zi2zx` child exited 127 with no stderr.
The targeted granularity rerun passed in 1.4 seconds after rebuilding, without
a source change; the launch failure's cause is unknown. Final bootstrap
verification matched. The whole suite remains unverified as green.

## Alpha execution

Generated C, the bundle, archives and the existing host32 sysroot were copied
to Alpha's existing `Projects/ziranlang/ziran/build/` tree. Its independently
modified sources were preserved. SSH used `/home/wao/.ssh/alpha.conf`.

The final host was compiled on Alpha with this command, substituting
`build/rill-libziran-before.a` or `build/rill-libziran-current.a` for the archive:

```sh
cc -m32 --sysroot=build/rill-sysroot \
  -isystem build/rill-sysroot/usr/include/i386-linux-gnu \
  -Bbuild/rill-sysroot/usr/lib/gcc/x86_64-linux-gnu/14/32/ \
  -Bbuild/rill-sysroot/usr/lib/i386-linux-gnu/ \
  -O2 -no-pie -DMATRIX_ZX -Ibuild/rill-host-inputs \
  build/rill-host-inputs/site_target_host.c build/rill-libziran-current.a \
  -lm -pthread -ldl -o build/rill-host
build/rill-sysroot/lib/ld-linux.so.2 \
  --library-path build/rill-sysroot/lib/i386-linux-gnu \
  build/rill-host build/rill-host-inputs/rill-row.zx 1622073797 11
```

The existing host warms for at least 150 ms, chooses an adaptive batch count
and validates every invocation. Statistics divide each batch by its count
before filtering. The C comparison compiles the generated `rill_row.c` with
`-O2 -Dmain=rill_unused_main`, then links the same host with `MATRIX_NATIVE`,
`MATRIX_HEADER="rill_row.h"` and
`MATRIX_ENTRY=zir_main_994f5cffbcc837e8`.

The baseline archive is preserved in ignored `backup/rill-libziran-before.a`.
The final current-tree archive is `build/linux-386-engine/libziran.a`.
Early member-only trials are not the final full-archive comparison.

## Private emulator test

The existing emu objects were relinked read-only against the initial full
candidate archive, producing the single test output
`taijiosnet/taijios/build/zx-smoke/runtime-emu`. Existing `o.emu`,
`build/zx-smoke/emu` and the installed executable were preserved. No source
or ZX module ABI change was needed for this interpreter patch.

In `/mnt/storage/Projects/taijiosnet/taijios`:

```sh
env -u DISPLAY -u WAYLAND_DISPLAY python3 \
  /mnt/storage/Projects/kryonlabs/rill/scripts/taiji-idle-probe.py \
  --root "$PWD" --emu "$PWD/build/zx-smoke/runtime-emu" \
  --output "$PWD/build/zx-smoke/runtime-probe.json" --seconds 30
```

This local test completed successfully: 888.2 ms to the first composed frame,
30.766% of one core over 30 seconds, and visible menu/Escape responses.
The installed Rill bundle and initial candidate archive were used; the later
narrow-division change is excluded. A later staged-Rill probe belongs to the
Rill idle worker and must not be mixed with this sample.

The parent coordinator owns the next emulator rebuild for a shared workspace
ABI change. No further emulator relink, commit, push, release or installation
was performed by this task.

Original gdb sampling evidence and scripts were preserved by the Rill idle
worker at Taiji's ignored `backup/idle-performance/legacy-probes/`, including
`samples.txt`, `sample.sh`, `threads.sh` and the `prof` tree.

## Canonical integer execution

The current target is 100×. This candidate reduces work in the portable ZX
interpreter's numeric dispatch. It uses the canonical values already held in
integer slots, reads the left operand's type by address, and looks up edge
metadata only for jumps and branches. Arithmetic retains its original traps
and result canonicalization. The patch adds no persistent runtime storage.

The physical Alpha profile used the rebuilt SAFE i386 archive and the packed
engine-only reader. Of 488 CPU-time samples, 147 landed in integer execution,
118 in numeric dispatch, 62 in instruction dispatch, and 49 in value dispatch.
Rill's separate private profile also found substantial work in checked address
derivation and loads. The row kernel measures an interpreter component of Rill.

The two runs below launched Linux i386 binaries directly on Alpha's physical
kernel on the Intel Core i7-8700. Both variants verified an identical bundle
and checked every invocation against its checksum. The run order was
baseline, candidate, candidate, baseline, candidate, baseline, baseline,
candidate, with 15 adaptive batch samples per process. Each sample is divided
by its batch count before pooling. The same inclusive 1.5×IQR rule is applied
to both tails for both variants. The JSON report retains all batches and
excluded indices.

| Workload | Ziran SAFE / candidate | Alpha rebuild of SAFE / candidate |
| --- | ---: | ---: |
| Rill-class blend row | 6.348244 / 5.214782 ms (1.217×) | 8.295467 / 5.544458 ms (1.496×) |
| Sieve | 19.813464 / 18.208352 ms (1.088×) | 22.629896 / 19.160914 ms (1.181×) |
| Records | 24.462469 / 20.257767 ms (1.208×) | 22.496754 / 23.838591 ms (0.944×) |

The first run uses the parent-provided SAFE archive. The second rebuilds the
SAFE and candidate interpreter members using the same Alpha GCC 15.2 and inputs.
The other 96 members are byte-identical in both archived variants, the Ziran
SAFE archive, and the Taiji link input inspected on Omega. Both preliminary
runs span variable host load and frequency. The clock- and affinity-gated
repeat below is the primary measurement.

On Alpha, stage the compiler inputs with their digests and generate both
versions with the same `zi2c` executable. `src`, `out`, `base`, `sysroot`, and
`archive` below name the staged source tree, generated C directory, benchmark
input directory, i386 sysroot, and copy of the SAFE archive being tested.
The command for each version is:

```bash
cc32=(cc -m32 --sysroot="$sysroot" -isystem "$sysroot/usr/include/i386-linux-gnu"
      -B"$sysroot/usr/lib/gcc/x86_64-linux-gnu/14/32/" -B"$sysroot/usr/lib/i386-linux-gnu/")
(cd "$src" && ZIRAN_ROOT="$PWD" "$base/zi2c" --no-main --root cmd \
  --define ZIRAN_EXEC_NATIVE_PLAN_INDEXES --define ZIRAN_EXEC_PACKED_INSTRUCTIONS \
  -o "$out" cmd/compiler_runtime.zi)
"${cc32[@]}" -O2 -fno-pie -fno-pic -D_GNU_SOURCE -std=c11 \
  -I"$src/include" -I"$src/cmd/zir" -Werror=frame-larger-than=16384 -fno-math-errno -fvisibility=hidden \
  -iquote "$out" -include "$base/engine_names.h" -DZIRAN_ENGINE_SOURCE_compiler_exec \
  -c -o "$out/compiler_exec.o" "$out/compiler_exec.c"
ar r "$archive" "$out/compiler_exec.o"
"${cc32[@]}" -O2 -no-pie -DMATRIX_ZX -DMATRIX_ZX_COMPACT -I"$base" \
  "$base/site_target_host.c" "$archive" -lm -pthread -ldl -o "$host"
"$sysroot/lib/ld-linux.so.2" --library-path "$sysroot/lib/i386-linux-gnu" \
  "$host" "$bundle" "$expected" 15
```

The generic bundles use `zi2zx --root bench --entry wasm_tier:Sieve` and
`--entry wasm_tier:Records` on `bench/wasm_tier.zi`. Expected results are
2262 and 68788. The blend fixture uses the original FNV ZX image with expected
result 1622073797. The benchmark host's `MATRIX_ZX_COMPACT` mode opens those
images with `BundleOpenEngineReadAt` and selects `BUNDLE_BACKEND_ENGINE`.

### Clock- and affinity-gated repeat

The same-toolchain comparison was repeated with the repository's `HostGate`.
Each invocation inherited one logical CPU and a sustained clock target of
4.30 GHz. Five invocations were retried because the sibling was more than 15%
busy. The receipt retains their observations and timings. Subsequently, the
same inclusive 1.5×IQR rule was applied to each set of 60 accepted timings.
The exact driver and source manifest are retained alongside the two archives
in `backup/rill-runtime-selection`. From the Ziran root on physical Alpha,
run `PYTHONPATH=scripts python3 backup/rill-runtime-selection/measure-gated.py`
with that staged input set. The driver appends its raw runs to
`measurement-gated.jsonl` in the same directory.

| Workload | SAFE median | Candidate median | Speedup |
| --- | ---: | ---: | ---: |
| Rill-class blend row | 6.260038 ms | 4.557757 ms | 1.374× |
| Sieve | 18.301850 ms | 17.278937 ms | 1.059× |
| Records | 20.048241 ms | 19.579293 ms | 1.024× |

Local verification commands ran without inherited display variables:

```sh
env -u DISPLAY -u WAYLAND_DISPLAY make check CHECK_FILTER=lowered_exec CHECK_JOBS=2 JOBS=2
env -u DISPLAY -u WAYLAND_DISPLAY make check CHECK_JOBS=4 JOBS=2
env -u DISPLAY -u WAYLAND_DISPLAY make check-bootstrap JOBS=2
env -u DISPLAY -u WAYLAND_DISPLAY bash backup/rill-runtime-selection/verify-i386.sh
```

The targeted run passed `lowered_exec` in 73.6 seconds. Bootstrap matched.
The full suite executed all 276 retained checks and passed 275 in 224.6
seconds. `lowered_exec`, `native_instance_cases.zi`, `zx_lowered_parity`,
`zx_module_runtime`, and `std_image_linux.sh` passed. `site_pages` failed on
`RUNE_ERROR` and `MAX_RUNE` in the separate `std/rune_text.zi` changes.
The exact Alpha SAFE rebuild and candidate also passed the hosted-module
fixture linked into Linux/i386 processes on Omega. This exercised host calls,
fuel, borrowed strings and packed reader parity as a local correctness test.

The integrator linked the exact Alpha candidate archive against existing
Taiji emulator objects and ran the unchanged Rill bundle in a private 128 MiB
guest. The desktop lifecycle test exited 0 and reported a main-pool high water
of 52,293,888 bytes. It covered initial paint, the menu, Counter, move/resize,
minimize/restore, Explorer, multiple applications, Settings and close.
The exact archive is installed in the Taiji link input and linked into the
installed Linux/386 emulator. The installed binary passed a second private
128 MiB lifecycle with the unchanged Rill bundle and a main-pool high water of
52,293,920 bytes. Both guest runs are correctness evidence. The source remains
uncommitted while the full local suite has a failure and its packed runtime
predecessor is absent from the branch tip.

## Borrowed scalar slice host calls

The portable engine now supports `BundleInstanceBorrowHostSlice` and
`ZxModuleProgramBorrowHostSlice`. They opt a fixed-width scalar slice on a
`#reads` binding into a callback-scoped, possibly unaligned little-endian byte
view. The `HostValue` layout and ordinary mutable slice adapter are unchanged.

The benchmark ran on physical Alpha (`alpha`, `systemd-detect-virt` reported
`none`, kernel `7.0.0-43-generic`) with Linux/i386 binaries built on Alpha using
GCC 15.2 and the same toolchain and sysroot for the four replacement archive
members. Both variants use the same verified bundle, checked result and host
driver for each workload. The archive for the baseline is the previous
installed checkpoint. Both drivers add every element to the result, and the
ordinary engine case expands, re-encodes and compares the `HostValue` images.

| Workload | Before, ms/run | After, ms/run | Before/after |
| --- | ---: | ---: | ---: |
| Read 1,024 `u32` elements | 0.082406082 | 0.001537022 | 53.6141× |
| Read 32,768 `u32` elements | 3.908443750 | 0.014755665 | 264.8775× |
| Rill blend row | 4.570867000 | 4.710499500 | 0.9704× |
| Sieve | 17.336642000 | 17.058893000 | 1.0163× |
| Records | 20.597030000 | 20.473803000 | 1.0060× |

The first two ratios measure one guest call, including the callback's sum.
The row and generic controls run without registering the new ABI. The row
took 3.1% longer and the other controls took slightly less time. Whole Rill
frame time was outside this measurement.

Each workload used the sequence ABBA BAAB, with 15 samples per accepted job.
The host gate records rejected attempts and their CPU clocks and sibling load.
Batch times are normalized by batch count before applying symmetric 1.5×IQR
fences using inclusive interpolated quartiles. Raw times, exclusions, bundle
and binary hashes are in [the Alpha receipt](zx-host-slice-alpha-2026-11-21.json).
The benchmark driver is `bench/benchmark_zx_host_slice.py`.

Interpreter CPU-time samples from Alpha used the final archive. With the
large legacy slice, `exec_from_image`, `exec_same_image_value` and
`exec_to_image` account for 906 of 1,769 samples. With borrowing, the
callback's summation accounts for 1,473 of 1,693 samples. The packed Rill row
spends 1,369 of 1,962 samples in integer arithmetic, numeric and value
dispatch, and per-instruction dispatch. It contains no borrowed host calls.
The sample counts and original program-counter samples are in
`backup/rill-runtime-selection/host-slice/strict/` with the build inputs.

The candidate archive SHA-256 is
`4cce1d9fc690f5a12404f57f251f3cf9a7f78473a7db70534ef70a82749bbc60`.
The baseline SHA-256 is
`336f79b54c4d44a072b73aada4e17b48540b8d901a5848400d167d82dcf77feb`.
The candidate replaces only `compiler_exec.o`, `zir_host_exec.o`, `zir_host.o`
and `zir_zx_module.o`. The exact source manifest SHA-256 is
`8903a37b993d67c7192a14b0260939fa95f602382a4acc3e744434df8fa08eb5`.
Build inputs and artifacts are preserved under
`backup/rill-runtime-selection/host-slice/strict/`. The candidate passed
retained hosted parity in an actual 32-bit process on Omega before handoff
to Taiji. The initial archive and receipt are preserved separately.

The affected local commands and receipts are:

- `env -u DISPLAY -u WAYLAND_DISPLAY make check CHECK_FILTER=zx_module_runtime CHECK_JOBS=2 JOBS=2`
  passed 1/1 with the final code (`a289ad619982f53189626198b182ead863d3`).
- `env -u DISPLAY -u WAYLAND_DISPLAY make check CHECK_JOBS=2 JOBS=2`
  passed 275/276 on the final tree (`2e9e23a3b03d77e0614e2ca08eb80497c4f0`).
  `lowered_exec`, `zx_lowered_parity`, native-instance tests, `zx_module_runtime`
  and the image tests passed. Site generation progressed after correcting
  the two rune constants. `site_pages` still reports 14 stale generated
  pages from other working-tree changes.
- `env -u DISPLAY -u WAYLAND_DISPLAY bash backup/rill-runtime-selection/host-slice/verify-i386.sh "$PWD/backup/rill-runtime-selection/host-slice/strict/artifacts/candidate.a"`
  passed hosted module, borrowed scalar and string, fuel and packed-reader parity
  (`9dd317a5c50e3910e6fed2792024161743e2`). The additional mismatched slice
  metadata case passed against both host builds (`7ca3532db7e79c7556572c6bcbe88861d615`).
- `env -u DISPLAY -u WAYLAND_DISPLAY make check-bootstrap JOBS=2` passed the
  native, Darwin and SIMD bootstrap comparisons (`c182a2cf70421d7053ab84aa1758d256c55d`).

Taiji's staged emulator linked the exact archive (`32a7c2e2e7cf73b14e8a2e479d763b526076`).
The private test rejected malformed row uploads and matched RGBA and packed
RGB pixels (`ee8f763d5dab23ad98a91d5d968b21c1ba11`). The 128 MiB Rill lifecycle
passed first paint, search, launch, move, resize and task restore, with a
main-pool high water of 52,228,960 bytes (`8c65279f3891143604911938231a3c0e8fec`).
The refreshed lifecycle also passed Explorer, multiple apps and Settings.
The main pool peaked at 54,206,688 bytes with the staged artifacts
(`36f71c2631a258a66d684e1d1b182eab4068`). The private workspace, image budget,
PNG/JPEG decode and deliberate fuel exhaustion tests passed
(`a177677b565f90f55c9b8afb2bef0558ce54`, `7944b9acad822589db13868ba2fdd3862873`).

The matched set was installed in Taiji. Readback confirmed the exact candidate
as `build/host32/libziran.a`, emulator SHA-256
`7cd55d19ebe809ba6edc16933a2ba5cddae3d3f40271b63733fb17139ce04ea8`, and
Rill bundle SHA-256 `6e03bcecfaa73008b9cbb60c43764b2e9bc14c978ca4d46328a3a5d7a536e3aa`
(`f19f9f9de16c5ee2f582fa4136eceb48652e`). The installed emulator and bundle
passed the private 128 MiB lifecycle again. The main pool peaked at
52,221,440 bytes (`3af76448f5e614ed99ec9fecabe504c7242c`). The owner's app
was not restarted. Taiji's install inventory is
`backup/idle-performance/runtime-pair/strict-installed.json`.

The installed guest test command ran from the Taiji checkout.

```sh
python3 tests/desktop/rill_lifecycle_test.py dis/wm/rill.zx \
    6e03bcecfaa73008b9cbb60c43764b2e9bc14c978ca4d46328a3a5d7a536e3aa \
    --emu Linux/386/bin/emu --report-memory --pool 128M --desktop-extras \
    --failure-frame backup/idle-performance/strict-installed-lifecycle-failure.png
```

Whole-UI timing is still open. Physical Alpha is reachable and Taiji was
absent at the standard checkout paths (`3fba45b6ceccc8d1cb332d95178e4211e18f`,
`4e09e6fa7140b8e102ed5fd5190423be9370`).

## Linux/i386 hot scalar blocks

The packed interpreter profile attributed 1,369 of 1,962 row-workload
samples to integer arithmetic and dispatch. The candidate runs hot spans of
8-, 16- and 32-bit scalar instructions as native i386 code inside the packed
engine. Each span uses the interpreter's scalar frame words. The interpreter
still handles addressing, host calls, edges, and fault reporting.

The first physical Alpha comparison used the installed strict packed baseline
archive `4cce1d9fc690f5a12404f57f251f3cf9a7f78473a7db70534ef70a82749bbc60`
and candidate archive
`96b4cd6a13dd56c0b94c8957d3002905cdc7295c398ae9242c0834700cce74b2`.
The candidate source manifest has SHA-256
`a0ff04a53aa9dd1a38aafa008159f7c823270630e393bed94cd8226f1b1e1c68`.
Both executables were linked on Alpha using the same GCC 15.2 32-bit toolchain,
drivers, and ZX bundles. The recorded order was ABBA BAAB, with four accepted
processes and 60 raw samples per arm per workload. Both arms used the same
inclusive 1.5×IQR rule. The full collection and rejected indices are in the
[raw Alpha results](zx-i386-blocks-alpha-2026-11-22.json).

| Workload | Baseline median (ms) | Candidate median (ms) | Baseline / candidate | Excluded samples, baseline / candidate |
| --- | ---: | ---: | ---: | ---: |
| Rill row | 4.396557 | 1.325508 | 3.317× | 0 / 1 |
| Integer blocks | 25.048616 | 7.469748 | 3.353× | 13 / 3 |
| Sieve | 16.376815 | 16.929766 | 0.967× | 1 / 0 |
| Records | 18.540126 | 19.141050 | 0.969× | 1 / 2 |

The measured Rill gain covers the row fixture. The separate private 128 MiB
guest run and whole-UI timings are pending for this implementation.

The exact candidate passed the retained Linux/i386 hosted parity suite.
The suite compared host effects, fuel exhaustion, bounds faults, and exact
division and shift failure locations. It also detected live RX pages during
hot execution and verified their release on instance close. The strict
baseline passed the same suite with the RX assertion disabled.

SIGPROF on Alpha attributed 34 of 979 Sieve samples and 32 of 1,100 Records
samples to the initial block hook. A second build reuses the interpreter's
frame in that hook and reduces the work done on cache misses. The same
baseline archive, input bundles, process order and sample rules were used.
See the [tuned raw Alpha results](zx-i386-blocks-tuned-alpha-2026-11-22.json).

| Workload | Baseline median (ms) | Tuned median (ms) | Baseline / tuned | Excluded samples, baseline / candidate |
| --- | ---: | ---: | ---: | ---: |
| Rill row | 4.507549 | 1.275633 | 3.534× | 3 / 5 |
| Integer blocks | 25.046210 | 7.174960 | 3.491× | 3 / 0 |
| Sieve | 16.598459 | 16.460535 | 1.008× | 3 / 4 |
| Records | 18.616992 | 18.726593 | 0.994× | 0 / 0 |

The tuned archive SHA-256 is
`976fb19d1048360d94b528abe177f0bd7684e0c6ac0dbca2cf2378a7224be68a`.
The source manifest SHA-256 is
`6dffaf3612e1d86a1eb0dd4d13eda454d8af8a4b53d384d67a2f1837ed0f7ec4`.
The exact Alpha archive passed the retained 32-bit hosted suite with the
RX assertion enabled (`f33f3ab0d286c2da2138ea18c671512adb87`).
The host gate moved from CPU 3 to CPU 1 during Records retries in this
comparison. The repeat below used the identical engine binaries, driver
inputs, order and rules, with all accepted runs on CPU 3 and every run
observing the sibling CPU 9. The existing gate retained its frequency and
sibling load requirements for both arms. See the
[same-core raw Alpha results](zx-i386-blocks-same-core-alpha-2026-11-22.json).

| Workload | Baseline median (ms) | Tuned median (ms) | Baseline / tuned | Excluded samples, baseline / candidate |
| --- | ---: | ---: | ---: | ---: |
| Rill row | 4.430269 | 1.294645 | 3.422× | 0 / 2 |
| Integer blocks | 24.892730 | 7.012384 | 3.550× | 3 / 5 |
| Sieve | 16.683282 | 16.557930 | 1.008× | 0 / 0 |
| Records | 18.777873 | 18.823682 | 0.998× | 4 / 2 |

The pinned repeat ran on Alpha from the canonical Ziran directory.

```sh
base=backup/rill-runtime-selection/i386-blocks/tuned
taskset -c 3,9 python3 "$base/source/bench/benchmark_zx_host_slice.py" \
    --hot-blocks build/rill-sysroot/lib/ld-linux.so.2 \
    build/rill-sysroot/lib/i386-linux-gnu "$base/artifacts" "$base/results-same-core.json"
```

The tuned archive, source snapshot, manifest and physical Alpha build script
are preserved under `backup/rill-runtime-selection/i386-blocks/tuned/`.
Its hosted parity was run from the canonical Ziran directory.

```sh
env -u DISPLAY -u WAYLAND_DISPLAY ZIRAN_TEST_I386_BLOCKS=1 \
    bash backup/rill-runtime-selection/host-slice/verify-i386.sh \
    "$PWD/backup/rill-runtime-selection/i386-blocks/tuned/artifacts/candidate.a"
```

The Taiji integrator installed the exact tuned archive as
`build/host32/libziran.a` and its linked Linux/i386 emulator. Install readback
confirmed emulator SHA-256
`675f03b0de6dd62ebf50100a0e15c21513d39f546b5f8863a6745f3315d418ec`
and the tuned archive hash (`08a8f227e75a398681d11625aae0ffe9f525`). The
installed defaults passed the private 128 MiB desktop lifecycle with no
overlay, including search, launch, move, resize, restore, Explorer and
Settings. The main pool peaked at 52,229,984 bytes
(`bd8741b6d7a2b9c0ddb705d9e1ea22f6aa84`). The owner's running app was left
alone. Taiji's receipt is
`backup/idle-performance/runtime-pair/hotblock-installed.json`.

The ratios above measure standalone ZX workloads. The integrator also ran
the matched private desktop in baseline, tuned, tuned, baseline order on
physical Alpha (`5cb66a419e44e808937667e158f808406650`). The observed
first-paint medians were 3,406.4 ms and 3,312.2 ms (two boots per archive).
The observed mouse-to-paint upper-bound medians were 1,206.75 ms and
1,197.65 ms, and Escape-to-paint medians were 375.8 ms and 376.9 ms
(14 observations per archive for each). These desktop medians include all
observations. They show a small startup change and similar input latency.

The supplemental hot-block and pointer manifests retained the strict
archive hash in the tuned archive field. The recorded emulator hash and
bundle match the tuned install. The integrator verified the installed
archive and recorded the corrected identities in Taiji's
`backup/idle-performance/alpha-results/manifest-provenance-corrections.json`.
The original samples and manifests remain available as collected.

The tuned tree's `env -u DISPLAY -u WAYLAND_DISPLAY make check CHECK_JOBS=2 JOBS=2`
executed all 276 retained tests and passed 275. Lowered execution, native and
packed parity, hosted execution, and the image tests passed. Only generated
package documentation failed (`8dd5a8fdcf0e773d7acd016cade58186cdd9`).
`env -u DISPLAY -u WAYLAND_DISPLAY make check-bootstrap JOBS=2` passed.
The site-only retry stopped at a dependency-lock mismatch in the separate
`mesh-client` checkout (`e0f7ddf71c220275dd5e8880b75cf5c90b89`).

## Direct branch argument copies

The next profile on physical Alpha attributed 162 of 832 integer fixture
samples and 76 of 767 Rill row samples to `ExecEdge`. Resolving each slot
once reduced branch transfer time while preserving the alias check,
cyclic-move scratch path and both original work charges.

Both waves used the same physical CPU core, archives, linked executables and
ZX bundles. The tuned hot-block archive is the baseline. Each wave ran
ABBA BAAB with 60 samples per arm before symmetric inclusive 1.5×IQR
filtering. Raw host checks, attempts, timing samples, and exclusion indices
are in the [first run](zx-i386-direct-edges-alpha-2026-11-22.json)
and [repeat](zx-i386-direct-edges-repeat-alpha-2026-11-22.json).

| Workload | First baseline → candidate (ms) | Repeat baseline → candidate (ms) | Speedup, first / repeat |
|---|---:|---:|---:|
| Rill row | 1.336534 → 1.245376 | 1.317483 → 1.226914 | 1.0732× / 1.0738× |
| Integer blocks | 7.348825 → 6.094210 | 7.327703 → 6.063479 | 1.2059× / 1.2085× |
| Sieve | 17.126228 → 15.357509 | 16.970928 → 15.176902 | 1.1152× / 1.1182× |
| Records | 19.579122 → 19.256112 | 19.407722 → 19.028392 | 1.0168× / 1.0199× |

The exact candidate archive is
`backup/rill-runtime-selection/i386-blocks/edges/candidate.a` with SHA-256
`8d62499e93a17c52560aef1418ef1adc5f5dd4f122c6ac798d273d00be526f99`.
Only `compiler_exec.o` differs from the baseline. The matching source
manifest SHA-256 is
`274da85b0f8a45d73f5b700cb6f6180c3cad657c4aba175ce2943a0887c9ad11`.
The exact archive passed Linux/i386 hosted parity with the executable-page
assertion (`34247da029877c895e1f7fdd040c1ad148e3`). The Taiji integrator
installed the matched archive and emulator. The installed defaults passed
the private 128 MiB desktop lifecycle, with a main-pool peak of
54,414,976 bytes (`43bca4b430d0769470e38a80e95e0cc68117`).

## Bulk copies for aggregate loads

A private Taiji desktop profile on physical Alpha located 956 of 1,019
`ExecCopyAddressToValue` samples in the aggregate byte-copy loop
(`602198d97c0b07cea07e4507460f57e007f2`,
`74c16525ad66a05bba8195c601d26e1d7ad7`). The C runtime now uses
`memcpy` for backed, disjoint aggregate loads of 32 bytes or more. Small
and overlapping copies retain their original byte order. Address checks,
extent checks, and work charges still precede the write. The aggregate
fixture reads a 32 KiB snapshot, then changes the source and reads the
snapshot to verify independent storage.

Both waves used the same physical CPU 3 and sibling CPU 9, the installed
direct-edge archive as baseline, and identical driver objects and ZX
bundles within each workload. Each arm kept its linked executable for
both waves. Alpha compiled and measured both arms.
The ABBA BAAB order gave 60 accepted raw samples per arm per workload
before applying symmetric inclusive 1.5×IQR filtering. Attempts, host
gates, raw times and exclusion indices are in the
[first wave](zx-i386-aggregate-load-alpha-2026-11-22.json) and
[repeat](zx-i386-aggregate-load-repeat-alpha-2026-11-22.json).

| Workload | First baseline → candidate (ms) | Repeat baseline → candidate (ms) | Speedup, first / repeat |
|---|---:|---:|---:|
| Aggregate load | 354.461815 → 328.903011 | 349.159913 → 325.468018 | 1.0777× / 1.0728× |
| Rill row | 1.212168 → 1.189016 | 1.196927 → 1.199195 | 1.0195× / 0.9981× |
| Integer blocks | 5.937757 → 5.808910 | 5.907725 → 5.819688 | 1.0222× / 1.0151× |
| Sieve | 15.056216 → 15.247660 | 15.304128 → 15.327962 | 0.9874× / 0.9984× |
| Records | 19.000420 → 19.135337 | 18.448567 → 18.622705 | 0.9930× / 0.9906× |

The exact candidate archive is
`backup/rill-runtime-selection/i386-blocks/aggregate-load/candidate.a`
with SHA-256 `9ecfc14049bec0988a55ebb6846d1833b580b3d6b5e65ed363895c53480675f4`.
Only `compiler_exec.o` differs from the installed direct-edge archive.
The source manifest SHA-256 is
`824ce2297727276d1ea9ad367e2ebbad7ef4cec446d90df1b1f56219739f9d72`.
The same directory contains the Alpha build and measurement scripts. The
exact archive passed the Linux/i386 hosted tests with the executable-page
assertion (`9029dd1433f778934acc91c5fae94ae1d603`).

To repeat the measurement on physical Alpha from the canonical directory,
use a fresh filename for the raw result.

```sh
bash backup/rill-runtime-selection/i386-blocks/aggregate-load/measure-alpha.sh results-reproduction.json
```

The archive parity test ran separately on Omega.

```sh
env -u DISPLAY -u WAYLAND_DISPLAY ZIRAN_TEST_I386_BLOCKS=1 \
    bash backup/rill-runtime-selection/host-slice/verify-i386.sh \
    "$PWD/backup/rill-runtime-selection/i386-blocks/aggregate-load/candidate.a"
```

The Taiji integrator ran a matched private 128 MiB desktop comparison
on physical Alpha with the same Rill bundle and pointer runner in both
arms (`c3d565efa99357b938325d89cd46a6a2109f`). Direct-edge and bulk-copy
archives booted in ABBA order on CPU 3. The upper bounds for menu and
Escape response measure input to the sampled paint. The raw observations
are retained below as measured. Each arm used inclusive 1.5×IQR with
zero-based exclusion indices.

| Desktop metric | Raw median, direct edge → bulk (ms) | Retained median, direct edge → bulk (ms) | Exclusions, direct edge / bulk |
|---|---:|---:|---|
| First paint | 3328.55 → 3036.75 | 3328.55 → 3036.75 | [] / [] |
| Menu | 726.65 → 481.90 | 725.80 → 481.50 | [0, 7] / [0, 7] |
| Escape | 383.50 → 272.95 | 396.05 → 264.15 | [2, 10] / [1, 8] |

```text
First paint, direct edge: 3443.9, 3213.2
First paint, bulk: 3035.6, 3037.9
Menu, direct edge: 1261.3, 718.2, 726.6, 726.7, 734.5, 719.3, 756.6, 1262.8, 716.9, 711.5, 744.1, 733.2, 718.2, 725.0
Menu, bulk: 849.8, 465.0, 482.0, 446.3, 487.3, 455.7, 483.5, 933.1, 480.1, 481.8, 484.2, 481.2, 503.6, 450.2
Escape, direct edge: 385.6, 441.1, 272.6, 441.5, 381.4, 375.6, 372.1, 410.0, 406.9, 376.5, 288.8, 406.5, 373.1, 407.0
Escape, bulk: 301.2, 396.8, 257.7, 294.5, 255.0, 278.2, 252.0, 302.7, 365.9, 287.8, 260.6, 208.4, 267.7, 242.3
```

The integrator verified guest lifecycle, row, workspace, PNG/JPEG and fuel
regressions, then installed the exact archive with its matching emulator
and verified the installed bytes (`0f402b1d3a95d73f88003c90c8cf9437206e`).
Two boots per arm limit the startup estimate. The same-pointer menu
median improved 1.51× with or without IQR exclusion.

`env -u DISPLAY -u WAYLAND_DISPLAY make check CHECK_JOBS=2 JOBS=2`
passed 275 of 276 retained local tests without reuse
(`a89a9a1a691a19c84dff9592389b8700bcec`). The only failure was
`site_pages`, which could not resolve mesh-client's C-only `Native`
functions under the default ZX API target.
`env -u DISPLAY -u WAYLAND_DISPLAY make check-bootstrap JOBS=2`
passed on the same runtime source before the benchmark registry fix
(`07e830aaf039394c1757614fe92935e816cd`).

After adding checked API targets and regenerating the affected package and
standard module documentation, all 276 retained tests passed without reuse
(`0cd799d67ee2a157e72fa87735940cdf8294`). Bootstrap also matched
(`1028079288258a1702e846349681fb0a1d7a`).

## Scalar type lookup

In an exact physical Alpha desktop profile of the installed bulk-copy emulator,
272 of 3,635 samples fell in packed numeric dispatch. The comparison call site
accounted for 129 samples. Its i386 caller copied the 52-byte `LirType` record
before each comparison. Comparisons and conversions now read immutable type
records in place. Floating-point instructions use the existing width-dependent
decoder.

Physical Alpha compiled both drivers and the candidate archive with GCC 15.2.
The installed bulk archive is the baseline. The only changed member out of 97
is `compiler_exec.o`. Both waves used CPU 3, observed sibling CPU 9, and used
ABBA BAAB ordering and identical ZX bundles. Each arm and workload has 60
host-gated samples. The two raw files include clock-rejected attempts and the
indices excluded by symmetric inclusive 1.5×IQR. [First wave](zx-i386-scalar-types-alpha-2026-11-22.json), [repeat](zx-i386-scalar-types-repeat-alpha-2026-11-22.json).

| Workload | First wave baseline → candidate | Repeat baseline → candidate |
| --- | ---: | ---: |
| Rill row | 1.233904 → 1.181268 ms (1.045×) | 1.179756 → 1.161774 ms (1.015×) |
| Integer blocks | 5.856730 → 5.642859 ms (1.038×) | 6.074680 → 5.638524 ms (1.077×) |
| Sieve | 14.953606 → 14.451802 ms (1.035×) | 14.778665 → 14.018898 ms (1.054×) |
| Records | 18.805164 → 17.742711 ms (1.060×) | 18.522154 → 17.568141 ms (1.054×) |
| Aggregate load | 333.953916 → 336.368017 ms (0.993×) | 323.929136 → 325.363412 ms (0.996×) |

The exact timed archive is
`backup/rill-runtime-selection/i386-blocks/scalar-types/candidate.a`,
SHA-256 `5fea529dff16abb6441704968dbd4a947a60598b5b18f4be05f8f7b78f2ca0c5`.
The adjacent source manifest has SHA-256
`b587ecc52c6124985c8d0042a546acaf520eaadf47c6a1c925b85bd573b47f87`.
The same directory has the build and measurement scripts. On physical Alpha,
run `bash build-alpha.sh`, then `bash measure-alpha.sh results.json` and
`bash measure-alpha.sh results-repeat.json` from that directory.
The retained Linux/i386 hosted and executable-page parity test passed against
the exact archive (`7aed1c9dca302f584aa56fd88b1f544f4618`). The integrator
received it for a private 128 MiB run against the installed Rill bundle.
That lifecycle passed and peaked at 52,212,992 bytes in the main pool.
`env -u DISPLAY -u WAYLAND_DISPLAY make -j4 check CHECK_JOBS=8` executed
and passed all 276 tests. Bootstrap matched on the same source
(`07194d0eb1aa4e298d0e503289291a846e92`).
