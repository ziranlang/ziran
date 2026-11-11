# Ziran execution contract

Status: current baseline plus decisions required by [step 01](01-contract-and-completion-matrix.md). This document is normative for the lowered portable IR planned in [step 02](02-lowered-ir-and-verifier.md); it does not claim that the replacement engine exists. The current implementation is the checked tree represented by [compiler IR](../cmd/compiler_ir.zi), the [portable VM](../cmd/zir/zir_vm_run.c), the [ZX native engine](../cmd/zir/zir_zx_native_lower.c), and their retained fixtures.

## Contract layers

1. **Checked language graph:** `.zi` or saved `.zir` produces one checked typed graph. Statements, expressions, source spans, foreign/effect facts, ownership facts, laws, and proof certificates remain in this layer; executable instruction text is not reparsed from statement text.
2. **Verified portable program:** lowering turns each reachable function into typed basic blocks with explicit control flow, effects, value definitions, portable layouts, call/cleanup regions, and capability imports. Verification rejects malformed structure, type mismatches, invalid dominance, impossible layouts, and missing effects before an image can be loaded.
3. **Immutable encoded image:** the `.zx` executable container owns immutable code, type/layout metadata, constants, entry identity, capabilities, assets, laws, and optional inspection metadata. It contains no host pointers, C structure padding, process addresses, or executable `.zir` requirement.
4. **Loaded instance:** one immutable image may create many instances. An instance owns mutable global slots, call stacks, owned allocations, host handles, serial execution ownership, fuel state, and failure state. Code and metadata are never mutated by execution.
5. **Target program:** a target description selects CPU architecture, ISA features, OS ABI, calling convention, endianness, pointer rules, and atomic/thread services. It cannot change portable arithmetic, ownership, call order, traps, or capability identity.
6. **Machine program and encoded object:** optimization and native lowering consume only the verified portable program and target description. Register allocation, physical stack frames, ABI call sequences, relocation, and encoding belong to these layers.

The backend APIs remain distinct: verify portable program, describe target, build machine program, encode image/object, load instance, run instance, inspect image or instance. No API accepts an unverified graph as executable code or exposes mutable process memory as program code.

## Numeric and scalar semantics

Current fixed integer types are `s8`, `u8`, `s16`, `u16`, `s32`, `u32`, `s64`, and `u64`. Abstract `int`/`integer` arithmetic currently uses the 64-bit operation policy in the VM helper, while pointer-sized `isize` and `usize` are outside the portable executable subset. Bit representations, rather than host signed arithmetic, define operations; [scalar helpers](../cmd/zir/zir_scalar.h) mask to the declared width and reinterpret signed payloads.

- Addition, subtraction, multiplication, signed negation/overflow, and unsigned wraparound wrap at the operand width. Unsigned intermediates are used on all targets so C behavior is defined.
- Signed minimum divided by `-1` wraps to itself; its remainder is zero. Integer division or remainder by zero is a failed execution/trap, not undefined behavior.
- Left shift masks/uses the destination width after an unsigned-width shift; signed right shift sign-extends. A negative count or a count at or beyond the operand width fails on every target.
- Narrowing casts truncate to the destination bit width; widening casts preserve or zero-extend according to source signedness. All 64 signed/unsigned conversion pairs are covered by generated boundary fixtures.
- Comparisons use the operand's signedness and width. Equality on floating NaN is false; float division may produce infinity or NaN. Float-to-integer conversion requires a value representable in the destination type; a nonfinite or out-of-range conversion fails.
- `float32` operations must round each float32 operation once. This is a current C/Go/native result and a required lowered-IR rule. The explicit current portable-VM gap for chained `float32` literals is recorded in [numeric conformance](../tests/numeric_conformance.json); step 02 must preserve the target rule rather than inherit that interpreter bug.
- Enums have a checked integer backing width; enum comparison and conversion follow that representation without assuming a native `int` rank.

