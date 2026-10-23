# Runtime and compiler benchmarks

## Complete native builds

```sh
env -u DISPLAY -u WAYLAND_DISPLAY python3 bench/native_builds.py \
  --out build/benchmarks/native-builds
```

Equivalent modular Ziran, C and Go programs produce an independently computed
64-bit recurrence. Every build runs a small checksum oracle; runtime samples
run about twenty million module calls. The default matrix uses 1, 16 and 64
modules, five build samples and nine runtime samples after warmups.

`fresh` removes fixture build outputs and changes comments in every application
source to miss Go application caches while preserving standard-library/toolchain
caches. `unchanged` preserves inputs and outputs. `leaf-edit` changes one leaf
body; `shared-edit` changes an imported constant used by all modules. Each edit
uses a new value. C uses Make dependency files; Go uses `-p=4`/`GOMAXPROCS=4`;
Ziran uses the ordinary native `--exe` driver. The Go cache is private to the
benchmark and its first toolchain warmup is recorded separately.

This measures whole build commands rather than adding separate phase times.
Source/command/tool hashes, raw samples, CPU time, artifact size and peak child
RSS are retained. Peak RSS is not the sum of concurrent compiler memory.
C's dynamic system libraries and Go's normal linked runtime have different
packaging costs. Results describe these kernels on one shared host with warm
filesystem caches, not a general language ranking or cold-machine build claim.
Reuse the stable output after archiving needed receipts and checking that its
outputs are inactive.

Add `--before-bin-dir DIR` to include a preserved `ziran` launcher and `zi2c`
compiler as `ziran-before`. Both Ziran variants use the same source files and
flags, separate output directories, and shuffled build/run order alongside C
and Go. The metadata records generated-C and executable byte identity without
requiring it for comparisons that intentionally change generated code.

Opt-in compiler profiles for native `--exe` builds include
`native.toolchain` (compiler identity and cache preparation), `native.objects`
(cache validation, compiler workers and dependency recording), and `native.link`
(linking and atomic executable replacement). Counters record compilation
attempts, reused objects and event/poll completion paths. Collect profiles
separately from timings; profiler counters add work and the profile environment
is part of the native object-cache identity.
`native.cache.lookup` and `native.cache.record` separate validation from
dependency recording within the object stage.

`native_process_spawn` and `native_process_fork` count the tool-launch paths,
including compiler probes. Native builds prefer `posix_spawnp` with the same
capture-pipe actions and inherited output lock. A failed spawn uses the
existing fork/exec path, which also preserves executable shell scripts without
a shebang. These counters identify the path taken; they do not imply a build
speed improvement on every host or workload.

The `native_probe_pipe` and `native_probe_compile` counters distinguish GCC's
piped empty-input probe from the ordinary compile probe retained for Clang and
other eligible drivers. These probes run even when all objects are reusable;
examine `native.toolchain` separately from object validation and linking.
Successful piped probes also enable piped object compilation. The
`native_object_pipe` counter records object attempts with that flag, including
an explicit `-pipe` in `CFLAGS`; reused objects do not increment it. Compilers
whose probe is unsupported or unsuccessful retain ordinary compilation.
This uses GCC's documented [pipe between compilation stages](https://gcc.gnu.org/onlinedocs/gcc/Overall-Options.html),
while retaining separate translation units and the existing worker limit.

For a complete build of the formatter application itself:

```sh
env -u DISPLAY -u WAYLAND_DISPLAY python3 bench/formatter_builds.py \
  --before-bin-dir PATH/TO/PRESERVED/bin \
  --out build/benchmarks/native-programs/formatter
```

This compares fresh and unchanged source/saved-IR builds with the same flags.
Each executable formats an unformatted fixture containing a Unicode comment
and a record, and checks the canonical result against the existing formatter.
Before/after generated C and executable bytes must match. Profiles are
collected separately from timings; the current checker prepares the saved IR
for both compiler versions. The metadata retains all source/tool hashes and
profiles. A large change on many small modules need not improve this application
by the same amount; report both results and retain the warm-cache/shared-host
limitations.

