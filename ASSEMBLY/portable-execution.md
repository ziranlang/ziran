# Portable lowered execution

Status: implemented. This engine is the portable instruction engine:
`libziran` embeds it, and `ziran run` and the embedding API execute
revision-28 `.zx` containers on it.

## Public API

`compiler_exec.zi` exports:

- `LirExecDefaultLimits() -> LirExecLimits`
- `LirExecPlanBuild(program, verify_limits, *plan) -> LirExecResult`
- `LirExecPlanFree(*plan)`
- `LirExecInstanceOpen(*plan, limits, identity, *instance) -> LirExecResult`
- `LirExecInstanceClose(*instance)`
- `LirExecRun(*instance) -> LirExecResult`
- `LirExecCall(*instance, function, arguments) -> LirExecResult`
- `LirExecResultCopy(*instance, output) -> bool`
- `LirExecProblemName(problem) -> string`

A plan verifies the complete `LirProgram`, computes type layouts, indexes every
function value slot, and computes global and frame layouts. It borrows the
program's slices; their backing storage must remain alive and unchanged until
`LirExecPlanFree`.

Plan build and instance open are reusable operations. They release an earlier
value before rebuilding or reopening. A failure leaves the destination closed
and not ready. `LirExecPlanFree` and `LirExecInstanceClose` are safe after
partial initialization and safe to call repeatedly.

The plan and instance structs must be zero-initialized before their first build
or open. This is automatic for ordinary Ziran locals; a generated-C caller uses
the normal `{0}` initialization.

An instance owns all mutable state: global bytes, derived-address provenance,
frame bytes and words, the explicit frame stack, the copied result, and
resource counters. `identity` must be nonzero and unique among instances
whose pointer values can meet. Reopening an instance reconstructs its globals
from the program initializers.

`LirExecCall` accepts scalar arguments in `bits` and record/array arguments in
`bytes`. `LirExecResultCopy` copies a record or array result from
instance-owned result storage.

## Execution model

The engine runs verified instructions only. It does not load `.zir`, evaluate
the source AST, evaluate generated C fragments, or fall back to another
evaluator. When the plan is built, each verified function is translated once
into compact code: one entry per instruction, specialized by operation and
width, with its operands' frame words and storage offsets resolved and each
edge's argument copies listed. One loop runs it. Constants, null, function
addresses and fuel need no code: a frame starts with those words written.
Common pairs run as one entry: a compare and the branch on it, an address
derivation and the load or store that is its only use, a field address that is
only the base of the next derivation, two word extracts, and an add before a
jump. Anything the compact code does not run itself (calls through function
values, host calls, allocation, resizing, traps) and every failure goes to the
reference implementation, which runs one verified instruction at a time;
`LirExecRunReference` runs a whole call that way.

Calls push bounded `LirExecFrame` records onto an explicit vector. Recursion
therefore consumes the configured frame budget rather than the host language
call stack. Each frame has packed storage for records and arrays and one word
per scalar value. Values carry no definedness state: the verifier proves every
use is dominated by its definition and has its operand's type, and values from
outside the plan (call arguments, host results, global images) are checked
where they come in. Block transfers whose arguments could overwrite each other
first copy every argument to frame scratch and then publish all block
parameters, so swaps and other parallel-transfer cycles retain their incoming
values; transfers proven free of overlap copy in order.

Writable globals are instance-local packed byte storage. Pointer values are
opaque tokens naming an instance identity and a checked global-derived
provenance entry. `field.address` derives a checked record member provenance
entry, while `element.address` checks a dynamic fixed-integer index against the
array's count and derives the element's provenance. Both require the exact
parent pointee type, and repeated equivalent derivations reuse a token;
load and store require an exact pointee type. Function addresses use a separate
nonzero function-index encoding and indirect calls recheck the full signature.

Integer arithmetic narrows after each instruction. Signed division implements
the minimum-value divided by `-1` result without invoking host overflow.
Division by zero and shift counts outside the left operand width are structured
failures. A shift count is read using its own verified integer type, including
mixed-width shifts. Numeric conversions implement the verified bool/integer and
fixed integer/float matrix with checked float-to-integer ranges. Floating
negation flips the IEEE sign bit, preserving signed zero and NaN payload bits.

Fuel is consumed only by explicit `fuel` instructions, making it a stable
semantic checkpoint independent of dispatch implementation. Work is charged
separately for instruction dispatch, copies, and aggregate/member traversal.
The compact code charges the work and fuel of a run of instructions (up to the
next one it hands over) when it enters it. When they do not fit, or anything
in the run fails, it gives back what the instructions it has not run were
charged and hands the run to the reference implementation from there, so a
limit or failure stops at the same instruction, with the same remaining work
and fuel, as charging instruction by instruction. Frames, live values, derived
addresses, and total value bytes have independent limits.

