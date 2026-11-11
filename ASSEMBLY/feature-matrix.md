# Current execution feature matrix

Status: current-tree baseline for [step 01](01-contract-and-completion-matrix.md). `Working` means the implementation and retained source/saved-IR fixture exist; it is not a claim that this documentation task reran the fixture. `Restricted` means a checked limitation or fallback applies. `Unknown` means no fresh execution evidence for this cell is present. `Proposed` belongs to the ten-step ASSEMBLY plan, not today's implementation.

Primary evidence is [implementation status](../docs/IMPLEMENTATION_STATUS.md), [capability output](../docs/CAPABILITIES.md), [ZX behavior](../docs/ZX.md), the stable feature registry in [features](../docs/FEATURES.json), [capability reporting source](../cmd/package_capabilities.zi), and retained tests. Cross-compilation or presence of a script never counts as target execution evidence.

## Execution modes

The opt-in [direct scalar prototype](README.md) is separate from these existing
engines. It emits Linux x86-64 ELF from checked source/saved IR, but does not
replace ZX or establish portability/performance completion.

| Mode | Current status | Evidence | C | Go | Portable ZX | Native ZX | Wasm | OS boundary |
|---|---|---|---|---|---|---|---|---|
| Generated C output | Working, source and saved IR | [implementation status](../docs/IMPLEMENTATION_STATUS.md), [C target](../tests/c99_target.sh) | emits C | n/a | n/a | lowers to same C backend | can compile selected C to wasm32 | Linux current host; Windows/macOS/Android toolchains have separate fixtures |
| Generated Go output | Working, secondary tier | [capabilities](../docs/CAPABILITIES.md), [call order](../tests/call_order.sh) | n/a | emits Go | n/a | n/a | n/a | Verifiable only where a compatible Go runtime/toolchain executes it |
| Portable VM | Working, explicit subset and verifier | [ZX contract](../docs/ZX.md), [VM verifier](../cmd/zir/zir_vm_verify.c) | n/a | n/a | runs | explicit `zx.native` failures do not fall back | fallback tier | Current retained checks run on Linux; VM is intended everywhere `libziran-vm` builds |
| Native ZX via TinyCC | Working on supported Linux/Android CPU hosts | [native execution](../docs/ZX.md) | generated C | n/a | n/a | executes | n/a | x86-64, ARMv7, ARM64 only; Windows/macOS portable |
| Browser linked native | Working via AOT objects | [linked browser code](../docs/ZX.md), [linked web test](../tests/zx_linked_web.sh) | AOT C | n/a | fallback | same code generator | wasm32/browser | Private headless Chromium fixture; parallel regions run in place |
| Wasm execution tier | Working restricted subset with provider fallback | [Wasm tier](../docs/ZX.md), [engine seam](../tests/zx_wasm_engine_test.c) | emitter | n/a | fallback | budget model shared | wasm32 | Scalars, owned strings, fixed arrays, records, typed `s64 -> s64` hosts; other constructs fall back |

## Portable language behavior

| Feature | Current status and target behavior | Evidence |
|---|---|---|
| Fixed-width integers | Working on C, Go, ZX source/saved IR: declared-width wrap, signed-min division/remainder, shifts, casts, bitwise and mixed-width behavior. Wasm uses the same scalar tier inside its subset; unknown outside available Wasm fixtures. | [numeric semantics](../tests/numeric_semantics.sh), [width matrix](../tests/numeric_width_matrix.sh), [manifest](../tests/numeric_conformance.json) |
| Float32/float64 | Working arithmetic, compare, infinity/NaN and range-checked conversion on native C/Go/native-ZX. Restricted: current `zi2zx run --vm` can evaluate chained float32 literals in double before final rounding; lowered IR must not preserve that gap. | [numeric manifest](../tests/numeric_conformance.json), [status](../docs/IMPLEMENTATION_STATUS.md) |
| Implicit widening | Working for the checked feature contract on C, Go, ZX. | [feature registry](../docs/FEATURES.json), [features test](../tests/features.sh) |
| SIMD vectors | Working with identical lanes across native C tiers, Go lane loops, native bundles, portable VM, and source/saved IR. C wasm32 may use SIMD128; browser bundles run SIMD through portable VM. CPU feature selection is target-specific. | [status](../docs/IMPLEMENTATION_STATUS.md), [SIMD target](../tests/simd_target.sh) |
| Scalars, strings, enums, plain records | Working in portable subset, including literals, copies, fields, scalar compound assignment, parameters and returns. Pointer-sized `isize`/`usize` and functions/statements outside the verifier subset are rejected. | [ZX subset](../docs/ZX.md), [VM verifier](../cmd/zir/zir_vm_verify.c) |
| Fixed arrays, slices, vector indexing | Working with checked bounds. Generated C may omit only proven bounds checks; portable host slices copy successful mutations back and reject overlapping mutable arguments. | [proven ranges](../tests/proven_index_ranges.sh), [host slices](../tests/host_slices.sh) |
| Compile-time assertions, `#run`, laws | Working as checked language features on all advertised targets; unsupported proof/effect forms or exhausted budgets are rejected/unknown rather than guessed. | [feature registry](../docs/FEATURES.json), [compile run](../tests/compile_run.sh), [laws](../tests/laws.sh) |
| Procedure defaults and procedure values | Working for advertised examples on C, Go, ZX; portable verifier checks slot signatures and rejects C callback procedure types. | [feature registry](../docs/FEATURES.json), [defaults](../tests/default_arguments.sh) |
| Print and I/O | Working literal-format print on C, Go, ZX. Rich OS I/O is a library/capability contract, not a portable opcode. | [feature registry](../docs/FEATURES.json), [print](../tests/print.sh) |
| Named imports and linking | Working; bundles follow direct calls, retain required startup/types/laws, and reject unresolved imports or unreachable unsupported code. | [ZX linker](../docs/ZX.md), [bundle verification](../tests/bundle_verification.sh) |