For a large shared header graph, use `bench/native_header_graphs.py` with
`--before-bin-dir DIR --out build/benchmarks/native-header-graphs` and optional
`--repetitions N`. It builds source and saved IR with 512 leaf records and 32
consumers. A foreign signature retains the complete record ABI; C static
assertions check its size and final field offset. Every build runs two dynamic
input oracles, and the final generated C and executable bytes must match.
Dependency metadata must contain at least 512 inputs per consumer. Fresh and
unchanged samples share warm machine caches and identical source inputs.
This compares two compiler versions, without a Go or C language baseline.

`bench/native_cache_names.py --before-ref REVISION
--out build/benchmarks/native-cache-names` isolates ordered-name collection at
16, 256, 2,048 and 8,192 names. It compiles the earlier cache source from Git
through compiler standard input and compares it with the current source.
Each sample inserts unique names, repeats duplicates, checks order and an
independent checksum, then frees the collection. Input preparation and process
startup are outside the internal timer. Report this container timing
separately from whole builds; process RSS also includes benchmark inputs.
Both harnesses save raw samples, tool/source hashes and actual CPU affinity.
For a before/after comparison that reaches late record types within the former
512-module limit, add `--groups 4 --read-last` to the header-graph benchmark.
`--groups` and `--width` control the shared record graph; defaults retain the
original 512-leaf first-field workload. Static ABI assertions and runtime
oracles follow the selected dimensions and field.

`bench/reachable_types.py --before-lib DIR/libziran.a --out
build/benchmarks/reachable-types` measures two qualified lookups per round in
wide, deep and shared import graphs. Graph creation and process startup are
outside its internal CPU/wall timers. Cold samples invalidate entry-scope
answers; cached samples retain them. Every sample records correctness.
The previous implementation fails positive lookups beyond 512 modules, so
those rows are explicitly labeled as incorrect and excluded from equivalent
work comparisons. Peak process RSS includes graph construction and storage;
it does not isolate traversal scratch memory. Symbol verification and both
profilers are removed from timed environments.

`bench/native_probe.py --before-bin-dir DIR --out
build/benchmarks/native-probe` measures the five-object cache regression
fixture with stable messages, alternating within-line version interleaving,
and changed GMP versions. It reuses the regression's native compiler shim and
serves saved real diagnostics plus controlled version records. Real object
compilation and linking are timed, with runtime oracles and native byte
identity checks. The ordinary probe subprocess cost and real-world frequency
of interleaving are outside this experiment; pair it with ordinary build
controls before drawing a general compilation-speed conclusion.

Entry-scope symbol resolution profiles report `type_lookup_hit`,
`type_lookup_miss`, `function_lookup_hit` and `function_lookup_miss`.
Misses include uncached ambiguous or absent results; repeated results count
as hits until declarations or imports change. Set
`ZIRAN_VERIFY_SYMBOL_LOOKUPS=1` for correctness checks against fresh searches,
and leave it unset when measuring performance. Import-free scopes with at
most four procedures scan directly without cache or graph-visit storage.

## Paired checking and body eligibility

Preserve `ziran`, `zi2zir`, `zi2c` and `zi2zx` together before changing the
compiler, then run:

```sh
env -u DISPLAY -u WAYLAND_DISPLAY python3 bench/body_eligibility.py \
  --before-bin-dir build/body-eligibility/before \
  --out build/benchmarks/body-eligibility
```

This reuses `compiler_scaling.py`'s module and generic generators. Defaults
cover 8, 32 and 128 modules with 16 and 64 fields, plus local and imported
generic specializations. Both compiler versions must produce identical saved
IR, emitted C and bundles. Identical C is compiled once per source/saved form;
native and both portable runtimes must match independent arithmetic oracles.
Nine shuffled pairs measure whole checking, C emission and bundle construction
commands. Validation runs and a separately profiled warmup precede timings;
profiles retain body-expression and record-field counters without adding
profiling overhead to timing samples. CPU time and peak child RSS are retained.

Use `--project NAME ROOT ENTRY` for an offline package check with its existing
lock and local overrides. Keep both compiler variants under the overridden
Ziran root so the package launcher uses each variant's sibling tools. Project
sources and manifests are hashed and checked for changes. These workloads
measure compiler work on one shared host, not full native build times or
general Go/C performance parity.

## Repeated portable execution

Before changing the runtime, preserve `zi2zir`, `zi2zx` and `libziran.a` together
in a stable baseline directory. Then run:

```sh
env -u DISPLAY -u WAYLAND_DISPLAY python3 bench/vm_execution.py \
  --before-dir build/vm-execution/before --out build/benchmarks/vm-execution
```

