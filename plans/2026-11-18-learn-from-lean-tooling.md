# Learn from Lean 4: tooling and developer workflow (2026-11-18)

The initial research was analysis only; the separately authorized implementation
slice is recorded below. The research ranks Lean 4's editor/build tooling ideas against Ziran's actual
command-line and edit-tool workflow. Compiler/runtime performance is covered
separately in `plans/2026-11-18-learn-from-lean-compiler.md`; proof and
checking work in `plans/2026-11-18-learn-from-lean-verification.md`.

## What Lean 4 does, briefly

- **Language server:** a watchdog process plus one worker per open file.
  Workers crash-isolate elaboration and restart on import changes or stack
  overflow; the watchdog keeps only open-file contents and crash locations.
  `lake serve` wraps `lean --server`; `lake setup-file` builds a file's
  imports for the server so edits do not rebuild dependencies.
  https://github.com/leanprover/lean4/blob/master/src/Lean/Server/README.md
- **Snapshots:** commands elaborate into a task tree of `Snapshot` nodes
  (`Lean.Language.Lean.process`). The processor picks granularity (Lean uses
  one snapshot per top-level command, nested inside declarations). Each
  snapshot carries syntax, a message log (diagnostics), info trees, a
  reporting range, and an optional cooperative cancel token
  (`cancelTk? : Option IO.CancelToken`). Incremental reuse compares old and
  new *syntax* instead of hashing state, and Lean's own docs warn that an
  edit can invalidate commands *before* the edit point (grammar atomicity),
  so reuse backs up to a safe boundary.
  https://github.com/leanprover/lean4/blob/master/src/Lean/Language/Basic.lean
  https://github.com/leanprover/lean4/blob/master/src/Lean/Language/Lean.lean
- **Incremental elaboration:** declaration-internal reuse builds on
  persistent data structures and promise cells; tactic-level reuse is opt-in
  per combinator (`registerBuiltinIncrementalTactic`). The Lean 4 paper
  describes the server as continuously analyzing the source text.
  https://lean-lang.org/papers/lean4.pdf
- **Build and developer tools:** Lake traces hash inputs to decide
  dirtiness, supports local and remote artifact caches, ships configurable
  `lake test`/`lake lint` drivers, and `lake shake` computes a minimal
  import set.
  https://lean-lang.org/doc/reference/latest/Build-Tools-and-Distribution/Lake/

## Ranked proposals

### 1. Serve the existing edit/check pipeline from one persistent process (Lean: language server + lake serve)

Lean: one server keeps documents and analysis state alive across edits;
worker crashes are contained and the session survives restarts. Links above.
Ziran now: `ziran edit` (`cmd/edit.zi` + `cmd/zir/zir_edit.c`) is one-shot —
each request spawns a process, reopens the document, and re-loads the whole
program set via `ProgramsLoadWithDefines` (`cmd/zir/zir_load.c::ProgramsLoadWithDefines`,
line 1413). The internal tool API is already staged like a session
(`ZiranEditOpen`/`ModulePath`/`Define`/`AddEdit`/`BuildEdit`/`ValidateEdit`/
`FinishEdit` in `cmd/zir/zir_edit.c`, bound in `cmd/edit.zi`), but the handle
dies with the process. There is no server anywhere (`grep jsonrpc cmd/` is
empty; `cmd/transport.zi` is a byte-exact bundle encoding, not a protocol).
Change: `ziran serve` (or `zi-edit serve`) keeps one `ZiranEdit`-style handle
and `CompilerContext` alive over a stdio loop, reusing the existing
schema_version-1 JSON request/response envelope (`cmd/edit.zi::ApplyRequest`)
as the message format. Per-candidate checking stays full until proposal 2
lands. Load module paths and defines once per session (Lean's
`lake setup-file` lesson), not per request.
Benefit: removes per-request process startup and whole-set reload from the
edit loop, and gives live diagnostics a home. No speedup is claimed until
measured.
Effort: medium-high (C stdio framing, request lifecycle, long-lived memory).
Risk: stale or leaking session state; mitigate with explicit session close
and the existing `document_sha256` guard (`ZiranEditOpen`, zir_edit.c:112).
Validation: a serve-session test drives a sequence of edits and requires
byte-identical results and diagnostics versus one-shot `ziran edit` runs
(extend `tests/semantic_edit.sh`).

### 2. Digest-keyed frontend check cache (Lean: .olean artifacts + Lake traces)

