# Learn from Lean 4: combined review and shortlist (2026-11-18)

Analysis only: no Ziran source, build, test, commit, push, or helper work
produced this file. It merges the four 2026-11-18 research lanes into one
prioritized shortlist and a review verdict. Detail, per-proposal risk and full
source lists stay in the lane plans:

- compiler/runtime: `plans/2026-11-18-learn-from-lean-compiler.md`
- verification: `plans/2026-11-18-learn-from-lean-verification.md`
- language/elaboration: `plans/2026-11-18-learn-from-lean-language.md`
- tooling/developer workflow: `plans/2026-11-18-learn-from-lean-tooling.md`

Evidence rules used below: **measured** cites an existing receipt (Alpha
wasm-tier JSON, `bench/results/call-borrow-*.json`, SIGPROF sampling in
`plans/2026-11-15-native-speed.md`); **hypothesis** is an expected win that
must be shown on the named benchmark before landing. Every item is a
process-local build/run step. Nothing here proposes a permanent background
service; the one long-lived idea (the serve loop) is gated on owner-started
lifetime with an explicit Exit, per the standing no-daemon rule.

## Implementation status

- **IN PROGRESS (codex-ziran-wasm-widget-speed-3):** specialize constant
  arguments and devirtualize `CallIndirect` (compiler plan proposal 3) plus
  the loop-context inlining budget (proposal 4). Uncommitted working-tree
  edits: `cmd/compiler_opt_specialize.zi` (+99), `cmd/compiler_opt_inline.zi`
  (+78, including an `Expandable(..., maximum * 4)` loop-context call site),
  `cmd/zir/zir_load.c` (module-limit rework, not caching), and cases in
  `tests/compiler_opt_cases.zi`, `tests/module_root_capacity_cases.zi`,
  `tests/spec/zx_lowered_aggregates.zi`. Not completed, not measured yet.
- **IN PROGRESS (Lean tooling):** editor result diagnostics, including failure
  envelopes and full native metadata, are implemented in `cmd/edit.zi` and
  `cmd/zir/zir_diagnostic.{c,h}`. Fresh focused diagnostic local tests passed
  6/6 and semantic edit parity passed 1/1. Source is already committed by the
  preceding acting runtime; shared full suite/bootstrap and installation/push
  status remain unverified by this lane.
- Other research proposals remain proposals unless their owning lane reports
  implementation and local validation.

## Shortlist (small, feasible, ranked)

### 1. Structured diagnostics: expected/actual fields + editor result array (tooling 4 + language 3)

Effort S-M, lowest risk, immediate value for the LLM repair loop. One
accessor feeds text mode, JSON mode, and a `"diagnostics":[...]` array in the
`ziran edit` result (`cmd/edit.zi::ApplyRequest` currently returns only
path/applied/source), plus `expected_type`/`actual_type` fields beside the
prose in `check.type` (`cmd/zir/zir_check_expr.c:1210`). The rich model
already exists (`cmd/zir/zir_diagnostic.h::DiagnosticDetails`; JSON quickfix
alternatives in `cmd/zir/zir_diagnostic.c::report`, line 118); `docs/DIAGNOSTICS.md`
makes the code the stable identity, so types belong in fields. Validation:
`tests/diagnostic_format.sh` + `tests/diagnostic_metadata.sh` fixtures in both
modes; same diagnostic set in text and JSON.

### 2. Digest-keyed frontend check cache (tooling 2)