This suite exercises integer arithmetic, mixed signed/unsigned comparisons,
floating-point arithmetic, record calls over an array, fixed-array returns,
population count, immutable string-byte scans, and the standard-library JSON parser. The array case passes
a 4,096-element value through two functions and checks untouched zero elements
as well as both ends.
The JSON case scans nested records, reads signed fractional numbers, and decodes
raw UTF-8 and escaped strings, including surrogate pairs. Its checksum comes
from independently decoded Python JSON values and their UTF-8 bytes.
The string case weights UTF-8 bytes at two traversal strides, with an
independent Python checksum and an empty input among the documents.
Each uses a separate Python result calculation. Source and saved IR
must create identical bundles, and both runtime versions must recreate the
source bundle before timing. Every first and repeated run must match the oracle.
The default uses ten thousand numeric iterations, one JSON corpus pass, and
nine repeated executions per instance, within the runner's million-statement
limit. Results separate
checked loading, instance opening, first execution, repeated execution and closing
through `vm_startup.py`; its measurement and memory limitations below apply.
These are synthetic VM comparisons on the recorded host, not native C/Go or
whole-application performance claims. Fixture manifests retain the generator
hashes and expected results alongside the existing raw timing receipts.

To calibrate these same workloads against generated native C and Go:

```sh
env -u DISPLAY -u WAYLAND_DISPLAY python3 bench/vm_native.py \
  --vm-results build/benchmarks/vm-execution \
  --out build/benchmarks/vm-native
```

The harness regenerates each workload and its independent oracle, requires
identical native output from source and saved IR, and verifies both forms.
C defines `ZIRAN_NO_ASM`; Go uses `CGO_ENABLED=0` and `GOMAXPROCS=1`, retaining
ordinary garbage collection. Both controls therefore execute the checked body.
Native timers cover a batch of 128 calls by default, including the call and
checksum loop; per-call medians are compared with the recorded VM steady
medians. Increase `--runs` when short batches quantize CPU usage to zero;
`--runs 4096` gives longer CPU samples for this suite. Every sample checks the
complete batch checksum. Child peak RSS is
measured separately from the driver process. Keep the raw samples, generated
sources, commands, build observations and source/tool/artifact hashes.

These are matched generated backends, rather than independent handwritten
implementations. Native batches and repeated VM instances differ in scheduling
and memory management. Their ratios expose remaining interpreter overhead on
the measured workloads; they do not establish whole-application C/Go parity.

## Android device execution

Prepare matched Ziran source/saved-IR C and Go programs, handwritten C/Go
controls, and a portable VM fixture for one Android ABI:

```sh
python3 bench/android_execution.py prepare \
  --out build/benchmarks/android-device/execution \
  --ndk "$ANDROID_NDK_HOME" --abi armeabi-v7a
```

Run the measurement stage on the host that owns the device's ADB connection,
passing its exact serial:

```sh
python3 bench/android_execution.py run \
  --out build/benchmarks/android-device/execution --serial DEVICE_SERIAL
```

The script verifies transferred hashes, checks every checksum against an
independent Python calculation, shuffles native trials, discards process
warmups, and records internal kernel time and peak process RSS. Go uses one
logical processor. The VM runs the small input and separates checked loading,
instance preparation and repeated execution. A fresh directory under
`/data/local/tmp` holds each attempt's files and is removed afterward; the
result file records failures and cleanup status and protects existing results.

These are single-workload device measurements. Clocks, temperature and
background activity remain uncontrolled, short CPU samples can round to zero,
and peak RSS includes startup/runtime memory. ARMv7 uses the checked body for
this fixture's x86_64/AArch64 assembly variants. Cross-build observations do
not establish Go-like compilation speed on Android.

To compare the same seven portable workloads across saved VM libraries:

```sh
python3 bench/vm_android.py prepare \
  --vm-results build/benchmarks/vm-execution \
  --out build/benchmarks/vm-json/android \
  --ndk "$ANDROID_NDK_HOME" --abi armeabi-v7a \
  --library before PATH/TO/BASELINE/libziran.a \
  --library after build/bundle-runtime/android-armeabi-v7a/libziran.a
python3 bench/vm_android.py run \
  --out build/benchmarks/vm-json/android --serial DEVICE_SERIAL --cpu 0
```