The current stable numeric cases and their exact fixture are published by [capability reporting](../cmd/package_capabilities.zi) and described in [numeric conformance](../tests/numeric_conformance.json); [numeric semantics](../tests/numeric_semantics.sh) and the [width matrix](../tests/numeric_width_matrix.sh) cover source and saved IR through C, Go, and ZX.

## Portable layouts, values, and memory

The lowered IR needs a deterministic **logical layout** distinct from native ABI layout. It records scalar kind/width/signedness, field order, byte size, alignment, inline/packed eligibility, array length and element layout, union alternatives, enum backing, and ownership/borrow class. Equivalent checked declarations must produce equivalent metadata under different allocation histories. Native alignment, padding, bitfield, vector ABI, calling convention, and address representation are selected only in the target program.

Current VM behavior motivates but does not constrain the physical representation: small primitive-only records may occupy an inline value payload, packed arrays store fixed-width lanes with validity bits, and larger or reference-bearing records use named logical slots ([VM representation](../cmd/zir/zir_vm_internal.h)). These are interpreter implementation choices. The new portable image must expose logical offsets, not tagged C pointers or `Value` layouts.

- Scalars, immutable strings, fixed arrays, and copyable records have value semantics. Assignment and parameter passing copy logical values unless an owned storage class explicitly moves.
- Unions select one active alternative; reading the wrong alternative is a checked failure. Records containing references, opaque foreign members, or oversized representations retain explicit logical storage.
- Fixed arrays and vectors have checked indexing and length. Proven-range optimizations may remove a bounds check only while the verifier can reconstruct the same proof.
- `New(T)` allocates zero-initialized storage and returns a pointer class; `free(pointer)` releases it, ignores null, and rejects a freed or non-heap pointer. Types with owned vector storage cannot bypass cleanup. The current contract and physical-OOM boundary are documented in [heap allocation](../tests/heap_new.sh).
- Bundle pointer parameters are opaque host handles: they may be stored, compared with null/equal, and passed to matching capabilities. They cannot be dereferenced as arbitrary process addresses. Native C pointers retain native lifetime rules; this is a target boundary, not a portable aliasing contract.
- Immutable strings have byte length and may contain NUL. A `TextView` is a borrow of a place. Current C mutation observes borrowed bytes; Go and ZX snapshots isolate mutation, as reported by [capabilities](../docs/CAPABILITIES.md). Lowered IR must preserve each target's observable rule through an explicit borrow or snapshot operation rather than pretending one representation is universal.
- Portable state export deep-copies owned strings, arrays, records, enums, scalar lanes, and vectors. Pointers, slices, growable vector backing, and other borrowed/host resources are rejected.

## Ownership and cleanup

The checked graph records move and borrow facts. Lowering must make each effect explicit; a backend may not invent an extra copy, drop, or mutation.

- Direct owned `Vec` locals and parameters drop at block close, return, `break`, and `continue`, including every path through nested control flow. Vectors reachable through record fields and fixed arrays drop recursively. Globals remain shared and are not automatically released.
- A valid move source is a whole local binding, a fresh call result, or a plain record-member path rooted at a local aggregate. A direct move clears its source before a later drop. An aggregate move clears every reachable native vector field; the portable representation invalidates the corresponding logical slots. Moved storage is unreachable thereafter.
- Indexed and pointer-backed member paths and paths rooted at a global remain rejected as move sources. Assignment over an owned direct or aggregate-held vector is rejected. A fresh vector expression statement drops immediately; a member/index access must first bind the fresh result.
- `BuilderFinish` consumes the builder. Native C transfers nonempty bytes and retains them for process lifetime; Go and the VM copy an immutable snapshot. Empty native storage may be released. `Text` is an owned record wrapping a vector and follows aggregate move/drop rules.
- Live vector slices and text views restrict mutation, resizing, moving, replacement, and calls whose checked effect summary writes their backing. Checks are field-sensitive where represented: sibling fields remain mutable. Raw foreign declarations have no checked read/write summary and remain the caller's responsibility.
- Scoped cleanup is a region in the instruction graph, not a source-text transformation in the backend. Returns, breaks, continues, traps, and parallel-region exits follow the same explicit cleanup edge set. The current source pass and tests are in [cleanup lowering](../cmd/zir/zir_cleanup.c) and [owned values](../docs/OWNED_VALUES.md).

