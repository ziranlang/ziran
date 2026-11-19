# Learn from Lean 4: compiler and runtime (2026-11-18)

Analysis only: no Ziran source, build, or commit. Comparison of Lean 4's
compiler/runtime with Ziran's, ranked proposals first. Baseline: Alpha
receipt `wasm-tier-alpha-2026-11-15-native.json` and
`plans/2026-11-15-native-speed.md`; today's tree (native Records ~2.9x C,
engine ~3.4x, Wasm ~2x after the 2026-11-15 aggregate work).

## What Lean 4 does, briefly

Lean 4 is eager and purely functional; the runtime counts references, and the
compiler (LCNF pipeline) inserts explicit `inc`/`dec`, then proves most of
them unnecessary (borrow inference) and reuses dead constructor cells
(reset/reuse), so idiomatic functional code updates in place (FBIP). Code is
specialized per fixed function/instance argument (`@[specialize]`), inlined by
annotation kinds (`@[inline]`, `@[inline_if_reduce]`), boxings are elided
(tagged scalars, unboxed enums), and everything lowers to plain C compiled by
GCC/Clang. Concurrency is a runtime thread pool of Tasks.

## Ranked proposals

### 1. Prove the old value dead; update in place everywhere (Lean: reset/reuse + borrow)

Lean: before lowering, every constructor gets a `reset`/`reuse` contract;
at runtime `isShared` (RC==1) decides reuse vs fresh allocation, so
`Array.set` and list rebuilds mutate in place when unique.
Links: https://lean-lang.org/doc/reference/latest/Run-Time-Code/Reference-Counting/
(the `discardElems` IR example), https://arxiv.org/abs/1908.05647,
https://github.com/leanprover/lean4/blob/master/src/Lean/Compiler/LCNF/ResetReuse.lean
Ziran now: `Insert` lowers to copy-then-mutate unless a backend proves the
source dead. Native has storage coalescing and views
(`cmd/compiler_machine_aggregate.zi::Coalesce`, `cmd/compiler_x86_64.zi::AggregateInsert`);
Wasm has its own (`cmd/compiler_zx_wasm_plan.zi::CoalesceAggregates`, `PlanViews`);
the engine replays Insert as word/bytes handlers over frame plans
(`cmd/compiler_exec.zi`, `InsertWord`/`InsertBytes` ~L1269-1317); a
call-argument borrow/versioning pass was in flight on 2026-11-18
(untracked `bench/results/call-borrow-*.json` receipts).
Change: one LIR pass computing unique-use facts (single use + no effect or
possible trap between definition and last use + no borrowed edge, the rules
the borrow pass used), rewriting `Insert` into in-place member stores and
call arguments into borrowed parameters; engine, Wasm, zi2c and machine IR
consume the same facts instead of re-deriving them per backend.
Expected win: Records-shaped code (nested record/array updates, typical in
Kryon widgets) reaches <=1.5x C on engine, Wasm and C tiers; today engine 29.2
us / Wasm 17.0 us vs C 8.66 us on the wasm-tier Records kernel.
Risk: snapshot semantics when the old value is still observed (copy cycles
keep a temporary today); checked-token invalidation; parity suites must keep
hostile twins trapping.

### 2. Vec growth reuses storage when nothing borrows it (Lean: FBIP growth)