Libraries must match the selected ABI and current VM API. Add another
`--library LABEL PATH` to retain an earlier reference alongside a recent
baseline. Preparation verifies fixture sources and expected results against
the independent generator and records library, runner and bundle hashes.
The device stage runs every variant consecutively for each case/repetition,
with shuffled groups and rotated variant orders that balance positions across
repetitions. It discards process warmups.
It saves each sample immediately, refuses to overwrite results, checks
transferred files, and verifies removal of its own remote directory.
`--cpu` pins only the test child; omit it to retain normal scheduling.
Record affinity with the results: neither mode controls device frequency,
temperature or background activity, and a result on one core or ABI does not
establish gains on another.

## CPU-selected assembly

```sh
env -u DISPLAY -u WAYLAND_DISPLAY python3 bench/cpu_features.py \
  --out build/benchmarks/cpu-features
```

This dependent xorshift/popcount workload supplies x86_64 `popcnt` and AArch64
`simd` assembly loops with an optimized portable Ziran population count. The
harness compares CPU selection, forced Ziran fallback, handwritten C with CPU
selection, and handwritten C fallback. Each input is checked against a Python
integer/bit-count oracle, including negative and zero iteration counts. The
small case also runs in the VM. Source and saved IR must emit identical C and
bundles. The metadata records whether the host has the requested feature;
native runs can legitimately select their portable implementation.

Use `--assembly-form block` for a local block around the complete loop, or
`--assembly-form operation-block` for a local population-count block called
in every iteration. The default remains `function`. Use separate stable
outputs for each form. CPU-feature blocks pay selection and helper-call costs
at each block execution; comparing both granularities shows when those costs
outweigh a specialized instruction. A whole-loop block amortizes them.

The default large input is twenty million iterations to reduce startup's share
of runtime. Loop assembly and optimized C can have different scheduling, so
these numbers do not isolate instruction cost or establish a language ranking.
Results retain commands, source/tool/artifact hashes, warmups, shuffled runtime
order, CPU time, peak RSS and raw samples. Timings include process startup and
the GNU/BusyBox time launcher when available. They measure one host with warm
caches and no CPU isolation; cross-compilation is not hardware performance
evidence. Reuse the stable output directory after preserving required receipts
and removing inactive outputs.

## Native Go assembly bridges

```sh
env -u DISPLAY -u WAYLAND_DISPLAY python3 bench/go_assembly.py \
  --before-bin-dir build/go-assembly/before/bin \
  --out build/go-assembly/benchmark
```

Preserve the prior compiler binaries before rebuilding the modified emitter.
This harness measures the same dependent xorshift/popcount workload as
`cpu_features.py`, using function and block assembly around the whole loop,
then a local assembly block per operation. It compares the prior emitter,
current default Go, current `CGO_ENABLED=0` Go, native cgo with
`-tags=ziran_asm`, and generated native/forced-portable C. Source and saved IR
must produce identical Go companions. A runtime field records whether the Go
native dispatcher was installed; every result is checked with the independent
integer oracle.

The native bridge is opt-in because per-call cgo overhead can outweigh the
instruction savings. Compare all three placements before choosing a native
build. Kernel timers exclude startup; CPU time covers the kernel and peak
child RSS includes the runtime. Keep raw shuffled samples and warmups,
commands, tool/artifact/source hashes and individual build observations.
These warm-cache, shared-host synthetic measurements do not establish whole
application speed, Go compilation parity or universal memory improvements.
Reuse the stable output only after preserving needed receipts and confirming
that its binaries and build outputs are inactive.

## Bundle startup and real-package checks

Host embedding has a separate capability-scaling benchmark:

```sh
env -u DISPLAY -u WAYLAND_DISPLAY python3 bench/host_capabilities.py \
  --before-dir build/host-capabilities/before --out build/benchmarks/host-capabilities
```

Preserve `zi2zx` and `libziran.a` before changing the runtime. The default uses
1, 16, 64 and 256 host capabilities, nine paired process samples and nine fresh
instances after discarded process/instance warmups. Each instance executes 32
rounds of every capability. Callbacks check their names, contexts, arguments and
order; results must match a separate Python integer recurrence. Both compilers
must generate identical bundle bytes. Receipts separate checked load, capability
enumeration, binding/initialization, execution and close CPU/wall times. Optional
glibc allocator in-use deltas are affected by caching and are not total live
memory or RSS. Results cover these synthetic fixtures on one shared host.

