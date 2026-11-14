# Current execution feature matrix

Status: current-tree baseline for [step 01](01-contract-and-completion-matrix.md).
`Working` means the implementation and retained source/saved-IR fixture exist;
it is not a claim that this documentation task reran the fixture. `Gap` means
the direct assembly target lacks required work and must reject it explicitly.
`Excluded` is a deliberate target-contract exclusion. `Unknown` means no fresh
execution evidence for this cell is present. Current fallback or downgrade
behavior is recorded only as baseline evidence and is a Gap for the new target;
it never counts as support under the [gap policy](gap-policy.md). `Proposed`
belongs to the ten-step ASSEMBLY plan, not today's implementation.

Primary evidence is [implementation status](../docs/IMPLEMENTATION_STATUS.md), [capability output](../docs/CAPABILITIES.md), [ZX behavior](../docs/ZX.md), the stable feature registry in [features](../docs/FEATURES.json), [capability reporting source](../cmd/package_capabilities.zi), and retained tests. Cross-compilation or presence of a script never counts as target execution evidence.

## Execution modes

The opt-in [direct scalar prototype](README.md) is separate from these existing
engines. It emits Linux x86-64 ELF from checked source/saved IR, but does not
replace ZX or establish portability/performance completion.

| Mode | Current status | Evidence | C | Go | Portable ZX | Native ZX | Wasm | OS boundary |
|---|---|---|---|---|---|---|---|---|
| Generated C output | Working, source and saved IR | [implementation status](../docs/IMPLEMENTATION_STATUS.md), [C target](../tests/c99_target.sh) | emits C | n/a | n/a | lowers to same C backend | can compile selected C to wasm32 | Linux current host; Windows/macOS/Android toolchains have separate fixtures |
| Generated Go output | Working, secondary tier | [capabilities](../docs/CAPABILITIES.md), [call order](../tests/call_order.sh) | n/a | emits Go | n/a | n/a | n/a | Verifiable only where a compatible Go runtime/toolchain executes it |
| Portable instruction engine | Working; runs every `.zx` and `zi2asm --run` | [ZX contract](../docs/ZX.md), [engine](../cmd/compiler_exec.zi) | n/a | n/a | runs every bundle | n/a | separate target | Host capabilities and platform console services only |
| Wasm execution tier | Working restricted subset; provider rejection is a hard `zx.wasm` error and never starts another engine | [Wasm tier](../docs/ZX.md) | emitter | n/a | explicit separate engine only | budget model shared | wasm32 | Scalars, owned strings, fixed arrays, records, typed `s64 -> s64` hosts; other constructs must fail direct-Wasm preflight |

## Portable language behavior

| Feature | Current status and target behavior | Evidence |
|---|---|---|
| Fixed-width integers | Working on C, Go, ZX source/saved IR: declared-width wrap, signed-min division/remainder, shifts, casts, bitwise and mixed-width behavior. Wasm uses the same scalar tier inside its subset; unknown outside available Wasm fixtures. | [numeric semantics](../tests/numeric_semantics.sh), [width matrix](../tests/numeric_width_matrix.sh), [manifest](../tests/numeric_conformance.json) |
| Float32/float64 | Working arithmetic, compare, infinity/NaN and range-checked conversion on native C/Go/ZX. lowered IR must not preserve that gap. | [numeric manifest](../tests/numeric_conformance.json), [status](../docs/IMPLEMENTATION_STATUS.md) |
| Implicit widening | Working for the checked feature contract on C, Go, ZX. | [feature registry](../docs/FEATURES.json), [features test](../tests/features.sh) |
| SIMD vectors | Working with identical lanes across native C tiers, Go lane loops, bundles and source/saved IR. C wasm32 may use SIMD128; bundles run vectors lane by lane. CPU feature selection is target-specific. | [status](../docs/IMPLEMENTATION_STATUS.md), [SIMD target](../tests/simd_target.sh) |
| Scalars, strings, enums, plain records | Working in portable subset, including literals, copies, fields, scalar compound assignment, parameters and returns. Pointer-sized `isize`/`usize` and functions/statements outside the verifier subset are rejected. | [ZX subset](../docs/ZX.md), [verifier](../cmd/compiler_lowered_verify.zi) |
| Fixed arrays, slices, vector indexing | Working with checked bounds. Generated C may omit only proven bounds checks; portable host slices copy successful mutations back and reject overlapping mutable arguments. | [proven ranges](../tests/proven_index_ranges.sh), [host slices](../tests/host_slices.sh) |
| Compile-time assertions, `#run`, laws | Working as checked language features on all advertised targets; unsupported proof/effect forms or exhausted budgets are rejected/unknown rather than guessed. | [feature registry](../docs/FEATURES.json), [compile run](../tests/compile_run.sh), [laws](../tests/laws.sh) |
| Procedure defaults and procedure values | Working for advertised examples on C, Go, ZX; portable verifier checks slot signatures and rejects C callback procedure types. | [feature registry](../docs/FEATURES.json), [defaults](../tests/default_arguments.sh) |
| Print and I/O | Working literal-format print on C, Go, ZX. Rich OS I/O is a library/capability contract, not a portable opcode. | [feature registry](../docs/FEATURES.json), [print](../tests/print.sh) |
| Named imports and linking | Working; bundles follow direct calls, retain required startup/types/laws, and reject unresolved imports or unreachable unsupported code. | [ZX linker](../docs/ZX.md), [bundle verification](../tests/bundle_verification.sh) |