Lean: reuse analysis makes growth loops allocation-free: a uniquely referenced
array is extended in place; no copy per iteration
(https://pldi21.sigplan.org/details/pldi-2021-papers/7/Perceus-Garbage-Free-Reference-Counting-with-Reuse,
https://leanprover.github.io/functional_programming_in_lean/programs-proofs/summary.html).
Ziran now: HISTORICAL profile - the 67.7%-of-Vectors SIGPROF figure and the
`cmd/compiler_machine_elf.zi::WriteRuntime` resize-veneer reference date
from the 2026-11-15 plan and predate the storage-reuse/mremap work that now
lives in `cmd/compiler_x86_runtime.zi`. A fresh 557-sample SIGPROF report puts
NativeResize at 5.7%, the native image at 71.9%, host/libc at 28.1%; these are
worker samples, not a validated optimization gain, and a bounded NativeResize
realloc candidate is under test by the native lane. This proposal needs fresh
measurement before any win is claimed.
Change: geometric capacity growth in place when LIR facts show no live
borrowed view and no checked slot points into the allocation; copy only when
facts are unknown, on shrink, or at limits; stale aliases must still trap
after growth; failure keeps the old allocation live (plan fix 2, restated as
the Lean rule instead of a general allocator).
Expected win: unvalidated - the historical 1.5-3x bound no longer follows from
the fresh profile; re-measure after the native lane's realloc candidate lands.
Risk: moves checked-pointer version semantics; needs resize/stale-alias host
twins and the Vectors checksum 94905000 before landing.

### 3. Specialize constant arguments and devirtualize indirect calls (Lean: @[specialize])

Lean: `@[specialize]` duplicates a declaration per fixed function/instance
argument, cached per SCC; `@[inline_if_reduce]` inlines when the result avoids
a match. Links:
https://github.com/leanprover/lean4/blob/master/src/Lean/Compiler/LCNF/Specialize.lean,
https://leanprover-community.github.io/mathlib4_docs/Lean/Compiler/InlineAttrs.html
Ziran now: `cmd/compiler_opt_specialize.zi::Choose` duplicates only when an
entry *boolean* parameter flows into a branch condition (<=96 blocks, in-cycle
only); `CallIndirect` through a constant function address never folds.
Change: extend Choose to any entry parameter constant at every call site
(scalars first), and rewrite `CallIndirect` to `Call` when the callee is a
constant `FunctionAddress`, so inlining and selection see a direct call; keep
the growth budget and duplicate-name checks.
Expected win: widget dispatch/visitor code; 1.05-1.3x on dispatch-heavy
kernels; show it on the wasm-tier Hash kernel and the widget cohort in
`plans/2026-11-17-site-benchmark-cohort.md`.
Risk: code growth on fan-out call sites; the 4-round 25% growth budget in
`cmd/compiler_opt.zi::MemberRounds` is the guardrail.

### 4. Benefit-based inlining instead of one flat 48-instruction cap (Lean: inline kinds)

Lean: inlining is annotation-driven per kind, not a single size limit
(https://github.com/leanprover/lean4/blob/master/src/Lean/Compiler/InlineAttrs.lean).
Ziran now: `cmd/compiler_opt_inline.zi::Expandable` rejects callees above
`inline_calls = 48` (`cmd/compiler_opt.zi::LirOptDefaultOptions`); the leaf
pass covers one-operation callees; 4 rounds total.
Change: keep 48 as the cold default; allow a larger budget when the call site
sits in a loop (loop depth from `cmd/compiler_opt_loop.zi`) or its result
feeds a compare/branch; add `#inline`/`#noinline` attributes to override.
Expected win: hot widget helpers and the fib accumulator loop lose call/frame
overhead (fix 6 receipts showed scalar selection alone giving ~1.1x on
Sieve/Hash); Calls needs its repaired recursion fixture before claiming a
number.
Risk: compile time and image size; `plans/2026-11-17-code-size.md` is the
guardrail; attributes must not change checked semantics.

### 5. Keep generated C boring, reuse optimizer facts in zi2c (Lean: emitC)

Lean: codegen emits plain C with explicit RC calls and trusts GCC for the
rest (https://github.com/leanprover/lean4/blob/master/src/LeanIR.lean,
`Compiler.LCNF.emitC`).
Ziran now: `cmd/zi2c/zir_c_lower.c` lowers instruction by instruction;
aggregate facts from the optimizer are not reused, so update-heavy code can
keep copy sequences GCC cannot remove.
Change: when proposal 1's facts exist, zi2c shares slots and views so the
emitted C does straight member stores (the C hot envelope in the 2026-11-15
plan is 19 instructions; generated C should match that shape).
Expected win: C-tier parity on widget kernels without depending on our native
optimizer; simpler `-O2` output.
Risk: C aliasing assumptions; validate with `tests/local_owned_fields.sh` and
the standard benchmarks.

## Does not fit Ziran's model

- Runtime reference counting (`isShared`, inc/dec, RC headers): manual
  ownership and the >=C speed goal forbid per-object counters; static liveness
  (proposal 1) buys the same reuse free.
- Tagged scalars / boxed value words (Boxing reference): Ziran scalars are
  already unboxed machine words in registers; nothing to unbox.
- Closure unboxing: Ziran has no closures; function addresses are static and
  `CallIndirect` folding is proposal 3.
- Lean's Task runtime (thread pool, priorities, cancellation,
  https://lean-lang.org/doc/reference/latest/IO/Tasks-and-Threads/): standing
  owner rule forbids background services; `std/thread_linux.zi` pthreads with
  explicit joins already cover it.
- lambda_RC's formal borrow calculus as a source contract: too heavy for
  manual-safety Ziran; one-use/dead-value liveness captures the same wins.
- Perceus-style runtime reuse checks for immutable persistent structures:
  Ziran data is mutable by default; the copying cases that matter are
  proposals 1-2.

## Sources (accessed 2026-11-18)

- Counting Immutable Beans (Ullrich, de Moura, IFL 2019): https://arxiv.org/abs/1908.05647
- FBIP via Perceus (Reinking, Xie, de Moura, Leijen, PLDI 2021): https://pldi21.sigplan.org/details/pldi-2021-papers/7/Perceus-Garbage-Free-Reference-Counting-with-Reuse
- Reference manual, Run-Time Code (Boxing, Reference Counting): https://lean-lang.org/doc/reference/latest/Run-Time-Code/
- Reference manual, Tasks and Threads: https://lean-lang.org/doc/reference/latest/IO/Tasks-and-Threads/
- LCNF ResetReuse / Specialize / InlineAttrs: https://github.com/leanprover/lean4/tree/master/src/Lean/Compiler
- LCNF ExplicitRC: https://lean-lang.org/doc/api/Lean/Compiler/LCNF/ExplicitRC.html
- Functional Programming in Lean, summary: https://leanprover.github.io/functional_programming_in_lean/programs-proofs/summary.html