For the public embedding API, compare one checked `BundleOpen` followed by
independent `BundleInstantiate`/run/close operations:

```sh
env -u DISPLAY -u WAYLAND_DISPLAY python3 bench/bundle_instances.py \
  --before-dir build/bundle-runtime/before --out build/benchmarks/bundle-instances
```

The preserved directory contains `zi2zx` and `libziran.a`. Scalar, 256-member
enum and 64-module fixtures use independent Python integer oracles. Both
compilers must produce identical bundle bytes. Nine interleaved process samples
per version follow a process warmup; each process measures 49 fresh instances
after an instance warmup. The receipt separates load, instantiation, execution
and close CPU/wall times, and first-instance costs. Small phases include clock
overhead; whole-process wall time includes startup. These synthetic results on
one shared host do not establish general execution speed or memory savings.
The raw-IR runner below always verifies a caller-owned program on instance open.

`bench/enum_values.py` generates enum-heavy fixtures with independent integer
sum oracles, verifies source/saved bundle identity, and runs the comparison
below. For example:

```sh
env -u DISPLAY -u WAYLAND_DISPLAY python3 bench/enum_values.py \
  --before-dir build/compiler-check/before --out build/benchmarks/enum-values
```

Use `--project NAME ROOT ENTRY` to include real package checks. Compiler profiles
count `enum_value_parse` and `enum_values_reuse`. The runtime reuses immutable
evaluated bodies keyed by body text and backing type, including invalid bodies;
each lookup still requires the complete enum to be valid. Snapshots live until
process exit and add retained metadata, so fewer evaluations do not by themselves
prove a memory reduction. Reuse the stable output after preserving receipts
and checking that old outputs are inactive.

`bench/vm_startup.py` compares two preserved compiler/runtime builds. Before
changing the compiler, keep its `zi2zir`, `zi2zx`, optional `ziran` launcher,
and `libziran.a` in a stable baseline directory. Do not replace that baseline
while a comparison is running. The current tools default to `build/bin`, with
the current library in `build/libziran.a`.

For example, using fixtures generated by `compiler_scaling.py` with its default
16 fields:

```sh
env -u DISPLAY -u WAYLAND_DISPLAY python3 bench/vm_startup.py \
  --before-dir build/vm-performance/before \
  --out build/vm-performance/comparison \
  --case modules128 \
    build/benchmarks/compiler-scaling/modules-128/source/program.zi \
    build/benchmarks/compiler-scaling/modules-128/source/program.zx 130048
```

Each `--case` names a source file, its bundle, and an independently derived
integer result. Both bundlers must recreate exactly the supplied bundle before
measurement. Every first and repeated VM execution must return that result.
The C runner measures checked bundle loading, instance opening, first execution,
repeated execution in the same instance, and closing separately. Loading retains
structural, canonical, semantic, and law validation; opening retains VM
verification and global initialization. Execution has a million-statement
limit. Cases must return the same expected result across repeated instance runs.

Add `--project NAME /absolute/project/root relative/entry.zi` to compare complete
package checks through each build's sibling `ziran` launcher. These checks use
`--project --offline`, preserve the project's lock and local overrides, and
record compiler profiles separately from timing samples. The project sources
and revision must remain stable during measurement. Package cases can also run
without VM cases. They measure checking, not complete native application builds.

Results include source/tool/library/runner hashes, commands and working
directories, warmups, shuffled before/after order, raw timings, CPU time,
peak RSS, and live VM value bytes. The latter excludes IR and metadata caches;
peak RSS is process memory, not live allocated bytes. GNU/BusyBox time avoids
attributing Python's inherited peak memory to the measured process when
available. Whole-process timing includes that launcher. These are warm-cache,
single-host comparisons without CPU isolation; they do not establish cold-build
speed, a language ranking, or performance on other hardware. Preserve needed
receipts and remove inactive outputs before reusing the stable output path.

## Assembly and portable fallback

```sh
env -u DISPLAY -u WAYLAND_DISPLAY python3 bench/assembly.py \
  --out build/benchmarks/assembly
```

The byte-swap recurrence compares source and saved IR with native assembly,
forced Ziran fallback, handwritten C and Go, and the `.zx` fallback. Every
sample is checked against a separate Python oracle; emitted source and bundles
must agree between source and saved IR. On x86_64 the harness also checks for
a `bswap` instruction in the native binary. The fixture includes an AArch64
variant, but measurements prove only the recorded host's behavior.

