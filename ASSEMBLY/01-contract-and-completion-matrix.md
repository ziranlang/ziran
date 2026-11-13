# Step 01 — Freeze the execution contract and completion matrix

Status: in progress. The experimental [scalar prototype](README.md) does not close this step's completion gates. Delivery order, plan review and per-row cutover rules are in [milestones](milestones.md); capturing the Alpha physical-host baseline of the old engines is milestone M0, before any old path is replaced.

The [assembly-target gap policy](gap-policy.md) is normative. A missing
implementation is a named failing matrix cell with a stable diagnostic; no
fallback, downgrade, skipped probe, emulated run, or legacy engine can satisfy
that cell.

## Objective

Upgrade `.zx` into a portable executable instruction format and add a direct native compilation path. Preserve `.zir` as the checked language interchange format. Make every transformation inspectable, including the final linked instructions. QBE is outside this plan.

The intended architecture is:

```text
.zi -> .zir -> lowered portable IR -> .zx container
                              |                  |
                              +--------+---------+
                                       |
                         optimization and target lowering
                                       |
                    machine IR -> registers and stack frames
                                       |
                     instruction encoding -> object code
                                       |
                         native linker -> executable

.zx -> instruction verifier -> portable interpreter
.zx -> instruction verifier -> same native backend -> executable memory
```

Assembly listings are views of the instruction stream used for encoding, not a required write-and-reparse stage. Final disassembly reads the actual linked or loaded bytes.

## Ordered implementation files

1. [Contract and completion matrix](01-contract-and-completion-matrix.md).
2. [Lowered IR and verifier](02-lowered-ir-and-verifier.md).
3. [Complete language lowering](03-language-lowering.md).
4. [Portable instruction engine](04-portable-instruction-engine.md).
5. [ZX format and linking](05-zx-format-and-linking.md).
6. [Optimization and first native backend](06-optimization-and-native-backend.md).
7. [Architecture and platform coverage](07-architecture-and-platform-coverage.md).
8. [Assembler, object files, and native linker](08-assembler-objects-and-linker.md).
9. [Native runtime, cache, and host integration](09-native-runtime-and-integration.md).
10. [Inspection, performance, and final cutover](10-inspection-performance-and-cutover.md).

## Current repository boundaries

- [The shared IR schema](../cmd/compiler_ir.zi) represents typed statements, expression graphs, declarations, source spans, laws, and proofs.
- [The current ZX codec](../cmd/compiler_bundle.zi) packages portable IR and metadata; [the bundle implementation](../cmd/zir/zir_bundle.c) connects it to checking and linking.
- [The native bundle lowering](../cmd/zir/zir_zx_native_lower.c) generates C; [the native engine](../cmd/zir/zir_zx_native.c) uses TinyCC.
- [The existing native build rules](../native-zx.mk) and [architecture contract](../docs/ARCHITECTURE.md) must change only after replacement paths are verified.
- Current working-tree changes belong to their existing tasks. Record a baseline and coordinate overlapping files before implementation; never absorb unrelated changes into this feature.

## Required decisions and deliverables

- [ ] Create a feature-by-target matrix from current compiler capability reporting, tests, and documented limitations. Name each feature, reference fixture, source/saved-IR behavior, portable behavior, native behavior, and required execution platform.
- [ ] Separate portable language behavior, native language extensions, CPU requirements, OS services, and foreign-runtime requirements. A Go-only foreign type does not become a portable native type by renaming its representation.
- [ ] Set the native language coverage target to all currently supported C-native language semantics, plus all currently supported portable ZX semantics. Keep C and Go output available. List legitimate target-specific exclusions explicitly.
- [ ] Cover the maintained Linux, Android, Windows, macOS, and browser/WebAssembly combinations. Derive exact CPU/OS/ABI rows from the live toolchain and repository; include existing 32-bit ARM support. New architectures such as RISC-V are explicit extensions, not an unbounded definition of completion.
- [ ] Specify numeric behavior, memory and pointer rules, ownership, cleanup, initialization, traps, call order, concurrency, fuel checkpoints, and capability contracts before assigning opcodes.
- [ ] Define the new format as an incompatible executable-format revision. Do not reserve an arbitrary version number now; choose it against the current writer when implementing step 05.
- [ ] Define backend APIs and ownership: verified portable program, target description, machine program, encoded image, and loaded instance. Separate immutable code from mutable instance state.
- [ ] State that validated native code is not an operating-system sandbox. Keep malformed-input rejection, capability restrictions, and native process isolation claims distinct.
- [ ] Define debug and release policies. Semantic checks remain correct in both; stripping inspection metadata is an explicit build choice.
- [ ] Record that cross-compilation, successful encoding, a child-agent receipt, or a saved benchmark report does not establish target execution correctness.
- [ ] Assign every non-Implemented matrix cell a stable Gap diagnostic or a
  deliberate Excluded contract. Verify rejection before output replacement,
  initialization, host calls or entry execution.
- [ ] Make engine and target selection explicit. Portable, native, Wasm, C and
  Go are separate contracts; none may rescue another after preflight fails.

The current M0/M1 implementation keeps those routes distinct: portable runs
verified LIR, native means direct verified-LIR-to-machine-code/platform output,
and the separately selected C and Go backends generate source that can be
compiled into platform binaries. Native slice comparison and host ABI gaps stop
before output replacement; they do not select any of the other routes.

## Proposed implementation ownership

Implement reusable compiler/runtime modules in Ziran under `cmd/`, following the existing compiler modules. Add only necessary C embedding and OS glue at the existing boundaries. Proposed module names in later steps are implementation targets, not files claimed to exist today. Use the current C backend to bootstrap new Ziran code initially, then verify the new backend can build those modules itself.

## Definition of 100% complete

Every required matrix cell has fresh execution evidence; no required cell is
silently skipped or satisfied by a fallback. Every remaining gap has its stable,
side-effect-free error and stays visibly incomplete. Both source and saved
`.zir` reach the same lowered semantics. `.zx` runs without reparsing semantic
statement text or requiring embedded executable `.zir`. Native AOT and native
ZX execution use the same code generator without generated C, TinyCC, an
external assembler, or an external linker on the promised direct path. External
OS libraries remain allowed dependencies.

All ten exit gates must pass. A smaller scalar prototype is a milestone, never the finished upgrade. There is no promise of universally optimal machine code: performance acceptance uses declared workloads and measured regressions, not the phrase “absolute performance.” Missing hardware, signing facilities, or tools remain named incomplete gates.

## Verification and handoff

Save the baseline matrix, reproducible fixture list, explicit non-goals, and ownership map in this folder or the established documentation/report locations. Do not create release tags or bump the product version for planning. Step 02 may proceed once the semantic and portability contracts are concrete enough to encode and validate.