## Calls, effects, concurrency, and fuel

Call arguments evaluate left to right on every target. Named/default binding occurs after argument evaluation. Call results feed directly into the consuming call or operation, but earlier values are read before later calls can mutate them. Compound assignment reads its destination before evaluating the right operand. These rules are fixed by [call order](../tests/call_order.sh) and [compound order](../tests/compound_order.sh).

Every callable and foreign capability carries an explicit effect class: pure, observing, mutating, external/host, allocation, trap, parallel, or combinations accepted by the verifier. Effects drive reordering restrictions, borrow checks, fuel placement, host preflight, and optimization barriers. A pure operation can be controlled only if its result and trap behavior remain indistinguishable.

- Direct, procedure-value, bound-provider, and foreign calls all declare parameter and result layouts. Host marshalling validates signatures and returned record field identities/types. Mutable slice arguments are copied back only after a successful synchronous host call; overlapping mutable slice arguments are rejected.
- A call owns its local cleanup region. Evaluation temporaries remain rooted until the last consumer and call complete; nested strings and records are collected only at safe statement/call boundaries in the current VM.
- Portable VM calls nest until a known C stack floor or 128 calls where bounds are unknown; the CLI uses a large stack and reports exhaustion. Native and Wasm tiers use explicit stack/budget guards. Recursion exhaustion is a failed execution, not allocator or stack corruption.
- Ordinary native and portable runs are unbounded. A host may select a positive cumulative budget before global initialization. The portable VM counts statements; native and Wasm count function entries and loop iterations, with literal-bound loops charged at region entry. Workers in one `#parallel for` region spend from the shared remaining budget, and the run charges all successful iterations when the region ends. Current behavior and tests are documented in [ZX execution](../docs/ZX.md) and [bundle step limits](../tests/bundle_step_limit.sh).
- `#parallel for` currently runs forward C regions on worker threads; Go, ZX, and reverse regions run serially with a downgrade marker. Workers process ordered chunks, the caller waits, and the first worker failure a serial run could meet stops the region. Portable instances support concurrent execution only across distinct instances; each instance and host callback context has one executing owner.
- `#parallel_gpu for` is checked but has no device backend: every target uses CPU fallback. A failed bounds/fuel check or refused shader discards device results and reruns on CPU. No lowering may claim GPU execution.

## Traps, capabilities, and isolation

Runtime failures stop the current run and leave the instance marked failed: out-of-bounds access, invalid union access, integer divide by zero, invalid float-to-integer conversion, invalid shift, unreachable code, allocation/cleanup contract failure, stack/fuel exhaustion, host contract violation, or a capability call returning an invalid value. A trap runs no later user code except cleanup already required by the verified failure edge; it must not read freed storage or synthesize a result. Diagnostics retain a source location when inspection metadata is present.

Required host services are capability identities, not dynamic imports: module/function pairs with checked ABI and argument-read/effect metadata. A provider check must reject an unsupported program without running initializers, opening a window, or executing the entry. Missing bindings fail before entry. Validated native code is **not an operating-system sandbox**: malformed-input rejection and capability restrictions do not confine memory, syscalls, signals, or bugs in linked OS libraries. Untrusted execution still requires a process or comparable OS boundary.

The current native ZX path uses generated C and TinyCC on supported Linux/Android CPUs, with an explicit `zx.native` failure and no VM fallback. Browser pages use ahead-of-time linked native objects or the portable VM. These are baseline delivery facts; the planned direct native backend must preserve the same failure and capability model without TinyCC in the promised direct path.

## Debug and release policy

Semantic checks, effects, layouts, control-flow validity, capability ABI, and trap/cleanup edges are mandatory in debug and release. Release optimization may remove only checks it can prove redundant. Stripping names, source spans, variable provenance, law evidence, or disassembly metadata is an explicit encoding choice selected by the producer and recorded in image metadata. Stripped inspection data never changes executable semantics. A debug image may be larger and slower, but a release image cannot rely on unverified assumptions.

