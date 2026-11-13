# Verified lowered-IR optimization pipeline

Status: bounded scalar pipeline, mandatory before experimental ZX production.
Source `--opt` selects optimized LIR inspection and source optimization reports;
ZX input is never reoptimized. This module does not select target
instructions, allocate registers, or claim a performance result.

The implementation is
[`cmd/compiler_opt.zi`](../cmd/compiler_opt.zi). It consumes a verified
`LirProgram`, builds an independent owned copy, runs enabled transformations in
a fixed order, verifies the program after every enabled pass, and exposes the
verified result for inspection or later execution.

## Public API

```zi
LirOptDefaultOptions :: () -> LirOptOptions
LirOptBuild :: (owner: *LirOptOwner, program: LirProgram, limits: LirLimits,
    options: LirOptOptions) -> LirOptResult
LirOptProgram :: (owner: *LirOptOwner) -> LirProgram
LirOptFree :: (owner: *LirOptOwner)
```

`LirOptOwner`, `LirOptOptions`, `LirOptPassCounters`, `LirOptCounters`,
`LirOptResult`, and `LirOptProblem` are ordinary named exported records and
enums. Generated C consumers use normal module-prefixed symbols and records.
The default options enable constant folding, copy propagation, CFG
simplification, and dead-instruction elimination with at most eight folding
rounds.

The input is immutable. `LirOptBuild` first verifies it under the supplied
limits and serializes it through the canonical owning lowered-IR codec before
changing any instruction. The result therefore does not borrow a module name,
function name, type member list, operand list, edge argument, constant, source,
or origin from the caller.

`LirOptProgram` returns a view borrowed from the owner. The view and every
nested slice or string expire at the next build or free on that owner. Building
again from that same borrowed result is supported because the next owned copy
is encoded before the preceding owner storage is released. A failed build
empties a non-null owner. Empty and repeated frees are harmless. A null owner
returns `Owner`.

## Pass order and verification boundaries

The order is fixed and deterministic:

1. Verify the immutable input.
2. Fold exact scalar constants.
3. Propagate proven identity copies.
4. Simplify constant control flow and remove newly unreachable blocks.
5. Remove unused, nontrapping pure instructions.
6. Verify and expose the final owned program.

Each enabled numbered pass calls `LirVerify`, even when it reports no change.
The folding pass re-encodes its appended constant table through the canonical
codec, so no temporary vector or local slice escapes. Structural rewrites
compact values, block parameters, operands, edge arguments, block IDs, entry
IDs, and instruction result IDs together. Every remaining declared SSA value
continues to have exactly one definition.

`LirOptResult.diagnostic` is the authoritative input or pass-verification
diagnostic on failure. Successful results contain the exact input/output
function, block, value, and instruction counts. The four per-pass counter
records separately report runs and changed instructions, values, blocks, and
constants. These counters follow stored function/block/instruction order and do
not depend on allocator order or hashes.

## Implemented transformations

Constant folding implements exact boolean and fixed-width integer arithmetic,
bit operations, negation, comparisons, same-width numeric bitcasts,
boolean/integer conversions, and integer widening or narrowing. Integer
overflow is masked to the declared width. Signed comparison, division,
remainder, and right shift use the portable engine's two's-complement rules.
Division and remainder fold only with a nonzero divisor. A shift folds only
when its count is in range. Unsafe constant divide and shift instructions stay
in the program and retain their traps. Float bitcasts preserve the bits, but
float arithmetic and numeric float conversions are not folded.

Copy propagation currently proves only same-type `Bitcast` identities. It
rewrites every dominated operand and edge argument to the original value.
Dead-instruction elimination then removes the unused identity instruction and
compacts its value ID. It does not guess that conversions or block-parameter
cycles are copies.

CFG simplification turns a branch into a jump only when its condition is a
verified boolean `Constant`. It keeps the selected edge and its arguments,
computes reachability from the function entry, removes only blocks that are no
longer reachable, and remaps every surviving block and value reference.
Dynamic branches, loop headers, backedges, and phi-like block-parameter cycles
are retained.

Dead-instruction elimination reaches a fixed point for unused `Constant`,
`Null`, add/subtract/multiply, integer bit operations, negate, comparisons,
`Bitcast`, `Aggregate`, and immediate-index `Extract` and `Insert` results.
These operations are pure and nontrapping under the current verifier and
portable engine. Dynamic-index `Extract` and `Insert` stay in the program
because their bounds checks can trap, including when their result is unused.

## Preserved behavior

No pass changes the order, count, immediate, or effect number of `Fuel`
instructions. Calls, indirect calls, host calls, loads, stores, allocation,
copy, zeroing, traps, and all other effectful instructions remain ordered.
Unused divide, remainder, shift, and conversion instructions remain because
they can trap. Address operations are retained conservatively. An instruction
that is folded or changed from branch to jump keeps its source origin. Retained
instructions, functions, sources, and origin chains otherwise remain
byte-for-byte data-equivalent through the owning codec.

There is no fast math, reassociation, speculative execution, speculative
memory access, effect motion, or synthetic float behavior.

## Resource bounds

`LirLimits` bounds each input/pass verification, the owned decoded program,
table sizes, instruction/value/block counts, and codec decode. Independently,
the optimizer charges its own deterministic table visits against
`limits.work`. `LirOptResult.work` is only this optimizer-accounted work. It
does not include verifier or codec visits, elapsed time, CPU instructions, or
performance.

Every explicit optimizer scratch vector and encoded buffer is charged against
`limits.bytes`. `LirOptResult.scratch_bytes` is the cumulative logical scratch
allocation charged during the build, including storage already released. It is
not current or peak RSS and does not include the separately bounded owned
decode. Counts and size arithmetic are checked before allocation, folding
rounds are capped at 64, and allocation failure is recoverable.

The codec and verifier retain their own documented fixed format, allocation,
and work bounds. Optimization does not recursively traverse
attacker-controlled SSA or CFG depth.

## Inspection

Keep the original `LirProgram` for the before view and call
`LirOptProgram(owner)` for the after view. Compare the result counters first,
then walk functions, blocks, instructions, origins, and effects in stored
order. The owner view can also be passed directly to `LirWrite`,
`LirExecPlanBuild`, or a native encoder after inspection.

The pass manager does not print dumps or timing internally. A CLI integration
can render the two explicit views and the deterministic counters without
giving the optimizer filesystem or terminal ownership.

## Explicitly unimplemented passes

The following are not represented by placeholder counters or no-op names:

- float arithmetic folding and float numeric-conversion folding;
- block-parameter coalescing, jump threading, and general block merging;
- range propagation, redundant-check removal, common subexpression
  elimination, and load/store optimization;
- inlining, loop transformations, vectorization, or fuel-aware code motion;
- machine IR, instruction selection, register allocation, spilling, and ABI
  lowering.

Those transformations need separate semantic proofs and verifier boundaries.
Their absence is visible rather than inferred from a generic optimization
level.

## Local test

The retained Ziran fixture performs before/after portable-engine execution,
including loops and block parameters, aggregates, function values, divide and
shift traps, exact Fuel consumption, limits, invalid input, repeated builds,
failed builds, and repeated free. Its launcher runs source and saved IR through
generated C, generated Go, and explicit portable VM execution:

```sh
env -u DISPLAY -u WAYLAND_DISPLAY build-queue -- \
    sh tests/compiler_opt.sh build/bin/ziran
```

Performance compilation and measurement are separate work and run only on
Alpha's physical host.
