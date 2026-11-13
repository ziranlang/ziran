# Step 02 — Implement lowered portable IR and its verifier

Status: in progress; [typed scalar/aggregate schema and verifier](README.md), including checked global-derived pointers and procedure values. Complete memory, capability and language coverage remain pending. Depends on [step 01](01-contract-and-completion-matrix.md). Next: [complete language lowering](03-language-lowering.md).

The schema carries the [gap policy](gap-policy.md) through target preflight:
unsupported required operations and target extensions have stable identifiers
and cannot be rewritten as calls into another engine or backend.

## Objective

Create a structured, serializable representation of executable operations below `.zir`. It must support efficient interpretation and native lowering while retaining useful optimization and debugging facts. Do not make portable instructions expose physical registers, host addresses, or a particular calling convention.

## Existing integration points

- [Checked compiler records](../cmd/compiler_ir.zi).
- [Current serialization module](../cmd/compiler_serial.zi) and [reader integration](../cmd/zir/zir_serial.c).
- [Scalar semantic helpers](../cmd/zir/zir_scalar.h).
- [Current portable verifier](../cmd/zir/zir_vm_verify.c).
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

- [ ] Define program, module, type, function, basic-block, value, instruction, constant, global, import, and debug-location records with stable IDs.
- [ ] Use SSA values and block parameters for register-like values. Represent mutable/address-taken storage through explicit memory operations. Specify block-parameter semantics so interpreter and native phi elimination agree.
- [ ] Define integer widths and signed operations explicitly. Specify narrowing, sign/zero extension, overflow, shift counts, division edge cases, comparison, and conversion behavior from current Ziran semantics.
- [ ] Define float32/float64 operations, bitcasts, NaN-sensitive comparisons, signed zero, conversions, and rounding assumptions. Do not introduce implicit fast-math permissions.
- [ ] Extend the existing checked global/frame pointers, procedure values and
  immutable string byte/count representation to heap-derived pointers, slices,
  unions, vectors and the remaining string operations without assuming a fixed
  host pointer width. Keep code, data and capability references distinct.
  The M1 subset now includes checked frame-local aggregate/procedure storage,
  bounded derived slice addresses, byte equality, readonly data and typed
  byte-view/pointer-free aggregate capabilities. Unions, resizable vectors,
  general ownership and raw host pointers remain open or deliberately rejected.
- [ ] Separate portable logical type identity from target physical layout. Keep field/element access and layout queries symbolic until target layout is known; explicitly mark native-layout-dependent declarations and required host ABI descriptors.
- [ ] Define loads, stores, copies, zeroing, address calculations, calls, branches, returns, traps, and unreachable terminators. Specify alignment, access width, alias assumptions, and permitted overlap for each memory operation.
- [ ] Preserve effect order with an explicit effect dependency or another equally checkable ordering contract. Calls, stores, cleanup, volatile/atomic operations, and fuel checkpoints must not move unsafely.
- [ ] Specify atomics and memory ordering only where supported by the language contract. Declare unsupported operations instead of interpreting ordinary loads as atomic.
- [ ] Define source-origin IDs and transformation-origin chains, including generated cleanup, inlined calls, generic instances, host adapters, and native variants.
- [ ] Attach conservative optimization facts such as known ranges, alignment, readonly arguments, and ownership. Distinguish verifier-derived facts from producer assertions; unsafe producer claims cannot authorize code motion or unchecked access.
- [ ] Implement deterministic traversal and textual dumps. Stable IDs must not depend on pointer values, allocator order, timestamps, or nondeterministic hash iteration.
- [ ] Implement a structural verifier for IDs, types, terminators, predecessor lists, argument arity, use-before-definition, dominance, block parameters, entry points, and reachable symbol signatures.
- [ ] Implement memory/effect and layout validation. Reject invalid offsets, contradictory alignments, malformed constant data, impossible call conventions, and illegal native-only operations in portable code.
- [ ] Bound decoder allocation and graph work by checked input sizes and explicit resource budgets. Use iterative algorithms where attacker-controlled depth could exhaust the compiler stack.
- [ ] Provide recoverable structured diagnostics with function, block, instruction, and source context. Library errors must not abort the host process or leak partially built programs.
- [ ] Represent required target features and Gap identifiers explicitly. The
  verifier rejects an operation outside the selected target contract; it never
  erases it, serializes it as a no-op, or substitutes a portable/native helper.

## Tests

Add `tests/lowered_ir_cases.zi` and `tests/lowered_verifier_cases.zi`, with shell launchers following the repository's existing check discovery.

- Positive fixtures: straight-line arithmetic, diamonds, loops, nested loops, block parameters, memory-backed locals, indirect calls, and multiple return fields.
- Negative fixtures: cycles in value definitions, invalid dominance, duplicate IDs, wrong block arity, missing terminators, bad types, impossible layouts, effect-order violations, and malformed debug references.
- Determinism: rebuild equivalent programs under different allocation histories and compare serialized instruction content and text dumps.
- Lifetime: retain a verified lowered program after closing its original `.zir` owner; verify ownership is explicit and no borrowed storage becomes invalid.

Run builds and local tests through `build-queue --` with `DISPLAY` and `WAYLAND_DISPLAY` removed. This step requires no benchmark claims.

## Exit gate

All defined instruction forms have documented semantics and verifier coverage. A malformed program cannot become executable merely because it has a “checked” flag. Every unresolved required operation remains a visible Gap with a stable diagnostic. No new saved format relies on C struct padding or process pointers. The schema, verifier, and printer are usable independently of a specific CPU backend.

## Handoff

Deliver the schema, opcode/effect/layout tables, diagnostics, deterministic fixtures, and verifier API to steps 03–05. Record any operation whose semantics remain unresolved; do not fill the gap with generated source text.
