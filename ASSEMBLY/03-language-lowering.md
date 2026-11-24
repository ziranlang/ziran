# Step 03 — Lower the complete supported language into executable operations

Status: in progress; [scalar, record, fixed-array, procedure-value and restricted global/pointer lowering](README.md). Complete language coverage remains pending. Depends on [step 02](02-lowered-ir-and-verifier.md). Next: [portable instruction execution](04-portable-instruction-engine.md).

Missing lowering follows the [gap policy](gap-policy.md): it is a stable,
source-located compiler error and an open matrix cell, never a request to
evaluate the AST, generate C, or select another engine.

## Objective

Translate checked `.zir` into the new portable executable model without recovering program meaning from statement text. Cover the full matrix from step 01; use a small arithmetic subset only to establish the lowering framework.

## Existing integration points

- [Checked program schema](../cmd/compiler_ir.zi) and [semantic body utilities](../cmd/compiler_zir_lower.zi).
- [Current statement, expression, and call emission](../cmd/compiler_zir_lower.zi) as behavior references, not source-generation dependencies.
- [Current semantic cleanup implementation](../cmd/compiler_ir_cleanup.zi).
- [Current native symbol/layout planning](../cmd/compiler_machine_elf.zi).
- [Current assembly metadata and LIR printing](../cmd/compiler_zir_lower.zi) and [SIMD classification](../cmd/compiler_simd_metadata.zi).

Proposed modules: `cmd/compiler_lower.zi`, `cmd/compiler_lower_expr.zi`, `cmd/compiler_lower_control.zi`, `cmd/compiler_lower_storage.zi`, and `cmd/compiler_lower_calls.zi`. Split further only when responsibilities require it.

## Implementation work

- [x] Lower concrete checked functions after generic specialization and compile-time selection. Preserve declaration ownership, imported names, public/private linkage, and stable function identity. ([`compiler_zir_lower.zi`](../cmd/compiler_zir_lower.zi) lowers checked programs, [machine_ir_cases.zi](../tests/machine_ir_cases.zi) lowers the real LIR, and [generic_assembly_cases.zi](../tests/generic_assembly_cases.zi) retains generic-identity coverage.)
- [x] Lower expressions in the language's evaluation order. Named arguments evaluate in source order, then map to parameter positions; do not accidentally inherit machine ABI order. (Lowering evaluates arguments in source order before the call ([`compiler_zir_lower.zi`](../cmd/compiler_zir_lower.zi)); [call_order.sh](../tests/call_order.sh), [compound_order_cases.zi](../tests/compound_order_cases.zi) and [native_operand_order_test.zi](../tests/spec/native_operand_order_test.zi) retain it.)
- [ ] Lower short-circuit logic, conditional expressions, branches, every supported loop form, and named break/continue into explicit blocks and edges. Landed slice: short-circuit logic, conditionals and the supported loop forms lower and execute with retained fixtures ([native_loops.sh](../tests/native_loops.sh), [assembly_cases.zi](../tests/assembly_cases.zi)). Doubt recorded: no retained direct-target fixture proves named/labeled break and continue lowering, so that part stays open.
- [ ] Lower returns through a shared, checkable cleanup mechanism. Preserve `defer`, owned-vector/text destruction, moves, temporary lifetime, early exits, and unwind behavior wherever the target contract supports it.
- [ ] Extend the implemented mutable scalar locals, checked global/frame
  pointers, nested record/fixed-array paths, immutable string literals/copies,
  result records and aggregate copies to local aggregate/procedure-value
  address-taking, heap-derived pointers, slices, dynamic aggregates, remaining
  string operations and enum storage. Preserve zero initialization and copy
  behavior.
  M1 now lowers local aggregate/procedure address-taking, borrowed fixed-array
  and byte-view slices, checked string indexing/comparison and ordered runtime
  initializers. Resizable owned aggregates, generated cleanup/moves, unions and
  remaining enum-storage coverage keep this broader item open.