## Memory, ownership, and cleanup

| Feature | C | Go | Portable ZX | Wasm | Evidence |
|---|---|---|---|---|---|
| Zeroed heap allocation/free | Working | Working | Working; rejected use after free/double free | Gap outside documented Wasm subset; direct Wasm rejects | [heap allocation](../tests/heap_new.sh), [Wasm tier](../docs/ZX.md) |
| Owned `Vec` operations | Working | Working, physical OOM may terminate | Working, allocation failures recoverable where declared | Gap outside Wasm subset; direct Wasm rejects | [owned values](../docs/OWNED_VALUES.md), [vector test](../tests/vec.sh) |
| Direct/aggregate vector drops | Working on block, return, break, continue | Working | Working | Gap; direct Wasm rejects until implemented | [aggregate safety](../tests/aggregate_vec_safety.sh) |
| Vector field/member moves | Working with listed source restrictions | Working | Working | Gap; direct Wasm rejects until implemented | [aggregate safety](../tests/aggregate_vec_safety.sh), [move expression](../tests/vec_move_expression.sh) |
| Text and `TextView` | Borrowed bytes observable | Snapshot | Snapshot | Owned strings in Wasm subset; other text forms are a Gap and reject | [capabilities](../docs/CAPABILITIES.md), [text view mutation](../tests/text_view_mutation.sh) |
| Native pointers | Native lifetime/alias responsibility | Native Go pointer rules | Opaque host handles in bundle ABI | Linear-memory boundary/provider values | [ZX host ABI](../docs/ZX.md), [capabilities](../docs/CAPABILITIES.md) |
| Instance global state | Native instance data | Go program state | Independent mutable slots per instance | Independent bindings/linear memory per instance | [component state](../docs/ZX.md), [component engine test](../tests/bundle_component_test.c) |

## Calls, concurrency, fuel, and traps

| Feature | Current behavior | Evidence |
|---|---|---|
| Argument and compound order | Working: left-to-right call arguments; destination read before compound right side on C, Go, ZX. | [call order](../tests/call_order.sh), [compound order](../tests/compound_order.sh) |
| Bounded execution | Working by explicit host selection. The engine and Wasm count calls and loop iterations; cumulative limited open includes initialization and every run. | [ZX limits](../docs/ZX.md), [step limit](../tests/bundle_step_limit.sh) |
| CPU parallel regions | Current baseline: C threads on supported hosts; Go/ZX serial downgrade and reverse-region serialization are assembly-target Gaps. The direct target rejects them until their promised parallel implementation exists. | [parallel test](../tests/parallel.sh), [native parallel](../tests/zx_native_parallel.sh) |
| GPU regions | Current baseline has CPU fallback only. The assembly target reports a Gap and rejects GPU selection until a device implementation exists. | [capabilities](../docs/CAPABILITIES.md), [assembly blocks](../tests/assembly_blocks.sh) |
| Runtime traps | Working failure boundary: bounds, unreachable, host/fuel/stack and numeric contract failures stop the run. | [ZX contract](../docs/ZX.md), [engine](../cmd/compiler_exec.zi) |
| Host capabilities | Working checked module/function ABI with provider preflight, bindings, mutable slice copyback and no overlap. | [host capabilities](../tests/host_capability.sh), [ZX host ABI](../docs/ZX.md) |
| Component replacement | Working transactional ABI/state migration/rollback for portable components and supported Wasm memory interface. | [component contract](../docs/ZX.md), [web lifecycle](../tests/zx_web_lifecycle.sh) |

## Native extensions and foreign runtimes

