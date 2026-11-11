# Step 04 — Execute the portable instruction set directly

Status: in progress; the [isolated verified-LIR engine](portable-execution.md) executes the current verifier-supported subset through the opt-in `zi2asm --run` path. Full memory, host-capability, lifecycle/state and target integration remain required. Depends on [steps 02](02-lowered-ir-and-verifier.md) and [03](03-language-lowering.md). Next: [ZX container integration](05-zx-format-and-linking.md).

## Objective

Build a portable execution engine for the new lowered instructions. It is a semantic reference and a working backend for hosts without native code generation. It must not walk `.zir` expressions or reconstruct source statements during execution.

## Existing integration points

- [Current VM interface](../cmd/zir/zir_vm.h), [execution](../cmd/zir/zir_vm_run.c), and [expression evaluator](../cmd/zir/zir_vm_eval.c).
- [Current host-value adapters](../cmd/zir/zir_zx_host.c) and [host integration](../cmd/zir/zir_host.c).
- [Current SIMD execution](../cmd/zir/zir_vm_simd.c).
- [Portable execution contract](../docs/ZX.md).

Implemented module: `cmd/compiler_exec.zi`. Proposed additional modules: `cmd/compiler_exec_memory.zi` and `cmd/compiler_exec_host.zi`. Keep required embedding exports at the existing C API boundary. Current source/saved-IR and serialized-instruction tests cover scalar/aggregate operations, direct/indirect calls, instance globals, explicit frames, budgets and structured traps; they do not establish completion of every item below.

## Implementation work

- [ ] Decode verified instructions once into an execution plan with indexed value slots and block destinations. Keep immutable function plans shareable across instances.
- [ ] Implement arithmetic, comparisons, conversions, branches, block-parameter transfers, memory operations, calls, and traps for every portable opcode.
- [ ] Implement scalar and aggregate storage with explicit logical layout and host translation. Preserve pointer provenance and current legal pointer behavior; do not silently replace the language memory model with WebAssembly linear-memory rules.
- [ ] Execute block-parameter transfers as parallel copies, including swap/cycle cases. Preserve exact source-visible behavior across loops and conditional joins.
- [ ] Implement calls and recursion using owned execution frames and defined stack/resource limits. A failure returns a structured runtime result without corrupting the host or a different instance.
- [ ] Preserve owned storage, global initialization, returned aggregates, strings with embedded NULs, empty/null slices, vector scope cleanup, and native callback boundaries where supported.
- [ ] Adapt capability calls using validated typed signatures. Check host result shapes before writing them into execution storage; preserve nested host-to-bundle calls and reentrancy contracts.
- [ ] Specify and implement fuel at stable semantic checkpoints. Optimizations must preserve when exhaustion becomes observable, especially relative to host side effects and traps. Any batched charging requires a proof that timing is equivalent.
- [ ] Implement portable SIMD lane behavior and legal parallel runtime operations without assuming native vector instructions or a host thread count.
- [ ] Keep execution state, host bindings, error state, and mutable globals per instance. Exercise simultaneous instances, nested calls, replacement, and retirement.
- [ ] Preserve snapshot/state migration semantics and library composition. Stable persistent identities must survive harmless instruction ordering changes without equating unrelated layouts.
- [ ] Make debug stepping instruction-based with source mappings and inspectable value slots. Avoid debug-only execution semantics that hide release-mode defects.
- [ ] Retain any existing `.zir` evaluator needed for compile-time execution, laws, or tooling until those consumers are deliberately migrated. Removing ZX dependence does not authorize deleting unrelated evaluator services.

## Tests

Add `tests/lowered_exec_cases.zi` and mutation/fault fixtures. Reuse [portable pointers](../tests/portable_pointers.sh), [host strings](../tests/host_strings.sh), [portable records](../tests/portable_host_records.sh), [VM stack bounds](../tests/vm_stack_bounds.sh), [VM retirement](../tests/vm_retirement.sh), and [concurrent instances](../tests/vm_concurrent_instances.sh).

Require the same results from source lowering, saved-IR lowering, encoded/decoded executable instructions, and the reference engine. Compare traps and host-call traces as well as final return values. Test allocation failures, malformed host responses, recursive callbacks, repeated failures, and cancellation/resource exhaustion without leaking instance state.

Run on the current machine and the required supported hosts; use private displays only where a host test actually needs them. Cross-compiled objects are not portable engine execution evidence.

## Exit gate

All required portable matrix cells run from lowered instructions with no executable `.zir` tree present. The engine rejects unverified programs, preserves instance isolation and documented resource behavior, and can supply independent differential results for native backend development.

## Handoff

Deliver the engine API, portable opcode coverage table, executable fixtures, trap/side-effect traces, and state migration tests. Performance tuning of dispatch follows measurement and does not block the semantic reference from being useful.
