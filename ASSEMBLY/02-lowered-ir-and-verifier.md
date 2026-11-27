# Step 02 — Implement lowered portable IR and its verifier

Status: in progress. The typed schema ([`compiler_lowered_ir.zi`](../cmd/compiler_lowered_ir.zi)), its bounded canonical codec, the structural verifier ([`compiler_lowered_verify.zi`](../cmd/compiler_lowered_verify.zi)), deterministic LIR text/serial dumps, and the optimization-fact layer under `--checks` are implemented and retained-test covered for the current M1 language subset, including checked global-derived pointers and procedure values ([README.md](README.md)). Heap-derived pointers, slices, unions, vectors, general ownership and the remaining language coverage keep the step open. Depends on [step 01](01-contract-and-completion-matrix.md). Next: [complete language lowering](03-language-lowering.md).

The schema carries the [gap policy](gap-policy.md) through target preflight:
unsupported required operations and target extensions have stable identifiers
and cannot be rewritten as calls into another engine or backend.

## Objective

Create a structured, serializable representation of executable operations below `.zir`. It must support efficient interpretation and native lowering while retaining useful optimization and debugging facts. Do not make portable instructions expose physical registers, host addresses, or a particular calling convention.

## Existing integration points

- [Checked compiler records](../cmd/compiler_ir.zi).
- [ZIR reader and writer](../cmd/compiler_zir_serial.zi).
- [Scalar semantic helpers](../bootstrap/runtime/zir_scalar.h).
- [Compiler embedding contract](../docs/COMPILER_API.md).

Proposed new modules: `cmd/compiler_lowered_ir.zi`, `cmd/compiler_lowered_verify.zi`, and `cmd/compiler_lowered_text.zi`. Keep binary ZX framing in step 05 rather than coupling instruction records to file offsets.

## Typed console service boundary

The direct console path represents output as effectful `HostCall` instructions,
not as generated C/Go or an untyped call to a process address. Its reserved
module is `ziran.platform`. The byte-output signature is exactly
`ziran.platform.write_all(u64 fd, *u8 bytes, u64 count) -> void`, with Host
calling convention, no variadic parameters, and no body. Here `*u8` is a
verified Ziran pointer token, not a raw host address. The native veneer resolves
that token to permitted byte storage and checks the requested extent before
issuing an operating-system write.

`compiler_lowered_verify.zi` admits this one pointer parameter only for that
exact module, name, arity, and type signature. It does not relax the ordinary
host-capability restriction on pointers or pointer-containing aggregates. The
native encoder independently validates the service signature; matching a name
alone cannot authorize a different ABI.

Literal-format `print` evaluates all arguments before its first output call.
It then emits literal byte writes and Ziran-owned signed-integer,
unsigned-integer, and boolean formatting services. The scalar formatter names
are `ziran.print.signed(s64)`, `ziran.print.unsigned(u64)`, and
`ziran.print.bool(bool)`, all returning void in the same reserved module.
`HostCall` retains its effect ordering through optimization. Float-format
service records may be inspected or serialized, but native ELF emission must
reject them with the named float-format gap until an exact formatter exists.
This console binding does not claim general foreign-call, host-trampoline, or
cross-target support; see [the runtime contract](09-native-runtime-and-integration.md).

## Implementation work

- [x] Define program, module, type, function, basic-block, value, instruction, constant, global, import, and debug-location records with stable IDs. ([`compiler_lowered_ir.zi`](../cmd/compiler_lowered_ir.zi) defines modules, sources, types, constants, globals, functions, blocks, instructions, external functions and `LirOrigin` records at `LirVersion` 4; [lowered_exec_cases.zi](../tests/lowered_exec_cases.zi) and the round-trip codec in [zx_executable.sh](../tests/zx_executable.sh) retain them.)
- [x] Use SSA values and block parameters for register-like values. Represent mutable/address-taken storage through explicit memory operations. Specify block-parameter semantics so interpreter and native phi elimination agree. (SSA values, block parameters and load/store address operations are in [`compiler_lowered_ir.zi`](../cmd/compiler_lowered_ir.zi); [lowered_exec_cases.zi](../tests/lowered_exec_cases.zi) executes block-parameter transfers and [machine_ir_cases.zi](../tests/machine_ir_cases.zi) lowers them for native.)
- [x] Define integer widths and signed operations explicitly. Specify narrowing, sign/zero extension, overflow, shift counts, division edge cases, comparison, and conversion behavior from current Ziran semantics. ([`compiler_lowered_ir.zi`](../cmd/compiler_lowered_ir.zi) fixed-width operations; [lowered_exec_cases.zi](../tests/lowered_exec_cases.zi) covers narrowing, wrap, signed-minimum division, invalid shifts and mixed-width shifts.)
- [x] Define float32/float64 operations, bitcasts, NaN-sensitive comparisons, signed zero, conversions, and rounding assumptions. Do not introduce implicit fast-math permissions. ([lowered_exec_cases.zi](../tests/lowered_exec_cases.zi) covers checked float casts, float32 rounding, signed zero and payload-preserving NaN negation; [compiler_opt_float_cases.zi](../tests/compiler_opt_float_cases.zi) retains float folding behavior.)
- [ ] Extend the existing checked global/frame pointers, procedure values and
  immutable string byte/count representation to heap-derived pointers, slices,
  unions, vectors and the remaining string operations without assuming a fixed
  host pointer width. Keep code, data and capability references distinct.
  The M1 subset now includes checked frame-local aggregate/procedure storage,
  bounded derived slice addresses, byte equality, readonly data and typed
  byte-view/pointer-free aggregate capabilities. Unions, resizable vectors,
  general ownership and raw host pointers remain open or deliberately rejected.