Lean: imports compile to `.olean` files; Lake stores trace hashes beside
artifacts and skips unchanged work; the language layer reuses elaboration
state for unchanged prefixes guarded by syntax comparison.
Ziran now: every `check`/`build`/`run`/edit-validate re-parses and re-checks
the entire program set (`ProgramsLoadWithDefines_in_context`,
cmd/zir/zir_load.c:1323); there is no frontend cache. The pattern already
exists twice downstream: the C-tier build cache
(`cmd/compiler_backend_cache.zi::BuildCache` line 28, `CacheMagic` "Ziran C
cache 1" line 14, `FileDigest`/`FileAdd` lines 316/249) and the native-image
disk cache (`cmd/compiler_native_cache.zi`, `NativeCacheFormat` "ziran native
image 1", key names everything the code depends on, refuses truncated/stale
entries). Checked programs already serialize (`cmd/zir/zir_serial.c::ProgramWrite`
line 1695, `ProgramRead` line 2013), and `ZirDocument`
(`cmd/zir/zir_document.h`) records a sha256 identity per document.
Change: cache key = toolchain/format version + module content digest +
import digests + define set (the `--define`/`--module-path` inputs edit.zi
already accepts). Persist checked program-set members via
`ProgramWrite`/`ProgramRead`; on load, reuse unchanged modules and re-check
only changed ones. Reuse granularity stays at module/declaration boundaries —
Lean's parser-atomicity warning means prefix reuse must stop at safe
boundaries, and intra-command reuse (its promise-cell machinery) is out of
scope here.
Benefit: repeated invocations in the edit loop skip unchanged modules. The
load phase is already profiled (`ProfileEnd("load", ...)` in
cmd/zir/zir_load.c), so any gain gets measured, not assumed.
Effort: medium.
Risk: staleness — the key must cover defines and module paths; invalidation
discipline copies the native cache's format-magic bump and digest refusal.
Validation: golden runs with and without the cache must produce identical
diagnostics and IR; cache-invalidation cases in the style of the native
cache tests (`tests/native_cache_module_cases.zi`).

### 3. Cooperative cancellation tokens (Lean: SnapshotTask.cancelTk?)

Lean: snapshot tasks carry optional cancel tokens; the server abandons stale
work when the document moves on, and requests wait for specific snapshots.
Ziran now: no cancellation anywhere in `cmd/zir` (grep for cancel comes back
empty); every check runs to completion once started, and rapid successive
edits queue redundant full re-checks (`check_candidate`,
cmd/zir/zir_edit.c:173).
Change: thread a cancel flag through parse/check loops with periodic checks
(per function or per fixed instruction budget), expose a `cancel` message in
the proposal-1 serve protocol, and let stale candidates abort early.
`ZiranEditFinish` (`cmd/zir/zir_edit.c::ZiranEditFinish`, line 325) already
refuses to apply when `still_current` (line 299) fails — cancellation is the
same idea moved earlier in the pipeline.
Benefit: an editor drops obsolete validations instead of queueing them;
prerequisite for responsive live checking.
Effort: medium (touches checker loops).
Risk: a cancelled check must report "aborted", never "ok" — a missed check
would be a correctness hole.
Validation: serve-session test cancels an in-flight check and asserts no
partial result reaches `ZiranEditFinish` or any cache.

### 4. Editor-facing diagnostics parity (Lean: per-snapshot diagnostics re-reported to the client)

Lean: every snapshot carries its diagnostics; the server re-reports the
current set as the document evolves, so editors always show live, positioned
problems.
Ziran now: the rich model exists (`cmd/zir/zir_diagnostic.h::DiagnosticDetails`:
semantic_id, cause_kind, expected_type/actual_type, related span/message,
target/capability, suggested_name, edit_span/replacement quickfix) and JSON
mode emits byte-precise quickfix alternatives (`cmd/zir/zir_diagnostic.c::report`,
line 118). Text mode prints `path:line:col: msg` plus a related note
(zir_diagnostic.c:204-211) but drops expected/actual and quickfix data; the
edit protocol reports success/failure and streams diagnostics to stderr —
the `ApplyRequest` result JSON (`cmd/edit.zi`) carries only path/applied/source.
Change: one diagnostics accessor consumed by text mode, JSON mode, and a
`"diagnostics":[...]` array added to the edit/serve result, so an editor gets
spans plus quickfix alternatives as data instead of parsing stderr. Text mode
gains a caret line and expected/actual suffixes.
Benefit: single diagnostic pipeline; editor integrations become mechanical.
Effort: low-medium.
Risk: output format churn; update `tests/diagnostic_format.sh` and
`tests/diagnostic_metadata.sh` goldens deliberately.
Validation: diagnostic_format.sh plus semantic_edit.sh assert the same
diagnostic set in text and JSON modes and in the edit result.

### 5. Developer-tool bundle: `ziran test` driver + unused-import lint (Lean: lake test/lint + lake shake)

Lean: Lake ships configurable test and lint drivers, and `lake shake`
computes a minimal import set.
Ziran now: the command list (`cmd/package.zi::PkgDispatch` line 216, usage
line 825) has check/ir/build/run/fmt/inspect/compile-commands but no `test`;
`ziran fmt [--check]` exists (dispatch line 230, usage line 782) and
`ziran compile-commands` (`cmd/package_compile_commands.zi`) targets the
generated C for clangd, not .zi sources. There is no unused-import warning.
Change: `ziran test` runs a project-declared driver (default: project test
scripts; honor the existing manifest `tool` delegation for external
harnesses, consistent with `cmd/package.zi` dispatch). Opt-in `ziran lint`
warns when an `#import`ed module contributes no referenced symbol to the
translation unit (a post-check symbol walk — needs scoping against the
checker's symbol tables before any implementation).
Benefit: one-command project verification and import hygiene beside the
existing check/fmt/build surface.
Effort: low for the test driver, medium for the lint.
Risk: lint false positives (generated or macro-introduced references); ship
opt-in with an escape flag.
Validation: new cases in the `tests/formatter.sh`/`tests/formatter_cases.zi`
style; run the lint against `cmd/` itself as a corpus before enabling it.

## Does not fit Ziran's model (today)

- **Multi-process watchdog/worker isolation:** Lean crashes-isolate per-file
  workers because one bad elaboration must not kill the IDE. Ziran's
  single-process compiler that exits non-zero on failure gives most of the
  recovery value without IPC; revisit only if proposal 1 shows real crash
  frequency.
- **Intra-command incremental elaboration:** Lean's promise-cell, syntax-
  compare reuse machinery presumes persistent data structures and a snapshot
  tree; premature before proposal 2 lands module-level reuse.
- **Remote artifact caches (`lake cache get/put`):** Ziran solves
  distribution with bundles; there is no evidence of a build-artifact
  sharing need yet.

## Sources (accessed 2026-11-18)

- Lean 4 language server architecture and worker/watchdog model:
  https://github.com/leanprover/lean4/blob/master/src/Lean/Server/README.md
- Snapshot framework and cancel tokens:
  https://github.com/leanprover/lean4/blob/master/src/Lean/Language/Basic.lean
- Incremental parsing/elaboration, reuse guards, parser atomicity:
  https://github.com/leanprover/lean4/blob/master/src/Lean/Language/Lean.lean
- Lean 4 paper (server analyzes the source text continuously):
  https://lean-lang.org/papers/lean4.pdf
- Lake: traces, caches, serve/setup-file, test/lint/shake drivers:
  https://lean-lang.org/doc/reference/latest/Build-Tools-and-Distribution/Lake/
- Lean 4 home: https://lean-lang.org/

## Implementation status (2026-11-18, owner-approved slice)

Proposal 4, first slice implemented on top of the analysis (owner: ''implemet
and improve''). The edit request envelope now carries the diagnostics the
request produced:

- cmd/zir/zir_diagnostic.{c,h}: bounded diagnostic collector
  (ZiranDiagnosticsBeginCollection/ZiranDiagnosticsEndCollection). While
  enabled, report() also appends the complete schema-version-2 JSON record, including document identity,
  related graph and edit alternatives, to a
  128 KiB / 128-entry buffer; streamed stderr output keeps its exact shape and
  over-limit entries are dropped whole, never malformed.
- cmd/edit.zi: BeginCollect at ApplyRequest entry, WriteDiagnostics helper,
  FailResult wrapper for every failure exit (message failures and bare
  native-failure returns), and a success-path diagnostics array. Every
  parsed edit request emits exactly one schema_version-1 result document on stdout;
  rejected edits now answer {applied:false, diagnostics:[...]} instead of
  empty stdout.
- cmd/edit.zi exit guard: compiler services report candidate syntax errors
  through a fatal diagnostic that exits the process (mirroring Lean's model
  of stream-then-abort on parse errors), so ValidateEdit could exit without
  any result document. RegisterExitGuard binds libc atexit; when the process
  leaves through that path, ExitGuard still flushes the collected
  diagnostics as a rejected envelope. Usage errors and unreadable request
  files also go through FailResult, so every edit invocation writes exactly
  one result document.
- tests/semantic_edit_cases.zi: SemanticRaw now parses the rejected-result
  envelope (schema_version 1, applied false, diagnostics array) in addition to
  the streamed per-line schema_version-2 records on stderr.

The text-mode caret/expected/actual suffixes from proposal 4 remain future
work, as do proposals 1, 2, 3 and 5.

Successor correctness review: repaired malformed success JSON, included
command-argument failures in the embedded array, and removed a borrowed-string
use after its native buffer was freed. Collection shares the existing serializer
in both text and JSON diagnostic modes. The retained semantic edit test now
compares embedded diagnostics with stderr, including complete native metadata,
and exercises malformed request JSON. A later integration fix uses raw libc
storage for the collection buffer so failed frontend scopes cannot free it;
an atexit guard emits the rejected envelope on fatal syntax-check exits.

Fresh successor local verification on the current checkout passed:
`build/bin/check-runner --bin-dir build/bin --filter diagnostic` (6/6,
including Android logging, bounds, metadata, format, targets and tool diagnostics)
and `build/bin/check-runner --bin-dir build/bin --filter semantic_edit` (1/1).
Both ran through managed desktop_run with DISPLAY/WAYLAND_DISPLAY removed;
no test results were reused. Tooling source is already committed in the current
checkout by the preceding acting runtime. Full suite/bootstrap validation is
owned by the shared integration lane; its fresh result and installation/push
status remain unverified here. No new commit, push or install by this successor.
