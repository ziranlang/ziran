# Learn from Lean 4: language and elaboration (2026-11-18, rev 2)

Analysis only: no Ziran source, build, test, commit, or helper work. This is
the language/elaboration lane of the learn-from-Lean pass; the compiler and
runtime lane is `plans/2026-11-18-learn-from-lean-compiler.md` and the
verification lane is `plans/2026-11-18-learn-from-lean-verification.md`.
Every Ziran reference was read in today's tree; every Lean claim points to a
primary Lean document or the lean4 sources. No measured wins are claimed —
expected benefits below are arguments, not benchmarks.

## Summary

| # | Proposal | Lean mechanism | Effort | Primary win |
|---|----------|----------------|--------|-------------|
| 1 | Postponed polymorphic-default elaboration | metavariable postponement | M | closes a documented language gap |
| 2 | Multi-error statement-level parse recovery | per-parser `recoverFn` + loop skipping | M | LLM loop sees all errors per run |
| 3 | Typed, explained diagnostics | elaboration metadata + Error Explanations | S | tools consume types, not prose |
| 4 | `using` completion (`using ... in`, data-scope, `hiding`) | bounded `open` shapes | M | last name-ergonomics gap |
| 5 | Reserved generated-name prefixes | macro scopes (hygiene) | S | capture-proof generated names |
| 6 | Unused local/import lints | unused-variables linter | S | dead-name feedback without `#must` |
| 7 | Ambiguity diagnostics list candidates | `ResolveName`/`ensureNonAmbiguous` | S | actionable name-conflict errors |
| 8 | Documented untyped-literal default table | default instances | S | predictable literal inference |

## How Lean 4's frontend works, topic by topic

### Elaboration pipeline

Lean processes one command (top-level declaration) at a time through parse →
macro expansion → elaboration → kernel check. Each elaborated command updates
the environment, parser tables, and open namespaces that the next command
sees; elaboration converts user-facing syntax into core terms and stores
expression types as metadata in a side table used by interactive tooling
(Lean Language Reference, "Elaboration and Compilation").

### Type inference

Term elaboration is expected-type-directed and metavariable-based, and it
*postpones* subterms whose types are not yet known: `Elab.App` logs
"propagation postponed, resulting type depends on arguments that have not
yet been elaborated" (`src/Lean/Elab/App.lean`), and `SyntheticMVars`
postpones `x` in `Prod.fst x` "because its type is not known"
(`src/Lean/Elab/SyntheticMVars.lean`). Overloaded literals resolve through
default instances — an `OfNat Nat` default at priority 100 selects `Nat`
without context (Reference, "Instance Declarations"; registry in
`src/Lean/Meta/Instances.lean`), and the standard library notes that
"monomorphic default instances must always win" to preserve the Lean 3
monomorphic feel (`src/Init/Notation.lean`). Coercions are user-declared
classes assembled into chains of smaller coercions (FPiL, "Coercions").

### Macros and syntax extension

The parser is recursive descent with Pratt precedence tables and Packrat-style
memoization for ambiguous grammars; users may define new syntax in any
command, and it becomes available in the next command (Reference,
"Elaboration and Compilation", "Defining New Syntax"). Macros transform
`Syntax` to `Syntax` before elaboration, and every name a macro introduces is
hygienic: fresh macro scopes make generated names inaccessible to user code
(Metaprogramming in Lean 4, ch. 6; the CADE-28 paper calls this a
capture-avoiding macro system).

### Modules and namespaces

Modules are units of code distribution; namespaces are units of code
organization, deliberately decoupled (FPiL, "Starting a Project"; Reference,
"Source Files and Modules"). Importing has no effect on open namespaces, and
visibility is controlled by bounded shapes: `open Foo hiding x`,
renaming/only/scoped variants, and `open Foo in <command>` for a
single-command scope (Reference, "Namespaces and Sections").

### Type classes and implicit arguments

