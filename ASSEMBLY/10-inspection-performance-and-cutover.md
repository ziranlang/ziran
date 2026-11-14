# Step 10 — Complete inspectability, validate performance, and cut over

Status: in progress; [lowered instruction dumps, optimization counters and bounded native byte-range maps](README.md). Complete pipeline inspection, profiling, performance validation and cutover remain pending. Depends on all preceding steps. This is the only step that may declare the complete upgrade finished.

## Objective

Make bottlenecks traceable from source to executed instructions, validate the complete feature/platform matrix, replace the old ZX execution contract, and install the locally verified toolchain. Inspection functionality is developed alongside steps 02–09; it is not postponed until this final gate.

## Existing integration points

- [Inspection command](../cmd/zir-inspect/main.c), [compiler profiling](../cmd/zir/zir_profile.h), and [package explain](../cmd/package_explain.zi).
- [Compiler command](../cmd/package_entry.zi), [compile-command reporting](../cmd/package_compile_commands.zi), and [feature reporting](../cmd/package_features.zi).
- [Check discovery](../tests/check_plan.zi), [build/test rules](../Makefile), and [benchmark instructions](../bench/README.md).
- [Architecture](../docs/ARCHITECTURE.md), [ZIR](../docs/ZIR.md), [ZX](../docs/ZX.md), [usage](../docs/USAGE.md), and [implementation status](../docs/IMPLEMENTATION_STATUS.md).

Proposed modules: `cmd/compiler_inspect_exec.zi`, `cmd/compiler_debug_map.zi`, and `cmd/compiler_profile_native.zi`. Final CLI spellings are implementation decisions; document them as proposed until they actually exist.

## Inspectability deliverables

- [ ] Provide deterministic dumps for ZIR, initial portable IR, optimized portable IR, selected machine IR, allocated machine IR, assembly, object metadata, link map, and final executable disassembly.
- [ ] Map source spans and inline/generic origin chains to instructions, addresses, symbols, and stack locations. Preserve many-to-many mappings when optimization combines or duplicates operations.
- [ ] Explain eliminated checks, folded expressions, inlining decisions, missed vectorization, dispatch selection, spills/reloads, frame sizes, call sites, and linker-generated stubs/padding.
- [ ] Produce human-readable output and structured JSON for each stage. Large programs support bounded function/block/address selection; tools must not require dumping an entire binary to inspect one function.
- [ ] Keep assembly/listing output tied to the actual encoder and final disassembly tied to actual bytes, including relocated addresses. Label estimated costs separately from measured costs.
- [ ] Generate source/line and unwind metadata suitable for platform debuggers and profilers. Register JIT functions and address ranges where supported, unregister on retirement, and keep sampled addresses attributable after code moves/unloads.
- [ ] On Linux, integrate available `perf`/JIT symbol-map facilities; supply the corresponding supported mechanisms on other maintained platforms. Record missing hardware counters explicitly rather than fabricating cycle data.
- [ ] Attribute samples to functions, instructions, inline frames, CPU variants, and host bridges. Report hot-path execution, spills, cache/branch evidence, and host-call overhead only when measured data supports the claim.
- [ ] Make stripped and debug artifacts share an explicit build identity and compatible sidecar maps. Inspection output must state the code identity, target, variant, optimization policy, and whether addresses describe AOT or loaded code.

## Correctness and integration closure

- [ ] Close every required cell in the step-01 matrix with fresh real execution evidence. Keep target-specific exclusions distinct from missing implementation.
- [ ] Verify the [gap policy](gap-policy.md): every remaining Gap has a stable
  diagnostic and no side effects; capability and inspection output expose it;
  no fallback, downgrade, skip, emulation or legacy engine is counted as support.
- [ ] Run source/saved-IR/new-ZX, optimized/unoptimized, AOT/native-ZX/interpreter, optional variants, foreign ABI, callbacks, concurrency, assets, libraries, persistence, and malformed-input suites.
- [ ] Run every retained local test with `make check` and `make check-bootstrap`. Do not substitute filtering, sharding or the retired `make check-full` workflow for the repository's physically pruned suite.
- [ ] Run sanitizer/fuzz/resource-bound campaigns against new decoders, verifiers, object readers, loaders, and instruction execution. Store minimized cases as Ziran fixtures and fix relevant failures.
- [ ] Verify new-backend self-hosting and bootstrap reproducibility, standalone output without external compilation/link tools, browser wasm execution, and supported downstream hosts on the full local stack.
- [x] Confirm old-format rejection and retire old ZX-specific AST execution and C/TinyCC generation only after replacements pass. Preserve compile-time/law tooling evaluators and optional C/Go backends still in use. A file of another revision is rejected with no fallback reader; the semantic bundle writer and reader, the portable VM, bundle libraries and the TinyCC ZX route are removed.