## Lowered IR representation decisions

Use typed virtual registers in basic blocks, not source trees and not physical registers. Registers are immutable except explicit mutable-place instructions; SSA-like value definitions must dominate uses. Keep source spans and provenance out of the execution datum, attached in parallel side tables. Functions and blocks need stable IDs, not object pointers. Constants are typed, canonical, and owned by the image.

The graph verifier must prove block termination, edge arity, dominance, type equality, move/borrow validity, cleanup region coverage, layout consistency, effect ordering, fuel edge placement, and capability signatures. It must also reject process pointers, target-specific sizes in portable code, duplicate IDs, cycles among value definitions, and metadata that references missing declarations.

### Concrete opcode categories

Do not assign numeric opcodes in this step. Reserve stable categories, each with typed operands and a separate effects word:

| Category | Required forms | Representation decision |
|---|---|---|
| Constants | scalar, string bytes/range, enum, null pointer, zeroed aggregate, typed SIMD lanes | Intern by canonical typed value; never reparse literal text at run time. |
| Data movement | copy, move, clear, zero-init, union select, view/borrow, snapshot | Destination and source carry logical layout and ownership class; moves clear source slots. |
| Integer arithmetic | add/sub/mul/neg, div/rem, and/or/xor/not, shifts, min/max if currently checked | Parameterize width and signedness; checked div/rem/shift trap classes are explicit. |
| Float arithmetic | add/sub/mul/neg/div/rem-if-supported, compare, classify, round, convert | Keep `f32` and `f64` distinct; no silent double intermediates. |
| Conversion | integer width/sign, integer/float, enum/integer, bitcast only where portable | Model checked range failure; reinterpretation cannot change logical type. |
| Aggregate memory | field read/write, fixed-array index, vector index, union field, slice bounds | Logical offsets and lengths only; target lowering selects ABI. |
| Heap and ownership | allocate-zero, free, retain/release hooks, cleanup region enter/exit | Explicit region terminator handles return, break, continue, and trap edges. |
| Control flow | conditional/unconditional branch, switch/complete enum, loop header, named break/continue, unreachable | One terminator per block; named control-flow IDs are resolved by verifier. |
| Calls | direct, procedure value, bound provider, foreign capability, host marshalling, tail call only if semantics survive | Arguments are explicit ordered operands; defaults/names are lowered before encoding. |
| Effects and fuel | checkpoint, budget set/consume, worker region enter/exit, CPU fallback | A checkpoint is observable and blocks moving an unbounded call/loop. |
| SIMD | lane insert/extract, element-wise arithmetic/compare/select, reductions currently supported | Lane type/count is portable; ISA lowering is target-only. |
| Text and collections | string length/index/equality, builder append/finish, vector operations currently supported | Preserve borrow versus snapshot and recoverable allocation failure. |
| Capability and instance | capability require/call, state export/import, instance replace boundaries | Capability identity and ABI checks precede entry/upgrade. |

Keep target-specific assembler, OS syscall, atomic, thread, ABI call, and platform-library operations out of the portable opcode stream. Encode them as declared capabilities or explicitly selected native extension records with target descriptions; unsupported combinations must fail at target preflight.

## Non-goals and open boundaries

- The replacement `.zx` format is intentionally incompatible. Its version is chosen against the actual current writer in step 05, not reserved here.
- Go-only opaque maps/types and C callback/assembly extensions do not become portable types by renaming. They remain target extensions with preflight rejection where the portable verifier cannot preserve them.
- Raw native FFI has no checked aliasing/memory-effect contract. Differently typed aliases and arbitrary host-backed aliases remain outside current guarantees.
- Cross-compilation, successful object encoding, a child-agent receipt, or a stored benchmark report does not prove target execution. Completion requires fresh evidence for the exact platform and fixture.
- RISC-V and every architecture beyond the maintained x86-64, 32-bit ARM, AArch64, and wasm32 rows are explicit future extensions, not implicit completion.