Implicit binders `{x : T}` are filled from expected types; instance-implicit
binders trigger a bounded backtracking search over a global instance database
with declared priorities as tiebreakers (Reference, "Instance Synthesis";
TPiL4 "Type Classes": a Prolog-like program, `synthInstance.maxSize`, and
search refusing out-parameters that are still metavariables). Unmentioned
type names can be auto-bound as implicits (`autoImplicit`, TPiL4
"Interacting with Lean").

### Ergonomics: diagnostics, explanations, linters, patterns

Lean errors are structured data rendered for humans. The manual ships a
registry of named errors — a Name | Summary | Severity | Since table per
error (Reference, "Error Explanations" chapter). Linters run automatically:
the unused-variables linter reports local variables never referred to, using
info trees (`src/Lean/Linter/UnusedVariables.lean`). Ambiguous names fail
through `ensureNonAmbiguous` after candidate resolution
(`Lean.ResolveName`). Match patterns destructure values "similar to the
syntax that would be used to construct the value", elaborated into case
trees by the match elaborator (Reference, "Terms/Pattern Matching";
`src/Lean/Elab/Match.lean`).

## Where Ziran stands today (topic comparison)

| Topic | Lean 4 | Ziran today (this tree) | Verdict |
|-------|--------|-------------------------|---------|
| Elaboration | command-at-a-time; metavariables; postponement | one-pass parse+check into ZIR; concrete types only | adopt postponement locally (P1) |
| Literal defaults | default instances with priorities | untyped integer `:=` infers `s64`; polymorphic calls need concrete context | document/complete the table (P8) |
| Defaults on calls | postponement until types land | defaults expand at call parse; literal-only specialized support | P1 |
| Macros / syntax | extensible parser + hygienic macros | frozen Jai grammar; `#insert` raw text splice | keep frozen; hygiene-lite (P5) |
| Modules | import = distribution | `#import`/`#load`, module parameters, module roots | parity or ahead; no action |
| Namespaces | bounded `open` shapes, scoped instances | `#scope_*`, `using` (fields/enums/imports), facade re-exports | finish documented gaps (P4) |
| Polymorphism | classes + instance backtracking search | explicit `$T` specialization | keep explicit (poor fit) |
| Coercions | `Coe` class chains, inserted outside-in | explicit checked `#as` | keep explicit (poor fit) |
| Diagnostics | structured messages + named explanation registry | stable codes + spans + `ziran explain` (schema v2) | typed fields + registry parity (P3) |
| Linters | auto-run linters (unused variables, ...) | `lint.cast`/`lint.char`/`lint.bool` warnings only | add unused-name lints (P6) |
| Ambiguity | candidates surfaced via `ensureNonAmbiguous` | `module.ambiguous`/`package.ambiguous` codes | list candidates (P7) |
| Pattern matching | structure-destructuring patterns | scalar `case` values only; `#complete`, `#through` | deferred (see below) |

## Ranked proposals

### 1. Postpone polymorphic-default elaboration until the specialized parameter type is known (Lean: elaboration postponement)

Lean: a subterm whose type is still unknown is queued and re-elaborated once
the surrounding types land (`Elab.App.lean`, `SyntheticMVars.lean` above).
Ziran now: call defaults expand during the call parse —
`cmd/compiler_expr_ir.zi::ExpandCallDefaults`, driven from
`cmd/compiler_parse.zi::ParseExpression` — with a checked helper per
parameter named by `cmd/zir/zir.c::FunctionDefaultHelperName` (used from
`cmd/zir/zir_parse_declaration.c` and `cmd/zir/zir_check_expr.c`). Literal
record defaults keep the specialized type at the call, but "field expressions
that themselves require the specialized type still need specialization
support" (docs/LANGUAGE_DIRECTION.md, "Jai source syntax", defaults).
Change: keep non-literal defaults as typed obligations attached to the call,
then check each default expression against the specialized parameter type
once the call's argument types are known — Lean's postponement in Ziran's
concrete-type world, replacing the literal-only special case with one rule.
Benefit: closes a documented usability gap; polymorphic defaults become as
usable as concrete ones; no new syntax or surface.
Effort: M. Files likely touched: `cmd/zir/zir_parse_source.c` (call
expansion), `cmd/zir/zir_check_expr.c`, the `.zir` writer, and
`docs/LANGUAGE_DIRECTION.md`.
Risks: helper-name stability in saved `.zir` (laws bind stable names);
defaults must keep declaration-scope resolution regardless of call site;
deterministic diagnostics (pairs with P3).
Future tests: extend `tests/generic_parameters.sh` and
`tests/generic_record_imports.sh` with defaults whose expressions use the
specialized type, plus rejection twins pinning today's errors for ambiguous
cases; run `make check CHECK_FILTER=generic` and `make check-bootstrap`.