- [ ] Separate portable logical type identity from target physical layout. Keep field/element access and layout queries symbolic until target layout is known; explicitly mark native-layout-dependent declarations and required host ABI descriptors.
- [x] Define loads, stores, copies, zeroing, address calculations, calls, branches, returns, traps, and unreachable terminators. Specify alignment, access width, alias assumptions, and permitted overlap for each memory operation. (Opcode set and alignment encoding in [`compiler_lowered_ir.zi`](../cmd/compiler_lowered_ir.zi), documented in [lowered-format.md](lowered-format.md). The portable contract deliberately still rejects `copy`/`zero` with `.Unsupported` ([`compiler_lowered_verify.zi`](../cmd/compiler_lowered_verify.zi)); executable coverage in [lowered_exec_cases.zi](../tests/lowered_exec_cases.zi).)
- [x] Preserve effect order with an explicit effect dependency or another equally checkable ordering contract. Calls, stores, cleanup, volatile/atomic operations, and fuel checkpoints must not move unsafely. (Effect numbers are verified in [`compiler_lowered_verify.zi`](../cmd/compiler_lowered_verify.zi) and preserved by every optimizer pass ([`compiler_opt.zi`](../cmd/compiler_opt.zi)); [compiler_opt_cases.zi](../tests/compiler_opt_cases.zi) retains exact Fuel consumption through optimization.)
- [x] Specify atomics and memory ordering only where supported by the language contract. Declare unsupported operations instead of interpreting ordinary loads as atomic. (The schema defines no atomic opcodes, and the verifier rejects forms outside the declared set with `.Unsupported` ([`compiler_lowered_verify.zi`](../cmd/compiler_lowered_verify.zi)); retained rejection fixtures in [lowered_exec_cases.zi](../tests/lowered_exec_cases.zi).)
- [x] Define source-origin IDs and transformation-origin chains, including generated cleanup, inlined calls, generic instances, host adapters, and native variants. (`LirOrigin` parent chains in [`compiler_lowered_ir.zi`](../cmd/compiler_lowered_ir.zi) survive optimization ([compiler_opt_cases.zi](../tests/compiler_opt_cases.zi)) and are restored by the debug attachment ([executable_bundle_cases.zi](../tests/executable_bundle_cases.zi)); native variant origins are produced by [`compiler_zir_lower.zi`](../cmd/compiler_zir_lower.zi).)
- [ ] Attach conservative optimization facts such as known ranges, alignment, readonly arguments, and ownership. Distinguish verifier-derived facts from producer assertions; unsafe producer claims cannot authorize code motion or unchecked access. Landed slice: verifier-derived pointer/range facts drive check elision under `--checks` ([`compiler_lowered_proven.zi`](../cmd/compiler_lowered_proven.zi); [proven_pointer_cases.zi](../tests/proven_pointer_cases.zi), [guarded_index_ranges.sh](../tests/guarded_index_ranges.sh), [trust_boundary_cases.zi](../tests/trust_boundary_cases.zi)). Producer-supplied ownership facts and facts that authorize motion without re-verification remain open.
- [x] Implement deterministic traversal and textual dumps. Stable IDs must not depend on pointer values, allocator order, timestamps, or nondeterministic hash iteration. (`zi2zx --emit lir` prints LIR through `AssemblyPrintLir`, the canonical owning codec serializes deterministically, and optimizer counters follow stored order ([`compiler_opt.zi`](../cmd/compiler_opt.zi)); [compiler_opt_cases.zi](../tests/compiler_opt_cases.zi) and the deterministic-output fixtures in [machine_ir_cases.zi](../tests/machine_ir_cases.zi) retain this.)
- [x] Implement a structural verifier for IDs, types, terminators, predecessor lists, argument arity, use-before-definition, dominance, block parameters, entry points, and reachable symbol signatures. ([`compiler_lowered_verify.zi`](../cmd/compiler_lowered_verify.zi), including dominator-tree computation; rejection fixtures in [lowered_exec_cases.zi](../tests/lowered_exec_cases.zi), verified-read enforcement in [zx_executable.sh](../tests/zx_executable.sh) and [executable_bundle_cases.zi](../tests/executable_bundle_cases.zi).)
- [x] Implement memory/effect and layout validation. Reject invalid offsets, contradictory alignments, malformed constant data, impossible call conventions, and illegal native-only operations in portable code. (Alignment and memory checks in [`compiler_lowered_verify.zi`](../cmd/compiler_lowered_verify.zi) (`.Alignment`, `.Unsupported`); calling-convention rejection retained in [machine_ir_cases.zi](../tests/machine_ir_cases.zi) and portable rejection in [lowered_exec_cases.zi](../tests/lowered_exec_cases.zi).)
- [x] Bound decoder allocation and graph work by checked input sizes and explicit resource budgets. Use iterative algorithms where attacker-controlled depth could exhaust the compiler stack. (`LirLimits` charging in the codec, verifier and optimizer ([portable-execution.md](portable-execution.md)); budget rejection retained in [lowered_exec_cases.zi](../tests/lowered_exec_cases.zi) and the container reader accounting in [executable_bundle_cases.zi](../tests/executable_bundle_cases.zi).)
- [x] Provide recoverable structured diagnostics with function, block, instruction, and source context. Library errors must not abort the host process or leak partially built programs. (`LirDiagnostic` carries function/block/instruction/value/origin context in [`compiler_lowered_verify.zi`](../cmd/compiler_lowered_verify.zi); failure-preserving owner semantics retained in [lowered_exec_cases.zi](../tests/lowered_exec_cases.zi) and [machine_ir_cases.zi](../tests/machine_ir_cases.zi).)
- [x] Represent required target features and Gap identifiers explicitly. The
  verifier rejects an operation outside the selected target contract; it never
  erases it, serializes it as a no-op, or substitutes a portable/native helper.
  (`TargetDescribe`/`TargetPreflight` rows with one gap-policy state and stable
  `assembly.gap.*` diagnostics in [`compiler_target.zi`](../cmd/compiler_target.zi);
  verifier `.Unsupported` rejection in [`compiler_lowered_verify.zi`](../cmd/compiler_lowered_verify.zi);
  registry coverage and every state in [machine_ir_cases.zi](../tests/machine_ir_cases.zi),
  row/code checks in [target_diagnostics.sh](../tests/target_diagnostics.sh).)

