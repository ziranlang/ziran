# C teacher: physical Alpha checkpoint, 2026-11-16

Compared the baseline [compiler revision](https://github.com/ziranlang/ziran/commit/96c23c7de6bc622225a916dfce18bff04486ba2b) with the [verified learning feature](https://github.com/ziranlang/ziran/commit/36e6caf9643d82b58434f11aad507da79f895922) on physical Alpha, Intel i7-8700. This is a fixed, pushed compiler comparison; later SSA, rotation, vectorizer and returned-record changes are outside this checkpoint. Learning recording and runtime checks were off. All builds and measurements ran serially through Alpha build-queue; every timing has eleven raw samples and symmetric inclusive-quartile 1.5×IQR exclusions.

The [before](c-teacher-alpha-2026-11-16-before.md) and [after](c-teacher-alpha-2026-11-16-after.md) assembly reports cover all six kernels, Button, Text, Input and hello with GCC 15.2 and Clang 21.1.8 at `-O2`. The after report has 1,506 ranked function comparisons and zero failed workloads. Native map extents identify functions, backward branches identify static loop envelopes, and SIGPROF supplies estimated function shares. Counts and weighted excess are inspection priorities, not cycle estimates or speedup predictions.

| Ranked finding | Native / C instructions | Native stack reads / writes | Function SIGPROF share | Owner |
| --- | ---: | ---: | ---: | --- |
| Records / Clang | 382 / 73 | 4 / 260 | 162 / 162 | records and aggregate lane |
| Sieve / Clang | 77 / 36 | 2 / 2 | 163 / 163 | SSA, rotation and loop lane |
| Hash / GCC | 50 / 26 | 0 / 0 | 158 / 158 | SSA and loop lane |
| Printing / GCC | 59 / 32 | 5 / 5 | 35 / 99 | call and memory selection candidates |

Vectors illustrates the limits of instruction counts: native has 104 instructions versus GCC’s 109, yet runs slower. Native has eleven stack reads and eleven writes versus seven and eight; GCC uses vector registers in its reduction. Stack traffic includes locals and saved registers, and vector-register uses include scalar floating point. Back-edge envelopes include cold paths; loop constants and missing C symbols do not prove LICM or inlining opportunities. Widget startup samples include session/theme setup and do not replace warm scenario timings. Zero samples mean unknown share. Existing detailed path findings were routed to the records, loop and vectorizer owners; this checkpoint adds the controlled Alpha measurements.

Fresh kernel medians, microseconds per call:

| Kernel | Native before | Native after | C after | Native/C after |
| --- | ---: | ---: | ---: | ---: |
| Sieve | 64.481 | 64.040 | 34.923 | 1.83 |
| Records | 47.748 | 47.181 | 15.743 | 3.00 |
| Hash | 40.322 | 40.406 | 27.961 | 1.45 |
| Vectors | 62.574 | 62.221 | 32.703 | 1.90 |
| Printing | 11.292 | 13.047 | 9.352 | 1.40 |

The [fresh baseline](c-teacher-alpha-before.json) and [fresh after receipt](c-teacher-alpha-after.json) use the standard lane’s current five-kernel harness. Sieve and Hash emit identical native artifacts before and after, so their small timing differences are noise observations. Records, Vectors and Printing artifacts change. The small Records/Vectors timing changes do not establish a useful speedup; Printing regresses from 11.292 to 13.047 µs and remains open. Native timings amortize one process over 32 calls; C, engine and Wasm time warm in-process calls. Compilation is excluded.

The original request also includes Calls. The standard lane removed it from its current timing list because Fib(20) is constant folded. The [six-kernel baseline](c-teacher-alpha-six-before.json) preserves the earlier physical-Alpha receipt at the same baseline revision (19:35 UTC), and the [six-kernel after control](c-teacher-alpha-six-after.json) restores Calls for a separate 21:18 UTC run. All five fresh baseline native digests match that older receipt; Calls itself has exactly the same native artifact before and after. Its native median is 6.129 → 5.954 µs (C after 11.450 µs). This is a historical control of a folded result and wrapper/startup overhead, not a fresh paired measurement of recursion. The [control receipt](c-teacher-alpha-2026-11-16-calls-validation.json) records the temporary list change, unchanged tools and exact harness restoration.

Fresh widget medians, microseconds per scenario:

| Widget | Native before | Native after | Improvement | C after | Go after |
| --- | ---: | ---: | ---: | ---: | ---: |
| Button | 8.807 | 6.969 | 20.9% | 2.726 | 4.162 |
| Text | 9.716 | 8.557 | 11.9% | 3.129 | 3.735 |
| Input | 12.882 | 9.963 | 22.7% | 3.924 | 6.610 |

The [before](c-teacher-alpha-2026-11-16-widgets-before.json) and [after](c-teacher-alpha-2026-11-16-widgets-after.json) widget receipts use the same Kryon revision and manifest. They exercise public layout, interaction and paint-command generation with checked batches, excluding pixel rasterization, system fonts and OS I/O. These improvements cover the integrated compiler revisions, including the constructor lane; they are not isolated gains from the learned scalar idioms. The gap to C and Go remains open.

Provenance required a correction: Alpha’s Kryon manifest pins Ziran v0.5, and `--project` selected that cached compiler even when invoked through the current launcher. The site harness’s compiler metadata alone did not establish the selected compiler. Earlier attempts failed safely and restored source/tools. Both accepted stages used an ignored temporary local override:

```toml
[overrides]
ziran = "/home/wao/Projects/ziranlang/ziran"
```

Before each accepted stage, the absolute current `build/bin/ziran pkg path ziran` command from Kryon’s directory resolved to that canonical compiler repository. Generated widget IR was archived and regenerated. The local override and lock bytes were restored afterward. Omega already had the canonical local override, so this discovery does not invalidate its previous widget checkpoint. The parent coordinator and widget lane received the compiler-selection metadata issue for routing a general harness fix.

The [validation receipt](c-teacher-alpha-2026-11-16-validation.json) records eight tool/runtime hashes, aggregate digests over 1,811 baseline and 1,819 after source files (compiler, harness and Kryon inputs), stable hashes during each stage, and exact preservation of `bench/standard` plus `build/standard`. All 336 tracked compiler/stdlib inputs were verified against the pushed target tree. The receipt retains the failed incremental target build and the successful full rebuild recovery; restored archive timestamps had left generated engine files stale. Both accepted compiler epochs use a full forced rebuild.

Exact accepted build/measurement commands, inside Alpha `build-queue`, with `ZIRAN_LEARN=off`, `HARMONY_BUILD_LOCAL=1`, and display variables removed:

```sh
make -B -j2 all
build/bin/ziran run --target=c bench/bench.zi -- wasm-tier --samples 11 --label c-teacher-alpha-before --output bench/results/c-teacher-alpha-before.json
build/bin/ziran run --target=c bench/bench.zi -- library-targets --project /home/wao/Projects/kryonlabs/kryon --manifest /home/wao/Projects/kryonlabs/kryon/tests/widget_benchmarks.json --samples 11
build/bin/ziran run --target=c bench/bench.zi -- c-teacher --output bench/results/c-teacher-alpha-2026-11-16-before.md
make -B -j2 all
build/bin/ziran run --target=c bench/bench.zi -- wasm-tier --samples 11 --label c-teacher-alpha-after --output bench/results/c-teacher-alpha-after.json
build/bin/ziran run --target=c bench/bench.zi -- library-targets --project /home/wao/Projects/kryonlabs/kryon --manifest /home/wao/Projects/kryonlabs/kryon/tests/widget_benchmarks.json --samples 11
build/bin/ziran run --target=c bench/bench.zi -- c-teacher --output bench/results/c-teacher-alpha-2026-11-16-after.md
build/bin/ziran run --target=c bench/bench.zi -- wasm-tier --samples 11 --label c-teacher-alpha-calls-control --output bench/results/c-teacher-alpha-six-after.json
```

All accepted commands returned zero. The completed feature’s Ziran differential tests compare 70,210 C/native output lines across 34 multiply factors, deterministic edge/random inputs and eight integer widths, followed by retained parity. Offline training twice produced byte-identical checked-in table source. Full retained local tests and bootstrap provenance, including concurrent peer changes and the later successful focused site reruns, remain in the [Omega feature summary](c-teacher-omega-2026-11-16-summary.md). No compiler source changed for this report milestone. CI has not been checked for this report. Alpha build tools were updated for measurement; this does not claim an owner launcher installation or a release.