### 2. Statement-level parse recovery with multi-error reports (Lean: `recoverFn` + command-loop skipping)

Lean: each parser carries a recovery handler (`recoverFn` with
`RecoveryContext`), and the top-level loop skips broken input to the next
command, so one run reports several independent errors
(`src/Lean/Parser/Basic.lean`, `src/Lean/Parser/Command.lean`).
Ziran now: the parser fails fast via `Services.Fatal` with `parse.*` codes in
`cmd/compiler_parse.zi`; recovery is exercised only by the C harness
(`tests/parser_recovery_test.c`, `tests/parser_recovery.sh`).
Change: at statement/declaration granularity, record the diagnostic, skip to
the next sync token (`;`, `}`, or a new top-level declaration), and continue
with a deterministic cap on reported errors per file; report at most one
error per recovered region to avoid cascades.
Benefit: the project's stated LLM loop (docs/LANGUAGE_DIRECTION.md, "Laws
and LLM development": diagnostics bound to the smallest relevant span) sees
all errors in one build instead of one per fix round.
Effort: M. Files likely touched: `cmd/compiler_parse.zi`,
`cmd/zir/zir_parse_source.c`, diagnostics plumbing, `docs/DIAGNOSTICS.md`.
Risks: clean-file `.zir` must stay byte-identical for the bootstrap suite;
new codes must enter the `cmd/package_explain.zi::DIAGNOSTIC_CODES`
registry that `tests/explain.sh` scans.
Future tests: `parser_recovery.sh` cases with two errors in one file
asserting both codes appear and a clean file stays byte-identical;
`make check CHECK_FILTER=parser` and `make check-bootstrap`.

### 3. Typed, explained diagnostics: structured fields + registry parity (Lean: elaboration metadata + the Error Explanations chapter)

Lean: elaboration stores expression types as side-table metadata that error
rendering consumes (Reference, "Elaboration and Compilation"), and the
manual ships a named-error registry with Name | Summary | Severity | Since
per entry (Reference, "Error Explanations" chapter).
Ziran now: diagnostics are schema v2 with code + span + message
(`cmd/diagnostics.zi::Emit`, the `schema_version` literal); `check.type`,
`check.constant_type`, and friends put types inside the message text, and the
`ziran explain` registry (`cmd/package_explain.zi::DIAGNOSTIC_CODES` with
summary/next_step, scanned by `tests/explain.sh`) carries no since/stability
metadata. docs/DIAGNOSTICS.md makes the code the stable identity and says
messages may change — so types belong in fields, not prose.
Change: (a) add optional `expected_type`/`actual_type` fields for the
type-family codes; (b) add since/stability metadata to the explain registry,
mirroring Lean's table; keep schema v2 additive or bump with a documented
migration; feed the same strings into `ziran explain` output.
Benefit: LLM repair and `ziran explain` consume types without parsing
English; message wording becomes freely improvable; the registry becomes
self-describing for consumers.
Effort: S. Files likely touched: `cmd/diagnostics.zi`,
`cmd/package_explain.zi`, `docs/DIAGNOSTICS.md`.
Risks: JSON consumers; keep byte-equality expectations of existing
diagnostic tests updated; `tests/explain.sh` conformance must stay green.
Future tests: fixtures asserting the new fields on `check.type` output and
the new registry fields; `make check CHECK_FILTER=explain`.