Effort M. Today every `check`/`build`/`run`/edit-validate re-parses and
re-checks the whole program set (`ProgramsLoadWithDefines_in_context`,
`cmd/zir/zir_load.c:1323`). Key = toolchain format version + module digest +
import digests + defines; persist via `ProgramWrite`/`ProgramRead`
(`cmd/zir/zir_serial.c:1695`/`:2013`); reuse at module/declaration boundaries
only (Lean's parser-atomicity warning). Two in-repo precedents set the
discipline: `cmd/compiler_backend_cache.zi` (`CacheMagic` line 14) and
`cmd/compiler_native_cache.zi` (format magic + digest refusal). Validation:
golden runs with/without cache produce identical diagnostics and IR;
invalidation cases beside `tests/native_cache_module_cases.zi`.

### 3. `ziran test` driver + unused-name lints (tooling 5 + language 6)

Effort S (driver; lints S per language 6: unused locals + imports, warnings-only, `cmd/zir/zir_check_program.c` plus two registry entries). The dispatch list (`cmd/package.zi::PkgDispatch`
line 216, usage line 825) has no `test`; the manifest `tool` delegation
(`item.manifest.project_tool`, package.zi:608) is the existing escape hatch.
Validation: cases in the `tests/formatter.sh` style; lint tried against
`cmd/` itself before enabling.

### 4. Vec growth reuses storage when nothing borrows it (compiler 2) — measured motivation

Historical SIGPROF sampling put 67.7% of Vectors samples in the then-current
native resize veneer (`plans/2026-11-15-native-speed.md`). The old
`compiler_machine_elf` resize-veneer reference is historical; the current
implementation is in `cmd/compiler_x86_runtime.zi` with class/mremap paths.
The compiler research lane reports a fresh 557-sample worker profile:
NativeResize 5.7%, native 71.9%, host 28.1%. These are worker samples, not a
validated before/after benchmark. Vectors native 1.5–3x remains an unvalidated
hypothesis; the historical share does not bound the current implementation.
Any growth change must clear stale-alias host twins and Vectors checksum
94905000 before landing.

### 5. Unique-use Insert-in-place + borrowed call arguments (compiler 1)

Largest Records/widget win but highest complexity (engine, Wasm, zi2c, machine
IR consume the same facts). Measured baseline: engine 29.2 us / Wasm 17.0 us
vs C 8.66 us on the wasm-tier Records kernel (Alpha receipt
`wasm-tier-alpha-2026-11-15-native.json`). Expected win (hypothesis):
Records-shaped code reaches <=1.5x C. A call-borrow/versioning exploration
already produced receipts (`bench/results/call-borrow-*.json`). Validation:
`tests/local_owned_fields.sh` + hostile twins keep trapping.

### 6. Proof-carried bounds elision under `--checks` (verification 1+2)

Extend `LirProvenBuild` (`cmd/compiler_lowered_proven.zi`) with
dominance-scoped slice/heap facts invalidated at free/resize/unknown calls,
and let `#law`/`#proof` reach `ProofKernel::DecideOrder`
(`cmd/compiler_proof_kernel.zi::CloseGoal`, line 287) for multi-variable index
guards; `MachineVerify::DirectProven` (`cmd/compiler_machine_verify.zi`)
stays conservative across any call/store that may resize. Applies only to
`--checks`/`#checks` builds; default builds already omit defensive checks
(`docs/USAGE.md`). Validation: `tests/proven_pointer_cases.zi`,
`tests/guarded_index_ranges.sh`, `tests/trust_boundary_cases.zi`,
`bench/wasm_tier.py` Vectors/Hash/Sieve under checks.

### 7. Language quick wins: reserved `zi_` prefix, parse recovery, ambiguity candidates (language 5, 2, 7; 8 verification-first)

S then M. Reject user-declared `zi_default_*` spellings and pin the one
generated-name convention (`cmd/zir/zir.c::FunctionDefaultHelperName`, line
1850) — capture-avoidance by prefix reservation; audit `std/` and `tests/`
first. Then statement-level recovery with a per-region error cap, skipping to
sync tokens, with new codes entering the `cmd/package_explain.zi`
`DIAGNOSTIC_CODES` registry (12 registry references today) that
`tests/explain.sh` scans. Validation: `tests/parser_recovery.sh` two-error
cases; clean-file `.zir` byte-identical. Language 7 lists every candidate
path in `module.ambiguous`/`package.ambiguous` messages; language 8
documents the literal default table (integer -> `s64`, float -> `float32`,
`#char` -> `s64`) verification-first — a checker disagreement there becomes
an owner decision, not a silent default.

## Gated or deferred

- **Persistent `ziran serve` session (tooling 1+3, cooperative cancellation
  tooling 3):** highest leverage for editor-driven checking but the largest new
  surface. Build only after item 2, only owner-started, with an explicit Exit;
  never an always-on daemon. Lean references: watchdog/worker architecture and
  `lake setup-file` dependency pre-build
  (https://github.com/leanprover/lean4/blob/master/src/Lean/Server/README.md),
  cooperative cancel tokens (`cancelTk? : Option IO.CancelToken`,
  src/Lean/Language/Basic.lean line 123).
- Deferred: polymorphic-default postponement (language 1), `using ... in` and
  data-scope `using` (language 4), zi2c reuse of optimizer facts (compiler 5),
  bounded translation validation (verification 3), `partial`/`unsafe` tainting
  for untrusted bundles (verification 4), co-located structural laws
  (verification 5).

## Poor fits (do not adopt)

From all four lanes: runtime reference counting / RC headers; tagged scalars
and boxed value words; closure unboxing (no closures); Lean's Task thread pool
and any permanent background service (`std/thread_linux.zi` pthreads with
explicit joins suffice); lambda_RC's formal borrow calculus as a source
contract; Perceus-style runtime reuse checks for immutable structures; syntax
extension/notation commands; type classes and instance-implicit search;
coercion class chains; auto-bound implicits; dependent types or proof terms in
the checked language; mandatory proof-carrying indexing; `termination_by`
proofs for all procedures; invoking Lean or an SMT solver inside `ziran` or
`.zx` loaders; trusting cached proof bits without re-check (`docs/LAWS.md`);
and for now: multi-process worker isolation, intra-command incremental
elaboration, remote artifact caches (see lane plans for revisit conditions).

## Review verdict on the four lane plans

- **Verified against today's tree:** every Ziran symbol/line sampled from the
  four plans exists as cited — compiler (`Choose`, `Expandable`,
  `inline_calls = 48` at `cmd/compiler_opt.zi:2102` inside
  `LirOptDefaultOptions` at :2099, `WriteRuntime`, `Resize`, zi2c lower),
  verification (`LirProvenBuild`, `MachineVerifyAllocated` :1408, `DirectProven`,
  `CloseGoal`, `EvaluateTheorem`, `ValidateReachable`, `law_waive`,
  `proofs/Scalar.lean`, `proofs/Oracle.lean`), language (`ExpandCallDefaults`
  :156, `FunctionDefaultHelperName` :1850, `DIAGNOSTIC_CODES`, recovery test
  files), tooling (`ProgramsLoadWithDefines` :1413, edit protocol, diagnostic
  emitter, cache magics, serializer, `zir_document.h`). Cited receipts exist
  (`bench/results/call-borrow-*.json`, wasm-tier Alpha receipt,
  `plans/2026-11-15-native-speed.md`).
- **Verified against primary Lean sources today:** `cancelTk?` field
  (Language/Basic.lean:123), `registerBuiltinIncrementalTactic`
  (Language/Lean.lean), watchdog + per-file workers and `lake setup-file`
  (Server README lines 29/38/46), Lake `serve`/`shake`/`cache get|put`/
  `testDriver`/`lintDriver`/`LAKE_NO_CACHE` (reference page).
- **Measured vs hypothesis:** lane-plan baselines and the historical 67.7% veneer
  share are receipt-backed measurements of their original revisions; all "expected win" numbers (<=1.5x C
  Records, Vectors 1.5-3x, 1.05-1.3x dispatch, ~1.1x inline) are hypotheses
  that require the named benchmarks before landing. Nothing in this review
  re-measured them.
- **Corrections:** none required. No factual errors found in any lane plan,
  including the tooling plan. No sibling plan was edited; anything later goes
  through the coordinating task first.
- **Flag:** compiler proposals 3-4 are partially implemented in the
  uncommitted working tree (see Implementation status) — the lane plan's text
  still describes them as proposals; treat the status section here as current.

- **Reconciliation:** the language lane expanded 232->468 lines and five->
  eight proposals; numbering 1-5 is unchanged, so every cross-reference in
  this file stays valid. New proposals verified in today's tree: lint codes
  `lint.cast`/`lint.char`/`lint.bool` (cmd/zir/zir_check_program.c:782-818),
  `#must` (docs/LANGUAGE_DIRECTION.md), zero `unused` codes in the diagnostic
  registry, `module.ambiguous`/`package.ambiguous` present, literal-default
  prose at docs/LANGUAGE_DIRECTION.md:157/285. P6 merges into shortlist 3,
  P7 into 7; P8 stays verification-first documentation. Language plan open
  questions (P1 `.zir` name stability, P2 error cap, P6 import-noise, P8
  float default) remain coordinator/owner decisions.

## Remaining implementation outside research scope

Completing and measuring the in-progress speed-3 work; then items 1-3 of the
shortlist are the smallest self-contained next steps, each gated on its named
validation; items 4-6 need the proof/fact plumbing from compiler proposal 1;
the serve loop waits for an explicit owner go-ahead. This file authorizes no
implementation by itself.

## Sources (accessed 2026-11-18)

- Lean server architecture, workers, `lake setup-file`:
  https://github.com/leanprover/lean4/blob/master/src/Lean/Server/README.md
- Snapshot framework, cancel tokens:
  https://github.com/leanprover/lean4/blob/master/src/Lean/Language/Basic.lean
- Incremental reuse guards and parser atomicity:
  https://github.com/leanprover/lean4/blob/master/src/Lean/Language/Lean.lean
- Counting Immutable Beans (RC/reset-reuse/FBIP): https://arxiv.org/abs/1908.05647
- Perceus garbage-free RC with reuse (PLDI 2021):
  https://pldi21.sigplan.org/details/pldi-2021-papers/7/Perceus-Garbage-Free-Reference-Counting-with-Reuse
- Lean reference, Run-Time Code (boxing, RC):
  https://lean-lang.org/doc/reference/latest/Run-Time-Code/
- Lean reference, Lake (traces, caches, serve, test/lint/shake):
  https://lean-lang.org/doc/reference/latest/Build-Tools-and-Distribution/Lake/
- LCNF Specialize / InlineAttrs / ResetReuse sources:
  https://github.com/leanprover/lean4/tree/master/src/Lean/Compiler
- Lean 4 paper (server continuously analyzing source): https://lean-lang.org/papers/lean4.pdf
- Lean language reference (elaboration, instances, namespaces, syntax):
  https://lean-lang.org/doc/reference/latest/
- Theorem Proving in Lean 4 (type classes, GetElem tactics):
  https://lean-lang.org/theorem_proving_in_lean4/
- Metaprogramming in Lean 4 (hygiene): https://leanprover-community.github.io/lean4-metaprogramming-book/main/06_macros.html
- Ziran lane plans: the four `plans/2026-11-18-learn-from-lean-*.md` files.