Every runtime failure reports a `LirExecProblem`. Failures at an executing
instruction also report function, block, instruction, source origin, remaining
work and fuel, and a trap payload where applicable. Repeated calls reset
transient execution state and counters while retaining the instance's globals.

## Verified coverage

The engine executes every operation and value type currently accepted by
`compiler_lowered_verify.zi`:

| Surface | Implemented forms |
| --- | --- |
| Types | void, bool, fixed signed/unsigned integers, float32/float64, pointers, fixed arrays, records, function addresses |
| Values | constant, null, global address, function address |
| Numeric | add, subtract, multiply, divide, integer remainder, negate, bit operations, shifts, all comparisons, convert, bitcast |
| Aggregate | aggregate, extract, insert |
| Memory | field/checked dynamic fixed-array element address, checked allocate with optional initializer, load, store over instance globals and frame allocations; `resize` heap blocks |
| Calls | direct and indirect portable calls, including bounded recursion |
| Control | fuel, trap, jump, branch, return, unreachable |

Array extraction/insertion accepts both verified literal ordinals and dynamic
fixed-integer indices. Dynamic indices preserve their declared signedness and
width, reject negative and out-of-bounds values before copying bytes, and retain
checks for empty arrays and zero-sized elements.

`Allocate` reserves pointee-sized storage for the executing frame and returns
an identity-checked pointer token bound to that frame and its generation. The
optional initializer must have the exact pointee type and is copied at
allocation time; without one, the new storage has the pointee's zero value.
Accepted pointees are any verified non-void value type: bool, fixed-width
integer, `f32`, `f64`, pointer, and aggregate/record/fixed-array values
(the verifier's `ValueType` rule in [`compiler_lowered_verify.zi`](../cmd/compiler_lowered_verify.zi));
[lowered_exec_cases.zi](../tests/lowered_exec_cases.zi) allocates and reads
back an aggregate pointee. Allocated pointers compose
with load and store; a token whose frame has returned, or whose identity no
longer matches, fails with `.Pointer`. Every execution allocates fresh storage,
including repeated execution of one instruction in loops and recursion, so
allocated pointer values are not stable across executions and are never
meaningful outside the instance that produced them. Total address slots,
including allocations, are bounded by the `addresses` limit.

The conformance cases cover narrowing and wrap, signed-minimum division,
division by zero, invalid shifts, mixed-width shifts, bool casts, checked float
casts, float32 rounding, signed zero, NaN comparisons and payload-preserving
negation, cyclic edge transfers, arrays and records, direct and indirect calls,
initialized and zero-default scalar and aggregate allocation, same-site loops and recursion,
active aliases, stale and reused-frame pointer failure, instance isolation,
pointer provenance, reuse and repeated close, all runtime limits, structured
origins, repeated failure, and verifier rejection.

The test driver also runs a hand-built executor oracle from source and
entry-pruned saved IR through generated C, generated Go, and bundles on the
engine.

## Exact current boundary

The verifier rejects `copy` and `zero`; it also rejects native-width integers,
vectors, capability-typed values, ordinary calls to external functions,
non-portable calling conventions, variadics, and non-writable global
addresses. Typed host capability calls (`HostCall` with the declared signature)
are supported and execute through `LirExecTypedHostCall`
([`compiler_lowered_verify.zi`](../cmd/compiler_lowered_verify.zi),
[`compiler_exec.zi`](../cmd/compiler_exec.zi)). Plan construction returns
`InvalidProgram` with the verifier diagnostic for rejected forms; they are not
silently skipped.

Heap blocks live in the instance-owned bytes beside host-returned storage. A
block's address names the block and its generation. `resize` and free end that
generation, so every address derived from the old block fails with `.Pointer`
afterwards, as a pointer into a returned frame does. The last block of the
owned bytes grows and shrinks in place; dead blocks at the end are given back.
Heap bytes count against `value_bytes`, and a request beyond it gives a null
result instead of a failure, so `VecPush` can report it.

Consequently this slice provides checked frame-local scalar/aggregate allocation and
typed heap blocks, but does not yet provide arbitrary byte memory,
host callbacks, native-width or SIMD values,
capability values, state serialization or
migration, thread-safe concurrent use of one instance, or parallel/SIMD
dispatch. The executable `.zx` reader, `ziran run`, and the embedding API
route bundles through this engine; the Wasm tier and native ELF output are
separate `zx2wasm` and `zx2native` lowering routes.

## Local verification

Run:

```sh
env -u DISPLAY -u WAYLAND_DISPLAY build-queue -- \
    make CHECK_FILTER=lowered_exec check
```

The focused test reports:

```text
lowered exec: verified engine and source/saved C, Go and bundle local tests passed
```

Performance comparison is intentionally absent here. The required before/after
`.zx` benchmark belongs to the completed integrated objective and must run on
the Alpha physical host, never on a desktop or VM.
