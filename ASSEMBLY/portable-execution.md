# Portable lowered execution

Status: implemented as an isolated lowered-IR engine. This is not the `.zx`
cutover, does not replace the existing bundle evaluator, and makes no whole-plan
completion or performance claim.

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
frame bytes, value-validity state, the explicit frame stack, the copied result,
and resource counters. `identity` must be nonzero and unique among instances
whose pointer values can meet. Reopening an instance reconstructs its globals
from the program initializers.

`LirExecCall` accepts scalar arguments in `bits` and record/array arguments in
`bytes`. `LirExecResultCopy` copies a record or array result from
instance-owned result storage.

## Execution model

The engine indexes verified instructions directly. It does not load `.zir`,
evaluate the source AST, evaluate generated C fragments, or fall back to
another evaluator.

Calls push bounded `LirExecFrame` records onto an explicit vector. Recursion
therefore consumes the configured frame budget rather than the host language
call stack. Each frame has packed value storage plus validity bytes. Block
transfers first copy every edge argument to frame scratch and then publish all
block parameters, so swaps and other parallel-transfer cycles retain their
incoming values.

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
Frames, live values, derived addresses, and total value bytes have independent
limits.

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
Accepted pointees are bool, fixed-width integer, `f32`, `f64`, and pointer
values. Aggregate pointees fail the `Allocate` operand rule, while pointers to
function types are rejected by the type verifier. Allocated pointers compose
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
initialized and zero-default scalar allocation, same-site loops and recursion,
active aliases, stale and reused-frame pointer failure, instance isolation,
pointer provenance, reuse and repeated close, all runtime limits, structured
origins, repeated failure, and verifier rejection.

The test driver also runs a hand-built executor oracle from source and
entry-pruned saved IR through generated C, generated Go, and the portable VM.

## Exact current boundary

The verifier rejects `copy`, `zero`, and `host.call`; it also rejects
native-width integers, vectors, capabilities,
external functions, non-portable calling conventions, variadics, and
non-writable global addresses. Plan construction returns `InvalidProgram` with
the verifier diagnostic for these forms. They are not silently skipped.

Heap blocks live in the instance-owned bytes beside host-returned storage. A
block's address names the block and its generation. `resize` and free end that
generation, so every address derived from the old block fails with `.Pointer`
afterwards, as a pointer into a returned frame does. The last block of the
owned bytes grows and shrinks in place; dead blocks at the end are given back.
Heap bytes count against `value_bytes`, and a request beyond it gives a null
result instead of a failure, so `VecPush` can report it.

Consequently this slice provides checked frame-local scalar allocation and
typed heap blocks, but does not yet provide arbitrary byte memory,
host callbacks, native-width or SIMD values,
capability values, state serialization or
migration, thread-safe concurrent use of one instance, or parallel/SIMD
dispatch. Serialization, CLI selection, `.zx` integration, source lowering,
x64 comparison, and cutover remain separate integration work.

## Local verification

Run:

```sh
env -u DISPLAY -u WAYLAND_DISPLAY build-queue -- \
    sh tests/lowered_exec.sh build/bin/ziran
```

The focused test reports:

```text
lowered exec: verified engine and source/saved C, Go and VM local tests passed
```

Performance comparison is intentionally absent here. The required before/after
`.zx` benchmark belongs to the completed integrated objective and must run on
the Alpha physical host, never on a desktop or VM.