- [x] Make alias-sensitive assignments and in-place aggregate results correct when a later argument mutates the destination or a global aliases an argument. Optimizations must not change old-value reads. (Lowering reads destinations before later calls ([`compiler_zir_lower.zi`](../cmd/compiler_zir_lower.zi)); [assembly_cases.zi](../tests/assembly_cases.zi) retains the `global_pointer_alias` and `element_alias` fixtures.)
- [x] Emit explicit bounds/trap operations before optimization. Carry enough range and effect information for later verified check elimination; do not copy C-only proof shortcuts without their preconditions. (Bounds/trap emission in [`compiler_zir_lower.zi`](../cmd/compiler_zir_lower.zi); verified check elimination from proven facts in [`compiler_lowered_proven.zi`](../cmd/compiler_lowered_proven.zi), retained by [proven_pointer_cases.zi](../tests/proven_pointer_cases.zi) and [guarded_index_ranges.sh](../tests/guarded_index_ranges.sh).)
- [ ] Lower direct/indirect calls, procedure values, callbacks, foreign C calls, varargs, startup functions, and function-address equality. Record ABI intent without assigning physical argument registers yet. Landed slice: direct/indirect calls, procedure values and startup entry lower and execute ([lowered_exec_cases.zi](../tests/lowered_exec_cases.zi), [executable_bundle_cases.zi](../tests/executable_bundle_cases.zi)). Callbacks, foreign C calls and varargs keep this item open.
- [ ] Represent language/runtime helpers through typed symbols with defined semantics. Implement helpers in Ziran where possible; inventory every remaining dependence on generated C macros, headers, or compiler intrinsics.
- [ ] Lower portable SIMD to explicit lane types and operations. Preserve saturation, wrapping, masks, lane order, shuffles, and signedness; keep native vector width selection for target lowering.
- [ ] Lower parallel regions into ordinary outlined procedures plus typed runtime operations. Preserve captures, reductions/results, deterministic failure behavior, and synchronization boundaries.
- [ ] Treat GPU offload and OS services as explicit declared backend/runtime capabilities. Implement existing behavior where supported and reject genuine missing facilities before execution without introducing application-specific compiler rules or CPU fallbacks.
- [ ] Preserve `#asm` portable implementations and typed native variant bindings as separate target implementations. A portable build lowers the portable implementation; a direct native build requires a matching architecture/feature variant and rejects its absence. Native templates have explicit operands, effects, clobbers, architecture, and CPU requirements; they cannot become portable opcode semantics.
- [x] Reject reachable foreign Go objects, Go maps/interfaces, reflection-only tags, or runtime operations in targets that cannot implement them. Retain the Go backend and its existing valid programs. (Opaque-identity and scalar-union checks reject unsupported portable lowering ([feature matrix](feature-matrix.md), [`compiler_ir.zi`](../cmd/compiler_ir.zi)); [compiler_inventory_cases.zi](../tests/compiler_inventory_cases.zi) retains the rejection and the Go target stays selectable.)
- [ ] Carry global initialization order, reachability roots, callback roots, exported ABI layouts, diagnostic spans, and law/proof provenance into the lowered program. Landed slice: ordered global initializers, entry/reachability roots, origin spans and law material link and execute ([`compiler_zx_link_reach.zi`](../cmd/compiler_zx_link_reach.zi), [bundle_provider_cases.zi](../tests/bundle_provider_cases.zi), [bundle_global_reachability.sh](../tests/bundle_global_reachability.sh)). Callback roots and exported ABI layout records keep the rest open.
- [x] Feed the result through the step-02 verifier unconditionally and make lowering failures recoverable through the compiler API.

## Tests

Create lowering fixtures in Ziran and reuse current behavior tests, including [numeric semantics](../tests/numeric_semantics.sh), [operand order](../tests/spec/native_operand_order_test.zi), [parallel captures](../tests/parallel_captures.sh), [generic assembly](../tests/generic_assembly_cases.zi), and [saved-IR mutations](../tests/saved_ir_mutations.zi).

For each matrix feature, compile from `.zi` and saved `.zir`; compare verified lowered structure, outputs, traps, observable host calls, and cleanup effects against the existing supported execution paths. Add independent expected-result oracles for numeric boundaries and aliasing cases rather than relying only on agreement between backends sharing helper code.

## Exit gate

Every supported source construct has an explicit lowering, a deliberate Excluded contract, or a named Gap with a stable diagnostic. No placeholder operation invokes an AST evaluator, generated C fragment, source-text interpreter, portable body, or alternate backend. All required cells must be implemented before step 10 can declare completion.

## Handoff

Deliver the complete lowered fixture corpus, runtime helper inventory, explicit capability signatures, and source-to-operation mapping. Interpreter and native work can proceed in parallel once the applicable opcode contracts are stable.