| Feature | Working behavior and restriction | Evidence |
|---|---|---|
| GNU assembly variants | Current C/Go behavior may use a checked portable body. For the direct assembly target, a missing matching target/feature implementation is a Gap and rejects at preflight; no portable body or cgo bridge satisfies the cell. | [assembly](../tests/assembly.sh), [ZX assembly](../tests/zx_native_asm.sh), [Go assembly](../tests/go_assembly.sh) |
| C callbacks / `#c_call` | Native C ABI only; Go and ZX target preflight reject it before output. | [capabilities](../docs/CAPABILITIES.md), [compiler inventory](../tests/compiler_inventory_cases.zi) |
| Go foreign types/maps | Go target extension only; opaque identity and scalar-union checks reject unsupported portable lowering. Renaming does not create a portable layout. | [compiler IR](../cmd/compiler_ir.zi), [capabilities](../docs/CAPABILITIES.md) |
| OS libraries | Explicit foreign/library dependencies remain allowed; native platform service portability is defined per OS row, not by opcode presence. | [bundle libraries](../tests/bundle_libraries.sh), [architecture](../docs/ARCHITECTURE.md) |
| Runtime assembly/C cache | Current native bundle cache is keyed by bundle/libraries/CPU/compiler sources; cached code can bypass program recheck only under documented boundary. | [native execution](../docs/ZX.md) |

## Maintained platform rows

| Platform / target row | Current verified surface | Target-specific restriction | Unknown verification |
|---|---|---|---|
| Linux x86-64, C, ZX and direct ELF | Current development host; source/saved-IR C, Go, ZX, direct `zi2asm` ELF, threads, SIMD tiers, OS libraries and retained tests. | Native C parallel threads; SIMD ISA dispatch; Linux process/thread services. | A complete matrix needs fresh named toolchain/hardware evidence per future gate. |
| Linux/Android ARMv7 | Portable ZX and generated C/Go; C SIMD fixtures cover NEON, VFP-only and scalar lanes; Android API 21 streams and optional four-ABI link paths. | 32-bit ARM ABI/CPU feature restrictions; optional NDK/QEMU/phone tooling. | Specific Android OS version/device combinations and non-SIMD execution are not established by this documentation pass. |
| Linux/Android AArch64 | Portable ZX and generated C/Go; NEON codegen and QEMU execution documented. | AArch64 hardware-capability assembly selection; Android toolchain. | Fresh physical-platform coverage beyond documented fixtures is unknown. |
| Linux/Android other CPU | Portable ZX and generated C/Go are the supported route. | No direct native row. | New architecture execution, including RISC-V, is proposed/unknown. |
| Windows generated C/Go | Compiler/runtime platform code and Windows fixtures exist. | No direct native row. | No fresh Windows host execution in this documentation task. |
| Windows portable ZX | The engine has no Linux-only service assumptions. | No native tier. | Fresh Windows host evidence unknown. |
| macOS generated C/Go and portable ZX | Darwin build definitions and package/file fixtures exist. | Darwin SHA/random system libraries and ABI differ. | No fresh macOS execution in this documentation task. |
| Browser/WebAssembly tier | Bundles lowered to wasm32 with per-instance memory, state, budget and lifecycle, tested in headless Chromium. | Subset listed above; unsupported constructs and provider rejection are direct-Wasm gaps. | Other browsers/versions and non-Chromium Wasm hosts are unknown. |
| Wasm32 standalone tools | `zi2asm --emit wasm` lowers verified ZX; the bundle Wasm tier runs it under an Emscripten `libziran`; optional Emscripten/wasmtime probes for SIMD and file-tree fixtures. | Requires installed Wasm CC/runtime; skipped when probes absent. | Current machine's optional tool availability is not execution evidence for target hosts. |
| Go output on OS | Generated source follows Go runtime. | Serial parallel regions, snapshots, foreign runtime restrictions. | Exact OS/Go version combinations not covered by a fresh matrix run. |

## Retained check surface

The retained suite is physically pruned. `make check` builds the retained tool and runs every discovered `.sh` check unless filtered; `make check-bootstrap` compares checked-in generated compiler sources; both run with desktop display variables removed. This is the suite that must accompany later implementation work. See [Makefile](../Makefile) and [check planning](../tests/check_plan.zi). Hardware/tool-dependent scripts may still skip when their required external probe is absent; a skip is not target execution evidence.

## Unknown and proposed work

**Unknown now:** fresh Windows/macOS/Android/browser execution for this exact revision; Wasm constructs outside the documented subset; Go behavior on every OS/runtime pair; pointer aliases and foreign effects lacking checked summaries; all future-backend target cells until a named fixture runs. Optional-tool skips, cross-compilation, child receipts, and stored benchmark reports remain non-evidence.

**Required step-01 follow-up:** lower the contract into typed basic blocks and verifier records; define portable layouts and effects; encode cleanup/fuel regions explicitly; separate target extension records from portable opcodes; then choose an incompatible `.zx` version against the actual writer in step 05. Feature completion requires fresh source/saved-IR execution evidence for every required matrix cell, with legitimate target exclusions named rather than silently skipped.