Raw samples include wall time, user/system CPU time, process peak RSS, discarded
warmups, commands, and output. Metadata contains tool versions, compiler and
fixture hashes, and limitations. When GNU time or BusyBox time is available, a small launcher
avoids counting Python's inherited high-water mark as the native program's
memory. Metadata identifies the fallback to raw `wait4` otherwise. RSS is not
live allocated bytes, and wall/CPU times include that launcher. Runtime order is shuffled. Go build
timings use a warm cache after the warmup. Compile phases are separate; these
are not end-to-end build measurements or a language ranking. Reuse the stable
output path after preserving needed receipts and removing inactive outputs.

For compiler scaling, module graphs, generic specialization, and source/saved
IR validation, run:

```sh
env -u DISPLAY -u WAYLAND_DISPLAY python3 bench/compiler_scaling.py \
  --out build/benchmarks/compiler-scaling --sizes 8 32 128
```

The harness validates every C and portable result against an independent
checksum and records checking, saved IR, C emission, native compilation, and
execution separately. `--project /absolute/path/to/entry.zi` adds a real
package check using its package map. Results include source and binary hashes,
raw samples, a discarded warmup, tool versions, cache caveats, and compiler
profiles. Choose a new output directory for every run.

Set `ZIRAN_PROFILE=/absolute/path/to/profile.jsonl` for any compiler command to
append a process profile without changing ordinary output or JSON diagnostics.
Profiles contain inclusive parse/load/check/emission/IR-write times, record
field parsing and snapshot-reuse counts, peak RSS, and allocation-request counts for compiler
workspace allocations through `AllocateOrExit`. These allocations are a
subset, not total allocations or currently live bytes. Nested phase times
overlap and must not be summed into a claimed build duration; the harness
measures whole commands separately.

Profiles also count `kept_text_pointer`, `kept_text_compare` and
`kept_text_lookup`. A small per-thread cache recognizes canonical immutable
pointers directly. Other input addresses are hints: their current bytes,
including the terminator, must match the immutable saved copy before reuse.
The cache has 128 entries (3 KiB per thread on 64-bit hosts). Saved bundles and generated source retain
the same contents. Use `tests/kept_text.sh` to check mutable buffers, substring
identity, truncation, table growth and concurrent reuse.

The October 3 field-snapshot comparison used module/generic sizes 8 and 32,
three measured repetitions, and checks of the real Kryon and Terminal packages
(162 validated measured samples per run). Kryon's field-parser calls fell
from 2,056,209 to 2,883 and Terminal's from 2,747,749 to 3,839. Whole-command check
medians were 515/537 ms for Kryon and 813/849 ms for Terminal (before/after), so this
run establishes elimination of repeated parsing, not a wall-clock speedup.
The machine was shared and both metadata records identify dirty working trees;
each records the actual tool hashes. Local raw samples are under
`build/benchmarks/compiler-fields-before-fixed` and
`build/benchmarks/compiler-fields-after`. Repeat on a stable machine before
using these timings as a regression threshold.

## Cross-target smoke benchmark

From the repository root, build the current toolchain and run:

```sh
env -u DISPLAY -u WAYLAND_DISPLAY make -j4 all
env -u DISPLAY -u WAYLAND_DISPLAY python3 bench/smoke.py
```

The script creates a new directory under `build/benchmarks/` with `metadata.json`,
`samples.jsonl`, `summary.json`, and `benchmark.md`. Pass `--out PATH` to select
a new output directory. `--compile-repetitions` and `--run-repetitions` adjust
the measured sample counts; each case also has one discarded warmup. The run
requires GCC, G++, Go and the Ziran tools. Rust, Java and Node comparisons are
included when their toolchains are present, and absences are recorded.

One integer recurrence is built from source and saved IR to C, C++, Go, and
portable `.zx`. The harness checks that generated source and bundles match,
and validates every runtime result against an independent modular-series
oracle before reporting times. It separately measures Ziran check/emission,
downstream native compilation, and whole-process execution. Runtime cases are
shuffled with a fixed seed between sample rounds. The portable VM runs only
the small input because its instruction budget excludes the large one.