## Memory, ownership, and cleanup

| Feature | C | Go | Portable ZX | Wasm | Evidence |
|---|---|---|---|---|---|
| Zeroed heap allocation/free | Working | Working | Working; rejected use after free/double free | Not in documented Wasm subset; fallback | [heap allocation](../tests/heap_new.sh), [Wasm tier](../docs/ZX.md) |
| Owned `Vec` operations | Working | Working, physical OOM may terminate | Working, allocation failures recoverable where declared | Fallback for constructs outside Wasm subset | [owned values](../docs/OWNED_VALUES.md), [vector test](../tests/vec.sh) |
| Direct/aggregate vector drops | Working on block, return, break, continue | Working | Working; VM uses logical invalidation | Fallback | [aggregate safety](../tests/aggregate_vec_safety.sh) |
| Vector field/member moves | Working with listed source restrictions | Working | Working | Fallback | [aggregate safety](../tests/aggregate_vec_safety.sh), [move expression](../tests/vec_move_expression.sh) |
| Text and `TextView` | Borrowed bytes observable | Snapshot | Snapshot | Owned strings in Wasm subset; other text forms fallback | [capabilities](../docs/CAPABILITIES.md), [text view mutation](../tests/text_view_mutation.sh) |
| Native pointers | Native lifetime/alias responsibility | Native Go pointer rules | Opaque host handles in bundle ABI | Linear-memory boundary/provider values | [ZX host ABI](../docs/ZX.md), [capabilities](../docs/CAPABILITIES.md) |
| Instance global state | Native instance data | Go program state | Independent mutable slots per instance | Independent bindings/linear memory per instance | [component state](../docs/ZX.md), [component engine test](../tests/bundle_component_test.c) |

## Calls, concurrency, fuel, and traps

| Feature | Current behavior | Evidence |
|---|---|---|
| Argument and compound order | Working: left-to-right call arguments; destination read before compound right side on C, Go, ZX. | [call order](../tests/call_order.sh), [compound order](../tests/compound_order.sh) |
| Bounded execution | Working by explicit host selection. VM counts statements; native/Wasm count calls and loop iterations; cumulative limited open includes initialization and every run. | [ZX limits](../docs/ZX.md), [step limit](../tests/bundle_step_limit.sh) |
| CPU parallel regions | Working restricted: C threads on supported hosts; Go/ZX serial downgrade; reverse regions serial. Worker failures and fuel are deterministic. | [parallel test](../tests/parallel.sh), [native parallel](../tests/zx_native_parallel.sh) |
| GPU regions | Working as CPU fallback only; no device backend. | [capabilities](../docs/CAPABILITIES.md), [assembly blocks](../tests/assembly_blocks.sh) |
| Runtime traps | Working failure boundary: bounds, unreachable, host/fuel/stack and numeric contract failures stop the run. | [native execution](../docs/ZX.md), [VM run](../cmd/zir/zir_vm_run.c) |
| Host capabilities | Working checked module/function ABI with provider preflight, bindings, mutable slice copyback and no overlap. | [host capabilities](../tests/host_capability.sh), [ZX host ABI](../docs/ZX.md) |
| Component replacement | Working transactional ABI/state migration/rollback for portable components and supported Wasm memory interface. | [component contract](../docs/ZX.md), [web lifecycle](../tests/zx_web_lifecycle.sh) |

## Native extensions and foreign runtimes