## Tests

The retained fixture is [lowered_exec_cases.zi](../tests/lowered_exec_cases.zi) (run through the repository's check discovery), with container-level verification retained in [executable_bundle_cases.zi](../tests/executable_bundle_cases.zi) and [zx_executable.sh](../tests/zx_executable.sh).

- Positive fixtures: straight-line arithmetic, diamonds, loops, nested loops, block parameters, memory-backed locals, indirect calls, and multiple return fields.
- Negative fixtures: cycles in value definitions, invalid dominance, duplicate IDs, wrong block arity, missing terminators, bad types, impossible layouts, effect-order violations, and malformed debug references.
- Determinism: rebuild equivalent programs under different allocation histories and compare serialized instruction content and text dumps.
- Lifetime: retain a verified lowered program after closing its original `.zir` owner; verify ownership is explicit and no borrowed storage becomes invalid.

Run builds and local tests through `build-queue --` with `DISPLAY` and `WAYLAND_DISPLAY` removed. This step requires no benchmark claims.

## Exit gate

All defined instruction forms have documented semantics and verifier coverage. A malformed program cannot become executable merely because it has a “checked” flag. Every unresolved required operation remains a visible Gap with a stable diagnostic. No new saved format relies on C struct padding or process pointers. The schema, verifier, and printer are usable independently of a specific CPU backend.

## Handoff

Deliver the schema, opcode/effect/layout tables, diagnostics, deterministic fixtures, and verifier API to steps 03–05. Record any operation whose semantics remain unresolved; do not fill the gap with generated source text.