### 4. Finish the documented `using` gaps using Lean's bounded `open` shapes (Lean: hiding/renaming/only/scoped, `open ... in`)

Lean: namespace visibility is controlled by precise shapes — `open Foo
hiding x`, renaming/only/scoped variants, and `open Foo in <command>` for a
single-command scope (Reference, "Namespaces and Sections").
Ziran now: `using` already covers imports, record fields, enums, and public
facade re-exports in the tested subset (checks in
`cmd/zir/zir_check.c` and `cmd/zir/zir_parse_condition.c`; tests
`tests/data_scope_using.sh`, `tests/using_struct.sh`), but data-scope
`using` for records/unions and forward discovery remain documented gaps
(docs/LANGUAGE_DIRECTION.md, using sections; `check.using_scope` in the
diagnostic registry).
Change: smallest slice first — a single-statement `using ... in` form, then
data-scope record `using` with an explicit `hiding` list where promotion
would otherwise be ambiguous; lowered references stay checked member
accesses in `.zir`, as today.
Benefit: removes the last ergonomic gap around long qualified names without
adding a namespace system; Lean demonstrates that scoped, bounded visibility
is enough for large codebases.
Effort: M. Files likely touched: `cmd/zir/zir_check.c`,
`cmd/zir/zir_parse_condition.c`, `cmd/zir/zir_check_statement.c`.
Risks: ambiguity ordering; interactions with `#if` before a later `using`
(already documented unsupported) should be sequenced after, not bundled;
saved-IR naming must stay canonical for facade-qualified spellings.
Future tests: new cases beside `tests/data_scope_using.sh` and
`tests/using_struct.sh` covering `using ... in`, data-scope record `using`,
and ambiguity rejection twins; `make check CHECK_FILTER=using`.

### 5. Reserve generated-name prefixes (hygiene-lite, Lean: macro scopes)

Lean: names introduced by macros carry fresh macro scopes so generated
declarations can never collide with, or capture, user names (Metaprogramming
in Lean 4 ch. 6; CADE-28 paper).
Ziran now: compiler-generated default helpers are named `zi_default_<hash>`
(`cmd/zir/zir.c::FunctionDefaultHelperName`; the hash mixes file path,
function name, source line, and parameter index) — refactoring a line can
change a generated name, and nothing documented stops user source from
declaring `zi_default_*` spellings.
Change: first verify current collision behavior; then reject `zi_default_`
(or the `zi_` family) in user source and keep the one generated-name
convention in a single shared constant — capture-avoidance by prefix
reservation instead of a macro system.
Benefit: generated and user names can never collide, and helper renaming
cannot silently change stored `.zir` names.
Effort: S. Files likely touched: `cmd/zir/zir_check_program.c` (rejection),
`cmd/zir/zir.c` (single constant).
Risks: a new rejection could touch existing sources — audit `std/` and
`tests/` spellings before enforcing.
Future tests: a rejection case for a user-declared `zi_default_*` name and
a scan that `std/` contains no reserved prefixes in user-visible names.

### 6. Unused local and unused import lints (Lean: the unused-variables linter)