| Feature | Working behavior and restriction | Evidence |
|---|---|---|
| GNU assembly variants | C functions/local blocks on represented x86 and Linux/Android AArch64 forms; unsupported platform uses checked portable body. Go uses cgo bridge or restricted direct ABI0 path; ZX uses portable body; bundle files never execute their own variants. | [assembly](../tests/assembly.sh), [ZX assembly](../tests/zx_native_asm.sh), [Go assembly](../tests/go_assembly.sh) |
| C callbacks / `#c_call` | Native C ABI only; Go and ZX target preflight reject it before output. | [capabilities](../docs/CAPABILITIES.md), [compiler inventory](../tests/compiler_inventory_cases.zi) |
| Go foreign types/maps | Go target extension only; opaque identity and scalar-union checks reject unsupported portable lowering. Renaming does not create a portable layout. | [compiler IR](../cmd/compiler_ir.zi), [capabilities](../docs/CAPABILITIES.md) |
| OS libraries | Explicit foreign/library dependencies remain allowed; native platform service portability is defined per OS row, not by opcode presence. | [bundle libraries](../tests/bundle_libraries.sh), [architecture](../docs/ARCHITECTURE.md) |
| Runtime assembly/C cache | Current native bundle cache is keyed by bundle/libraries/CPU/compiler sources; cached code can bypass program recheck only under documented boundary. | [native execution](../docs/ZX.md) |

## Maintained platform rows

| Platform / target row | Current verified surface | Target-specific restriction | Unknown verification |
|---|---|---|---|
| Linux x86-64, C and native ZX | Current development host; source/saved-IR C, Go, ZX, native bundle, threads, SIMD tiers, OS libraries and retained tests. | Native C parallel threads; TinyCC path; SIMD ISA dispatch; Linux process/thread services. | A complete matrix needs fresh named toolchain/hardware evidence per future gate. |
| Linux/Android ARMv7 | Native ZX supported; C SIMD fixtures cover NEON, VFP-only and scalar lanes; Android API 21 streams and optional four-ABI link paths. | 32-bit ARM ABI/CPU feature restrictions; optional NDK/QEMU/phone tooling. | Specific Android OS version/device combinations and non-SIMD execution are not established by this documentation pass. |
| Linux/Android AArch64 | Native ZX supported; NEON codegen and QEMU execution documented. | AArch64 hardware-capability assembly selection; Android toolchain. | Fresh physical-platform coverage beyond documented fixtures is unknown. |
| Linux/Android other CPU | Portable VM and generated C/Go are the supported route. | No native TinyCC ZX row beyond x86-64/ARMv7/ARM64. | New architecture execution, including RISC-V, is proposed/unknown. |
| Windows generated C/Go | Compiler/runtime platform code and Windows fixtures exist. | Native ZX does not use TinyCC; ZX runs portable VM. Bundle loading uses seekable temporary streams. | No fresh Windows host execution in this documentation task. |
| Windows portable ZX | Intended and implemented VM boundary. | No native tier; no Linux-only service assumptions. | Fresh Windows host evidence unknown. |
| macOS generated C/Go and portable ZX | Darwin build definitions and package/file fixtures exist. | Native ZX uses portable VM; Darwin SHA/random system libraries and ABI differ. | No fresh macOS execution in this documentation task. |
| Browser/WebAssembly linked tier | wasm32 AOT objects, per-instance memory/state, budget, lifecycle and independent linked instances tested in headless Chromium. | Subset listed above; unsupported constructs/provider rejection fall back to portable VM; SIMD bundles use portable VM. | Other browsers/versions and non-Chromium Wasm hosts are unknown. |
| Wasm32 standalone tools | `zi2wasm` emitter and engine seam retained; optional Emscripten/wasmtime probes for SIMD and file-tree fixtures. | Requires installed Wasm CC/runtime; skipped when probes absent. | Current machine's optional tool availability is not execution evidence for target hosts. |
| Go output on OS | Generated source follows Go runtime. | Serial parallel regions, snapshots, foreign runtime restrictions. | Exact OS/Go version combinations not covered by a fresh matrix run. |

## Retained check surface

The retained suite is physically pruned. `make check` builds the retained tool and runs every discovered `.sh` check unless filtered; `make check-bootstrap` compares checked-in generated compiler sources; both run with desktop display variables removed. This is the suite that must accompany later implementation work. See [Makefile](../Makefile) and [check planning](../tests/check_plan.zi). Hardware/tool-dependent scripts may still skip when their required external probe is absent; a skip is not target execution evidence.

## Unknown and proposed work

**Unknown now:** fresh Windows/macOS/Android/browser execution for this exact revision; Wasm constructs outside the documented subset; Go behavior on every OS/runtime pair; pointer aliases and foreign effects lacking checked summaries; all future-backend target cells until a named fixture runs. Optional-tool skips, cross-compilation, child receipts, and stored benchmark reports remain non-evidence.

**Required step-01 follow-up:** lower the contract into typed basic blocks and verifier records; define portable layouts and effects; encode cleanup/fuel regions explicitly; separate target extension records from portable opcodes; then choose an incompatible `.zx` version against the actual writer in step 05. Feature completion requires fresh source/saved-IR execution evidence for every required matrix cell, with legitimate target exclusions named rather than silently skipped.