These numbers are a smoke baseline, not a language ranking. They include
process startup; Go build measurements use a warm build cache and are labeled
accordingly. JIT languages are launched fresh rather than timed at warmed
steady state. The output records compiler revisions, source and tool hashes,
tool versions, commands, fixture hashes, raw timings, and limitations so that
a comparison can be checked against the exact experiment. A broader suite
still needs generics, larger module graphs, more compiler-scaling cases, and
real applications.

The first repeated baseline, including raw samples and machine metadata, is
in [`results/2026-09-27`](results/2026-09-27/benchmark.md). It used three
measured compile samples and five measured runtime samples per case on one
machine; its medians are not regression thresholds.

For a second workload that grows a `Vec(s32)` and then scans it, run:

```sh
env -u DISPLAY -u WAYLAND_DISPLAY python3 bench/vector_growth.py
```

This measures source and saved-IR checks and emissions to C, C++, Go, and
portable `.zx`, downstream native builds, and whole-process execution.
Handwritten C, C++, Go, Rust, Java, JavaScript, and Python versions provide
algorithm-level comparisons when their toolchains are installed; unavailable
tools are recorded explicitly. The checksum uses a closed-form arithmetic
oracle, and every runtime sample must match it. The portable VM runs the small
input only. Raw commands, samples,
source and tool hashes, and caveats are written alongside the report. These
workloads remain a smoke and collection baseline; they do not cover text,
records, generics, larger module graphs, or real applications.

The [vector-growth baseline](results/2026-09-27-vector-growth/benchmark.md)
contains three measured compilation and five measured execution samples per
case on one machine. The generated C and C++ runs were close to their
handwritten comparisons for the large input; generated Go took about 22 ms
versus 8 ms for handwritten Go. This identifies a workload for investigating
vector lowering, not a general performance ranking.

For a record-value and call workload over a fixed `[32]Point` table, run:

```sh
env -u DISPLAY -u WAYLAND_DISPLAY python3 bench/record_calls.py
```

Each round updates all records through a checked `Mix(Point, Point, s32, s32)`
call and accumulates a signed 64-bit checksum with wrapping `s32` field
arithmetic. The harness measures source and saved IR through C, C++, Go, Rust when Cargo is
installed, and portable `.zx`, downstream native builds, and whole-process execution.
Handwritten C, C++, Go, Rust, Java, JavaScript, and Python versions participate
when installed. Every sample must match an independent Python oracle; source
and saved-IR generated output and bundles are checked for equality. The VM runs
the small input only because the large input exceeds its instruction budget.
The [clean-head generated-Rust record-call baseline](results/2026-09-28-record-calls-rust/README.md)
contains 238 validated samples across 58 phase/case combinations. The
[earlier baseline](results/2026-09-28-record-calls/README.md) contains 206
samples across 50 combinations and predates generated-Rust participation.

For a focused before/after comparison of two validated Go binaries, run
`bench/vector_growth_ab.py --before PATH --after PATH --out NEW.json`. It
shuffles execution order, checks every checksum, and records raw samples and
binary hashes. A [paired fast-path result](results/2026-09-27-vector-growth-go-fastpath/README.md)
shows how the general arithmetic helper affected the vector workload.

A [provisional multilanguage matrix](results/2026-09-27-vector-growth-multilang/README.md)
adds Rust, Java, JavaScript, and Python comparisons after the Go arithmetic
fast path. Its metadata records the dirty working tree used for that run.

For a UTF-8 scanning workload over `aé中🙂`, run:

```sh
env -u DISPLAY -u WAYLAND_DISPLAY python3 bench/text_scan.py
```

The text has 10 bytes and 4 Unicode scalar values; every runtime sample must
return `148921 * rounds`. The harness compares Ziran source and saved IR to
C, C++, Go, Plan 9 C, and `.zx`, plus handwritten C, C++, Go, Rust, Java,
JavaScript, and Python when installed. It validates generated-source and
bundle equality, compares canonical Plan 9 dispatcher output with direct
`zi2c --target=plan9-c`, and records unavailable tools rather than estimating
them. When no Plan 9 compiler is installed, the harness still compiles the
emitted Plan 9 dialect with host GCC against a minimal fake Plan 9 libc and
marks those runtime samples as dialect validation rather than Plan 9 hardware
performance. The
[committed-head result](results/2026-09-27-text-scan-multilang/README.md)
contains 206 validated samples across 50 phase/case combinations and predates
generated-Rust participation.