Lean: a linter runs automatically on all commands and reports local
variables that are never referred to, using info trees
(`src/Lean/Linter/UnusedVariables.lean`; Reference, "Linters and Code
Quality").
Ziran now: warnings exist for redundant casts, character comparisons, and
boolean patterns (`cmd/zir/zir_check_program.c`: `lint.cast`, `lint.char`,
`lint.bool`), and results can require use via `#must`; the diagnostic
registry has no unused-name codes, and unused locals or imports pass
silently.
Change: add `lint.unused_local` and `lint.unused_import` warnings at check
time for procedure locals and file imports never referenced; warnings only,
deterministic order, never errors; treat `#run`-evaluated and
`#compile_time`-observed names as used.
Benefit: dead-name feedback without sprinkling `#must`; aligns with Lean's
automatic-linter posture and the LLM loop (warnings point at exact spans).
Effort: S. Files likely touched: `cmd/zir/zir_check_program.c`,
`cmd/package_explain.zi` (two registry entries).
Risks: false positives around `defer` cleanup, blank-identifier discard
conventions, and public `using` re-exports (define re-export as a use);
warning volume in large files.
Future tests: positive and negative lint cases beside the existing lint
tests; registry entries pass `make check CHECK_FILTER=explain`.

### 7. Ambiguity diagnostics list all candidates (Lean: `ensureNonAmbiguous`)

Lean: name resolution resolves candidates then fails explicitly through
`ensureNonAmbiguous`, surfacing the ambiguity as a first-class error
(`Lean.ResolveName`).
Ziran now: `module.ambiguous` and `package.ambiguous` exist in the registry
(`cmd/package_explain.zi::DIAGNOSTIC_CODES`), but whether the message lists
the candidate qualified paths is not documented.
Change: verify today's message; if candidates are not listed, include every
candidate's fully qualified path and declaring file in the message and, via
P3, in structured fields.
Benefit: the LLM loop and humans can resolve the conflict in one step
instead of grepping for same-named declarations; matches Lean's
candidates-in-error behavior.
Effort: S. Files likely touched: the `module.ambiguous` emission site,
`cmd/package_explain.zi`.
Risks: message length with many candidates (cap the list, count the rest);
keep the code stable so `ziran explain` keeps working.
Future tests: two same-named imports producing one diagnostic listing both
paths; rejection twins unchanged.

### 8. Document and complete the untyped-literal default table (Lean: default instances, monomorphic defaults win)

Lean: overloaded literals resolve through default instances with declared
priorities — `OfNat Nat` at priority 100 without other context (Reference,
"Instance Declarations") — and the standard library keeps monomorphic
defaults winning over generic ones (`src/Init/Notation.lean`).
Ziran now: untyped numeric literals "need a concrete context to infer `T`"
for polymorphic calls; an untyped integer local bound with `:=` infers
`s64`; `float` is an alias for `float32`; `#char` infers `s64`
(docs/LANGUAGE_DIRECTION.md). The equivalent default for untyped float
literals is not documented, and the rules live in prose rather than one
table.
Change: verify the float/char defaults in the checker, then document one
default table (integer → `s64`, float → `float32`, `#char` → `s64`) in
docs/LANGUAGE_DIRECTION.md and pin each row with positive and negative
tests; keep polymorphic calls context-required, mirroring Lean's
monomorphic-defaults-win rule.
Benefit: literal inference becomes predictable and documented instead of
folklore; no behavior change beyond documenting what the checker does.
Effort: S, verification-first. Files likely touched: the checker's literal
inference path, docs.
Risks: if the checker's actual behavior differs from the intended table,
this becomes a behavior decision — surface it to the owner instead of
silently picking a default.
Future tests: literal-default cases beside the generic tests;
`make check CHECK_FILTER=generic`.

## Sequencing and dependencies

- Quick wins, independent: P3, P5, P6, P7, P8 (all S; P3's typed fields make
  P7's candidate lists machine-readable).
- Core language work: P1 and P4 (both M) — P1 pairs with P3 for clean
  diagnostics; P4 should land after P7 so ambiguity errors point at
  candidates when `using` promotions collide.
- P2 (parse recovery, M) is independent and pays off for every later
  frontend change, since multi-error output shortens its own bring-up loop.
- No proposal depends on the compiler/runtime or verification lanes; P1 and
  P5 touch `.zir` naming, which the verification lane's bundle checks
  consume — coordinate those two if both land in one release window.

## Considered and deferred

- **Match-case record destructuring** (Lean's patterns mirror construction,
  Reference "Terms/Pattern Matching"; elaborator `Lean.Elab/Match.lean`):
  Ziran's `case` arms accept scalar values and its enums carry no payloads,
  so destructuring patterns need payload enums first — a language shape
  decision above this lane's pay grade. Revisit only if payload enums are
  ever adopted; `#complete` already covers exhaustiveness.
- **Declaration docstrings**: Lean attaches `/-- ... -/` docstrings to
  declarations and lints their quality, but the strongest primary anchors
  here (linter infrastructure) are already covered by P6; revisit if the
  site generator wants declaration-level docs.

## Does not fit Ziran's model

- **Syntax extension and notation commands** (`declare_syntax_cat`,
  `notation`, custom elaborators — Reference, "Defining New Syntax"):
  Ziran's grammar is deliberately frozen and audited, and user syntax would
  expand the checked surface that `.zir`/`.zx` determinism and law gates
  depend on. Lean needs it for mathematical notation; `#insert` already
  covers Ziran's code-generation need.
- **Type classes / instance-implicit arguments**: resolution is a
  backtracking search with priorities and output-parameter constraints
  (TPiL4; Reference "Instance Synthesis"), which conflicts with Ziran's
  explicit `$T` specialization and deterministic, budgeted law checks.
  Comparators and conversions stay explicit procedure parameters; if
  constrained polymorphism is ever needed, it should be a bounded explicit
  constraint list — never a global instance database.
- **Coercion class chains** (`Coe`/`CoeT`, assembled chains per FPiL):
  silent conversions undermine manual safety and law review; `#as` keeps
  conversions explicit and checked.
- **Auto-bound implicits** (`autoImplicit`): inventing `$T` from the body
  hides signatures from the LLM/law loop and destabilizes `.zir` naming;
  Ziran requires declared type parameters and should keep that rule.
- **Dependent types or proof terms in the source language**: excluded by
  the task, and anyway the checked-data/machine-verification split is the
  verification lane's subject
  (`plans/2026-11-18-learn-from-lean-verification.md`).

## Future validation matrix

| # | Positive tests | Negative twins | Cross-checks | Command |
|---|----------------|-----------------|--------------|---------|
| 1 | defaults using the specialized type in `generic_parameters.sh`/`generic_record_imports.sh` | ambiguous-default rejection pins | `.zir` byte equality on clean files | `make check CHECK_FILTER=generic` |
| 2 | two-error file reports both codes | single-error behavior unchanged | clean-file byte equality | `make check CHECK_FILTER=parser`, `make check-bootstrap` |
| 3 | JSON fixtures with expected/actual; registry since/stability | schema version guard | `tests/explain.sh` scan | `make check CHECK_FILTER=explain` |
| 4 | `using ... in`, data-scope record `using` | ambiguity rejection twins | facade naming unchanged | `make check CHECK_FILTER=using` |
| 5 | `zi_default_*` user name rejected | generated helpers keep working | `std/` prefix scan | `make check` (language checks) |
| 6 | unused local/import warnings fire | `#run`/re-export uses stay silent | registry entries | `make check CHECK_FILTER=lint` or new lint cases |
| 7 | ambiguity message lists both candidate paths | unrelated codes unchanged | explain entry updated | same-ambiguity case as today |
| 8 | each documented default row pinned | polymorphic calls still context-required | docs match checker | `make check CHECK_FILTER=generic` |

Bootstrap safety: every proposal that touches naming or diagnostics (P1,
P2, P3, P5) must keep `make check-bootstrap` green — clean sources produce
byte-identical `.zir`/`.zx` — per the repository rule that generated
output stays deterministic.

## Open questions for the coordinator/owner

1. P1: may generated default-helper names change in saved `.zir`, or must
   existing stored names stay stable (laws bind stable names)?
2. P2: how many errors per file before suppression — a fixed cap (e.g. 20)
   or unlimited with deterministic ordering?
3. P6: are unused-import warnings acceptable noise, or should they be
   local-only (unused locals first, imports later)?
4. P8: if the checker's current float default disagrees with `float32`, the
   documentation-first plan becomes a behavior decision — which default do
   you want if they disagree?

## Implementation status (same day, owner-authorized follow-up)

The owner asked to continue from this plan into implementation in the same
session ("so kep going improve furethr and cautlayl implemntei t"). The
S-tier items below were implemented in the shipping compiler's C tree
(`cmd/zir/`, which builds `zi2zir`, the tool `ziran check` launches), with the
diagnostic registry in `cmd/package_explain.zi`.

- **P7 (ambiguity candidates)** - implemented in
  `cmd/zir/zir_load.c::early_target`: the diagnostic now collects matching
  modules, lists up to five candidate `source_path`s (plus an `(N more)`
  tail), and states the count. Finding recorded during implementation: under
  the current loader invariants this diagnostic is a safety net, not a live
  path - `module_named` (zir_load.c) rejects duplicate module identities at
  load time, and stems derive from module names, so `early_target` sees at
  most one candidate today. The improvement keeps the message actionable if
  that invariant ever loosens; no CLI test can reach the code path today, so
  validation is the existing module suite plus this note.
- **P5 (reserved generated-name prefixes)** - implemented in
  `cmd/zir/zir_check_program.c::reserved_declaration` with
  `generated_helper_name`: user declarations (procedures, constants, globals,
  records, import aliases) named `zi_default_*` that do not match the exact
  generated shape (`zi_default_<16 hex>`, optional `_field_<digits>` tail)
  fail with the new code `check.reserved_name`; the compiler's own generated
  helpers pass unchanged. Registry: `check.reserved_name` added to
  `cmd/package_explain.zi::DIAGNOSTIC_CODES` in sorted position with a
  specific explain entry. Test: new `tests/reserved_names.sh` (five rejection
  twins plus a defaults-still-work positive case).
  Test-authoring notes fixed while making the script green: record fields
  separate with `;` (a comma-separated body leaves the type body unclosed and
  swallows the next directive), struct field defaults are not supported by
  the checker today (`zir_check_statement.c` "unknown stored type"), so the
  positive case exercises procedure default arguments only, `--exe` builds
  need `--entry module:function`, and the built binary reports the entry's
  return value as its process exit status (24 = Pick(1) + Pick(2, 3) + 2 +
  9) rather than on stdout.
- **P8 (untyped-literal default table)** - documented current behavior in
  `docs/LANGUAGE_DIRECTION.md` (integer to `s64`, real to `float64`,
  `#char` to `s64`, context pinning, overload-fit ordering from
  `cmd/zir/zir_check_expr.c::overload_fit`). The float32/float64 question in
  Open question 4 stays a behavior decision for the owner; nothing changed
  behaviorally.
- **P3 remainder (registry since metadata)** - deferred: the sweep is a
  200+ code documentation pass with no behavioral effect; it should be its
  own change.
- **P6 (unused local/import lints)** and **P1/P2/P4** - not started in this
  pass; unchanged effort estimates above apply.

### Verification status

- `build-queue -- sh -eu -c 'make check CHECK_FILTER=reserved_names && ...'`:
  `PASS reserved_names.sh (0.1s)` under the check-runner. The chained run's
  module filter passed 5/7; the two failures are sibling lanes' in-flight
  case sources (`tests/zx_module_compiler_cases.zi` - unresolved
  `PathParent`; `tests/module_root_capacity_cases.zi` - `using` syntax
  error), unrelated to the loader change: the module suites that exercise
  `zir_load.c` (`module_registration`, `module_parameters_cases`,
  `compiler_modules_cases`, `c_module_init`, `private_module_imports`) all
  passed.
- Explain registry parity for this lane's code verified directly:
  `ziran explain check.reserved_name` resolves with the new summary/action
  and the code appears in `explain --list --json`; the C emission side is
  discovered symmetrically by `tests/explain_cases.zi`'s C-file scan. The
  full `explain.sh` run currently fails on three codes
  (`zx.module-export`, `zx.module-import`, `zx.module-run`) emitted by a
  sibling lane's new untracked `cmd/zir/zir_zx_module.c` without registry
  entries - a shared-registry gap owned by that lane.
- `make check-bootstrap` re-run after the sibling refreshed the
  `compiler_bundle` seeds now fails on their next stale seed
  (`bootstrap/compiler_zir_lower/compiler_zir_lower.c` differs from the
  regenerated output). No bootstrap seed covers this lane's files
  (`bootstrap/` has no `package_explain`); the drift is owned by the lane
  editing `cmd/compiler_zir_lower.zi`.

## Sources (accessed 2026-11-18)

### Lean Language Reference (Verso manual)

- Elaboration and Compilation:
  https://lean-lang.org/doc/reference/latest/Elaboration-and-Compilation/
- Instance Declarations (default instances):
  https://lean-lang.org/doc/reference/latest/Type-Classes/Instance-Declarations/
- Instance Synthesis:
  https://lean-lang.org/doc/reference/latest/Type-Classes/Instance-Synthesis/
- Namespaces and Sections:
  https://lean-lang.org/doc/reference/latest/Namespaces-and-Sections/
- Source Files and Modules:
  https://lean-lang.org/doc/reference/latest/Source-Files-and-Modules/
- Terms/Functions (implicit binders):
  https://lean-lang.org/doc/reference/latest/Terms/Functions/
- Terms/Pattern Matching:
  https://lean-lang.org/doc/reference/latest/Terms/Pattern-Matching/
- Notations and Macros / Defining New Syntax:
  https://lean-lang.org/doc/reference/latest/Notations-and-Macros/Defining-New-Syntax/
- Coercions: https://lean-lang.org/doc/reference/latest/Coercions/
- Error Explanations (named-error registry, Since column):
  https://lean-lang.org/doc/reference/latest/Error-Explanations/
- Linters and Code Quality (chapter):
  https://lean-lang.org/doc/reference/latest/

### Theorem Proving in Lean 4

- Type Classes: https://lean-lang.org/theorem_proving_in_lean4/Type-Classes/
- Interacting with Lean (auto-bound implicits):
  https://lean-lang.org/theorem_proving_in_lean4/Interacting-with-Lean/

### Metaprogramming in Lean 4

- Syntax: https://leanprover-community.github.io/lean4-metaprogramming-book/main/05_syntax.html
- Macros (hygiene): https://leanprover-community.github.io/lean4-metaprogramming-book/main/06_macros.html

### Functional Programming in Lean

- Starting a Project (modules vs namespaces):
  https://lean-lang.org/functional_programming_in_lean/Hello___-World___/Starting-a-Project/
- Coercions:
  https://lean-lang.org/functional_programming_in_lean/Overloading-and-Type-Classes/Coercions/

### lean4 sources (GitHub master)

- `src/Lean/Elab/App.lean` (postponed expected-type propagation):
  https://github.com/leanprover/lean4/blob/master/src/Lean/Elab/App.lean
- `src/Lean/Elab/SyntheticMVars.lean` (postponement of typed unknowns):
  https://github.com/leanprover/lean4/blob/master/src/Lean/Elab/SyntheticMVars.lean
- `src/Lean/Elab/Match.lean` (match elaborator):
  https://github.com/leanprover/lean4/blob/master/src/Lean/Elab/Match.lean
- `src/Lean/Meta/Instances.lean` (default-instance registry):
  https://github.com/leanprover/lean4/blob/master/src/Lean/Meta/Instances.lean
- `src/Init/Notation.lean` ("monomorphic default instances must always win"):
  https://github.com/leanprover/lean4/blob/master/src/Init/Notation.lean
- `src/Lean/Parser/Basic.lean` (per-parser recovery):
  https://github.com/leanprover/lean4/blob/master/src/Lean/Parser/Basic.lean
- `src/Lean/Parser/Command.lean` (command-loop skipping):
  https://github.com/leanprover/lean4/blob/master/src/Lean/Parser/Command.lean
- `src/Lean/Linter/UnusedVariables.lean` (unused-variable linter):
  https://github.com/leanprover/lean4/blob/master/src/Lean/Linter/UnusedVariables.lean
- `Lean.ResolveName` API (candidate resolution, `ensureNonAmbiguous`):
  https://lean-lang.org/doc/api/Lean/ResolveName.html

### Paper

- de Moura & Ullrich, "The Lean 4 Theorem Prover and Programming Language"
  (CADE-28; hygienic, capture-avoiding macros):
  https://lean-lang.org/papers/lean4.pdf
