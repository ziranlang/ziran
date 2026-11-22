# Learn from Lean 4: review verdict (2026-11-18)

Study outcome for the four 2026-11-18 research lanes (compiler/runtime,
verification, language/elaboration, tooling). The shortlist's open items live
in plans/open-work.md; landed items are listed here.

Evidence rule: a proposal is **measured** only with a receipt on the named
benchmark. Expected-win numbers (Records <=1.5x C, Vectors 1.5-3x, dispatch
1.05-1.3x, inline ~1.1x) stay hypotheses until then.

## Adopted

- Structured diagnostics: `expected_type`/`actual_type` fields beside the
  prose and the editor result array (docs/DIAGNOSTICS.md), with editor result
  diagnostics in `cmd/edit.zi` and `cmd/zir/zir_diagnostic.{c,h}`.
- Specialized constant arguments, `CallIndirect` devirtualization, and the
  loop-context inlining budget (`cmd/compiler_opt_specialize.zi`,
  `cmd/compiler_opt_inline.zi`).

## Baselines

- Engine 29.2 us / Wasm 17.0 us vs C 8.66 us on the wasm-tier Records kernel
  (bench/results/call-borrow-*.json,
  bench/results/wasm-tier-alpha-2026-11-15-native.json).
- Historical SIGPROF sampling put 67.7% of Vectors samples in the then-current
  native resize veneer. The current implementation is
  `cmd/compiler_x86_runtime.zi`; its fresh 557-sample worker profile reads
  NativeResize 5.7%, native 71.9%, host 28.1%.

## Poor fits (do not adopt)

Runtime reference counting or RC headers; tagged scalars and boxed value
words; closure unboxing; Lean's Task thread pool or any permanent background
service (`std/thread_linux.zi` pthreads with explicit joins suffice);
lambda_RC's formal borrow calculus as a source contract; Perceus-style reuse
checks for immutable structures; syntax extension or notation commands; type
classes and instance-implicit search; coercion class chains; auto-bound
implicits; dependent types or proof terms in the checked language; mandatory
proof-carrying indexing; `termination_by` proofs for all procedures; invoking
Lean or an SMT solver inside `ziran` or `.zx` loaders; trusting cached proof
bits without re-check; for now: multi-process worker isolation,
intra-command incremental elaboration, and remote artifact caches.

## Sources (accessed 2026-11-18)

Lean 4 server, Language, and Lake sources and reference; the LCNF compiler
sources; Counting Immutable Beans (arXiv:1908.05647); Perceus (PLDI 2021).