Run every build and compiling test through `build-queue --`; remove `DISPLAY` and `WAYLAND_DISPLAY` unless a private display fixture supplies them. Missing target hardware or local integration dependencies are blockers to their completion cells, never a reason to substitute a CI promise.

## Performance acceptance

- [ ] Define workload-specific regression budgets before measuring. Cover arithmetic, branch-heavy loops, records/arrays, strings, calls, register pressure, SIMD/codec kernels, parallel regions, host crossings, cold compile/startup, warm-cache startup, memory use, and binary size.
- [ ] Benchmark only on Alpha's physical host directly, rejecting
  virtualization and preserving the actual host, CPU/OS features, toolchain
  identities, flags, workload inputs, selected engine, and output correctness.
  If Alpha is unavailable, leave performance verification pending.
- [ ] Compare new-native AOT and native ZX with the M0 receipts of the removed TinyCC path, portable instruction execution, and independent C/Go controls where meaningful. Compare matched semantics, safety checks, and feature levels.
- [ ] Retain all raw samples and excluded indices. Apply the same symmetric 1.5×IQR rule with inclusive interpolated quartiles to every target/platform comparison.
- [ ] Investigate regressions using source-linked final disassembly and measured profiles. Fix common lowering/allocation/runtime causes before adding workload-specific instruction cases.
- [ ] Report gains and losses by workload. Do not replace missing measurements with an “absolute fastest” claim or confuse cross-compilation with benchmark execution.
- [ ] Deliver the owner's requested full before/after ZX comparison when the implementation is finished: old portable VM and old C/TinyCC-native ZX against the new portable instruction engine and new-native ZX, plus direct AOT and matched C/Go/Wasm controls where supported. Show absolute timings, speedup/slowdown ratios, cold compilation and startup, warm-cache startup, warm throughput, memory use, generated code/bundle size, raw samples, excluded indices, and named missing measurements. The old engines are removed, so their before measurements are the receipts and correctness oracles captured on Alpha at [milestone](milestones.md) M0; historical results from a different host or workload are not the before measurement. Interim comparison 1 followed M1, and interim comparison 2 follows M3; this final report supersedes them.

## Documentation, installation, and publication

- [ ] Update format/opcode specifications, language/native target boundaries, capability reporting, CLI help, embedding API docs, error guides, and inspection examples.
- [ ] Update `CHANGELOG.md` under `Unreleased`. Regenerate affected documentation/site outputs through existing scripts; translate touched locale content where applicable.
- [ ] Install from the current working tree at the stable path and verify the installed tool can compile, inspect, and run representative new-format programs. Do not terminate the owner's running applications.
- [ ] Keep implementation uncommitted until the complete feature set and meaningful required local tests pass. Missing required verification leaves the work uncommitted.
- [ ] Follow the owner's applicable commit/push authority at implementation time, preserving unrelated work. A release/version bump/tag requires an explicit release request and the existing version workflow.
- [ ] If pushed under applicable authority, report “pushed, CI not checked yet” without waiting for or polling CI. Website publication follows the existing GitHub workflow only; verify deployment separately when it is actually requested/performed.

## Final exit gate and completion receipt

All ten steps are complete only when the portable instruction format, reference engine, complete required native target matrix, internal encoders/linkers, native ZX runtime, inspection/profiling surfaces, self-hosting, and installation have executed successfully under their required tests. Any Gap, Unknown, fallback, downgrade, skip, emulated substitute, or legacy-engine substitute keeps the corresponding gate open. Record exact remaining limitations instead of rounding partial coverage to 100%.

The final receipt names changed components, local test coverage, benchmark host/results, target execution coverage, installed behavior, and independently confirmed commit/push/release/deployment states. A successful build, commit, or agent handoff alone is not a completion receipt.
