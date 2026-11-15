# Jai lessons for Ziran (2026-11-15)

Owner words: "what can we leratn from jai to improve our", "so improev itp orprely plaes".

Planning only. Claude lanes implement afterwards. Sources read for this plan:
`docs/JAI_PARITY_ROADMAP.md`, `docs/LANGUAGE_DIRECTION.md`, `docs/USAGE.md`
(Imports, Module initializers, Multiline strings, Generic values),
`docs/FEATURES.md`, `docs/IMPLEMENTATION_STATUS.md`, `docs/LAWS.md`, the
checker and parser under `cmd/zir/`, the compiler modules under `cmd/*.zi`,
`std/`, `tests/` conventions, and the live lane plans
(`plans/2026-11-14-self-hosting.md`, `plans/2026-11-13-native-optimizations.md`,
`plans/2026-11-15-native-checked-pointers.md`).

## Verdicts on the coordinator's five lessons

| Lesson | Verdict |
| --- | --- |
| 1. Module parameters `#import "X"(FEATURE=false)` | **Adopt — Step 2.** Nothing in Ziran today parameterizes an import; `USAGE.md` has no such section. Highest structural win: it is the checked-source answer to pay-per-import feature selection. |
| 2. The build as a Ziran program | **Defer** (Phase 3). Jai's compiler-as-metaprogram needs a compiler command API inside the language. Self-hosting S5+ (checker in Ziran) is the natural gate. Module parameters plus the package manifests deliver most of the practical value first. |
| 3. Compile speed: direct native backend or one C file per program | **Adopt the C half now — Step 3.** The native machine path (M2–M4) already belongs to the native lane; this plan only adds the cheap, independent win: one translation unit per program. Kryon's `build/zx-player` holds 206 `.c` and 245 `.h` files today; the coordinator measured 62 `.c` + 77 `.h` for an empty window. |
| 4. Implicit context + per-frame temporary allocator | **Split.** The temporary allocator half is **Step 1** (pure `std/`, portable, no compiler change). The implicit-context half starts as an explicit `std/context` record (**Step 4**); the Jai-style hidden parameter and `push_context` wait for a language phase (Phase 2) because it touches every signature, the ABI, effect classes, and all serializers. |
| 5. Compile-time code generation and introspection (`#insert`, `#modify`, `type_info`, message loop) | **Adopt a narrow slice — Step 5** (`#insert` of compile-time string constants at file scope). `#modify`, `type_info`, and the message loop are **deferred** (Phase 3): general metaprogramming is explicitly unsupported today and must arrive with effect/resource limits designed for a Ziran checker, not bolted onto the C checker. |

Already satisfied — no work planned: module initializers (documented in `USAGE.md`, covered by `tests/runtime_globals.sh`, `tests/c_module_init.sh`, `tests/go_package_init.sh`), `#through` in match arms, `#caller_location` defaults, `#program_export`, `defer`, lexical/data-scope `using`, direct polymorphic calls. Generic constraints remain the existing roadmap's own open item; this plan does not duplicate it.

## Coordination with live lanes

- **Kryon pay-per-import lane** (module initializers, fix in `cmd/zi2go/zir_go_lower.c`): Step 2 also touches `zir_go_lower.c` (instance name prefixes). Sequence Step 2's Go-emitter sub-step after that lane's fix lands, or rebase onto it. Step 2's parser/checker/serializer/C-emitter work is conflict-free and can start now.
- **Self-hosting S1** (`cmd/compiler_zir_lower.zi`): no planned step touches it. Step 5 deliberately stays in the parse path so S1 is unaffected. Every checker-side feature below stores its decisions in checked IR + serializer fields, never in parser-only state, so the S5+ Ziran checker ports cleanly.
- **Native/M3 lane**: no file overlap (machine pipeline files untouched). Step 3 complements it: C stays the portable fallback and Kryon's desktop path.
- **Optimizer lane** (`cmd/compiler_opt.zi`): untouched. Module parameters fold before LIR, so the optimizer always sees final instances.

## Ordered steps

### Step 1 — `std/temp`: per-frame temporary arena (Group A) — first step

**Owner-visible behavior.** Frame handlers allocate scratch from one arena and
reset it in one call, instead of freeing per allocation:

```zi
#import "temp"

Frame :: () -> s32 {
    arena := TempBegin()
    defer TempEnd(arena)
    mark := TempMark(*arena)
    label := TempText(arena, "row")          // borrowed string view
    cells := TempSlice(arena, 64)            // borrowed []u8 scratch
    if TempSlice(arena, 0).count != 0 { return 1 }
    TempReset(*arena, mark)                  // one call drops the frame's scratch
    again := TempSlice(arena, 8)             // storage reused under the same rules
    if again.count == 8 && label.count == 3 { return 0 }
    return 1
}
```

Jai name: `Temporary_Storage` mark/reset. Ziran shape: `Temp` holds a
`Vec(u8)` plus the bump offset; `TempSlice` returns borrowed views; a reset
while any view is live fails through the existing Vec live-borrow rule, and
`defer TempEnd` frees at scope exit like any owned `Vec`. Works identically on
C, Go, and `.zx` because it is ordinary checked `Vec` code — no raw pointers,
no allocator interface, no engine change.

**Files and functions.** New `std/temp.zi` (`TempBegin`, `TempMark`,
`TempSlice`, `TempText`, `TempReset`, `TempEnd`); new `tests/temp_arena.sh`;
`docs/USAGE.md` standard-library subsection; `docs/IMPLEMENTATION_STATUS.md`
bullet; a `features` contract entry in `docs/FEATURES.json` is a cheap
follow-up once the contract stabilizes.

**Backend impact.** None below the language level. All four targets inherit
the existing Vec primitives; LIR/ZX/C/Go/Wasm/native untouched.

**Test.** `tests/temp_arena.sh` (repo convention: `set -eu`, `$1` = ziran,
`mktemp` workdir): answer equality from source and saved `.zir` via `ziran
bundle` + `ziran run`; `ziran build --target=c` + `cc -std=c99
-pedantic-errors -DZIRAN_BOUNDS_CHECK`; `ziran build --target=go` + `go run`.
Rejections via `tests/lib/expect_rejection.sh`: reset with a live view,
negative size, mark above the count, oversized request failing under the
bounded-resize ceiling.

**Done means.** Focused test green on all targets from source and saved IR;
`make check CHECK_FILTER=temp_arena` passes; `env -u DISPLAY -u
WAYLAND_DISPLAY make -j4 check` passes before commit; USAGE + status docs
updated.

**Size.** Hours (one short session). **Runtime.** Co-Z (std module, test,
docs); Claude review.

### Step 2 — Module parameters: `#import "X"(FEATURE=false)` (Group B)

**Owner-visible behavior.** Compile-time feature choice per import site;
inactive code never exists in the program:

```zi
// fonts.zi
CACHE :: 8;
Glyph :: (code: s32) -> s32 {
    #if STB { return StbGlyph(code) }
    return code
}
Table: [CACHE]s32;

// app.zi
Fonts :: #import "fonts"(STB = false);
App :: () -> s32 { return Fonts.Glyph(65) }   // StbGlyph is not compiled in
```

Rules: values are literals (integer, string, `true`/`false`/`null` — the same
class as `DefaultIsLiteral`); parameters are visible to `#if`, `#ifx`,
`#run`, constants, and type declarations inside that instance, like discovered
constants; two import sites with different parameter sets get two independent
module instances (distinct types and globals, initializers run per instance);
equal parameter sets share one instance; unknown, duplicate, or non-literal
parameters are check-time errors; only quoted module imports accept parameters
(`,file`/`,dir`/`,string` reject them).

**Files and functions.**
`cmd/compiler_declaration.zi` `ImportDeclaration` (:413) parses `(NAME =
literal, ...)` after the quoted target into a new `ModuleImport.parameters`
string plus error codes `.ImportParameterName/.ImportParameterValue/
.ImportParameterDuplicate`; `cmd/zir/zir_parse_declaration.c`
`parse_import_line` (:521) forwards it (imports are parsed line-based, so
parameters stay on one line); `cmd/zir/zir.h` adds `const char *parameters`
to `ZirImport` and extends `ModuleAddImport` (:238); `cmd/zir/zir_load.c`
`early_resolve_imports` (:1127) keys the module cache on `target + parameters`
and loads one `ZirModule` instance per distinct key (`ProgramAddModule` with an
instance-qualified name such as `fonts(STB=false)`); `cmd/zir/zir_parse_discover.c`
injects the parameters as discovered file constants before `#if` selection;
`cmd/zir/zir_serial.c` + `zir_load.c` record import parameters and instance
names and bump `.zir` version 48 → 49, rejecting older files; emitters add
per-instance name uniquing: `cmd/zi2c/zir_c_lower.c` (module stem +
cross-module symbol table) and `cmd/zi2go/zir_go_lower.c` (module prefixes —
**coordinate with the Kryon lane**, see above).

**Backend impact.** LIR: none (folding happens before lowering). ZX/Wasm:
receive only the folded instance, so they need no parameter awareness beyond
the version bump. C/Go: one copy of the module per distinct parameter set, with
instance-suffixed symbols and per-instance initializers in startup order.
Native: consumes LIR; unaffected. Saved IR: new required fields, version bump,
forged-parameter rejection.

**Test.** `tests/module_parameters.sh`: one program importing the same module
twice with different parameters — independent globals, both initializers run in
dependency order, inactive-branch names fail resolution with a diagnostic;
answer equality on C, Go, and `.zx` from source and saved `.zir`; rejections
for unknown/duplicate/non-literal parameters, parameters on `#import,file`,
and a saved-IR file with edited parameters; `tests/c_module_init.sh` and
`tests/go_package_init.sh` extended with one parameterized instance each;
`tests/features.sh` extended for the new contract.

**Done means.** Focused tests green on all targets from source and saved IR;
full `make check` passes; `docs/USAGE.md` Imports section documents the form;
`docs/IMPLEMENTATION_STATUS.md` and the parity matrix updated; FEATURES.json
byte-identical to `ziran features --json`.

**Size.** 2–3 sessions. **Runtime.** Claude (parser/checker/serializers/emitters);
Co-Z for tests and docs once the checker lands.

### Step 3 — DROPPED (owner, 2026-11-15) — One C translation unit per program (Group A, parallel with Step 1)

**Dropped.** Owner: "no oid nto wnat evreything to become one c file i want to noly have teh code we acutelly need!!!!". Fewer files is not the goal; generating only reachable code is (see AGENTS.md). Unreached-code pruning in the C emitter is the replacement work, owned by the Kryon pay-per-import lane together with cmd/zi2c.


**Owner-visible behavior.** `ziran build --target=c` (and `zi2c`) emits exactly
one `stem.c` plus one `stem.h` per program instead of one file pair per
module. A Kryon build's compile unit count drops from the hundreds to one;
`cc` parses one TU; Makefile-style `*.c` globs keep working unchanged.

**Files and functions.** `cmd/zi2c/zir_c_lower.c`: emit all modules'
declarations and bodies into one stream, reusing the existing cross-module name
uniquing; module-private procedures become `static`; per-module header
machinery (`header_visible_types`, `ZirCHeaderUses`, :1312–1425) collapses
into one root header exposing only the surface consumers include
(`#program_export` procedures, foreign declarations, and their reachable
types). `cmd/zi2c/main.c` + `zir_c_lower.h`: output-shape plumbing. Remove the
per-module splitter in the same change per the legacy-removal gate (no
compatibility flag).

**Backend impact.** C target only. Go, ZX, Wasm, LIR, native untouched. Module
initializer call order must be preserved in the single TU (one startup
sequence, import-first ordering as today).

**Test.** `tests/c_single_unit.sh`: a three-module program (one with a module
initializer, two declaring same-named file-private helpers, one
`#program_export`) must produce exactly `app.c` + `app.h` in the output
directory, compile with `cc -std=c99 -pedantic-errors -DZIRAN_BOUNDS_CHECK`,
and return the same answer from source and saved `.zir`; initializer order
asserted across the three modules; one assertion that the output contains
exactly one `.c` guards regressions. Existing suite (`c_module_init`,
`c_warnings`, `compile_run`, `vec`) then exercises the single-TU path by
default. Also verify Kryon's build rules include only the root header and glob
`*.c` (read-only check in the Kryon repo; coordinate any Kryon Makefile change
with its lane).

**Done means.** Focused test green; `make check` green; a measured line in the
implementer's report: file count and wall-clock `cc` time for a small
multi-module program before vs after.

**Size.** ~2 sessions. **Runtime.** Claude or Codex (C emitter); Co-Z for the
test and docs.

### Step 4 — `std/context`: scoped logger/context record (Group D, std only)

**Owner-visible behavior.** A portable context record libraries accept
explicitly, carrying the logger and scope tag without a global:

```zi
#import "context"

Serve :: (c: *Context, request: string) -> s32 {
    defer ContextLog(c, "done")
    ContextLog(c, "start")
    return request.count
}
```

`Context` holds a logger procedure value (capture-free named functions are
already supported), a tag string, and a payload slot kept as an owned
`Vec(u8)` instead of a raw pointer; `ContextLog` no-ops on a null logger.
Platform loggers stay in `std/stdio`; `std/context` itself stays portable.

**Files and functions.** New `std/context.zi`; new `tests/context_scope.sh`.

**Backend impact.** None below the language level; all four targets.

**Test.** `tests/context_scope.sh`: logger receives calls in order across
nested scopes; null logger no-op; record moves and views follow the
owned-value rules; C, Go, and `.zx` from source and saved IR.

**Done means.** Focused test green on all targets; `make check` green; USAGE
subsection written.

**Size.** Hours. **Runtime.** Co-Z.

The Jai-style **implicit** context (hidden parameter on every procedure,
`push_context`) is Phase 2, unscheduled: it changes every signature, the ABI,
effect classes, and all serializers, and should be designed against the
self-hosted checker (post-S5). Step 4 is its portable groundwork.

### Step 5 — `#insert`: file-scope compile-time source splicing (Group C, after Step 2)

**Owner-visible behavior.** The parser splices a compile-time string constant
as source, so checked declarations can be generated where generics cannot
reach, at parse time, identically on every target:

```zi
TABLE :: #run BuildDispatch();   // compile-time string of checked source
#insert TABLE;
```

`#insert CONSTANT;` at file scope splices a compile-time string constant as
source before discovery and checking; the spliced text is checked like ordinary
source (no unchecked path); diagnostics point into the constant with the
insertion site as context; runtime-dependent or effectful strings are
rejected; procedure bodies may not `#insert` (declaration scope only, like
Jai).

**Files and functions.** `cmd/zir/zir_parse_source.c` (file-scope directive
recognition), `cmd/zir/zir_parse_discover.c` (splice before `#if` selection,
sequenced after Step 2's parameter injection lands in the same file), grammar
helper in `cmd/compiler_parse.zi`. Serializer untouched: spliced declarations
are ordinary checked declarations. Deliberately outside
`cmd/compiler_zir_lower.zi` (S1 lane file).

**Backend impact.** None per target: all backends see the same checked
declarations. Saved IR is identical whether the program spliced the code or
wrote it by hand.

**Test.** `tests/insert_source.sh`: a `#run`-built string defines a procedure
that `Answer` calls — same answer on C, Go, and `.zx` from source and saved
`.zir`; rejections for a runtime-computed string, `#insert` inside a
procedure, and a spliced duplicate declaration.

**Done means.** Focused test green; `make check` green; `#insert` documented
in `docs/LANGUAGE_DIRECTION.md` (Jai source syntax section) and USAGE.

**Size.** 1 session. **Runtime.** Claude (parser); Co-Z tests/docs.

## Parallel groups

- **Group A (start immediately, disjoint files):** Step 1 (`std/temp.zi`,
  `tests/temp_arena.sh`) in parallel with Step 3 (`cmd/zi2c/*`). No shared
  files.
- **Group B (start immediately):** Step 2 parser/checker/serializer/`zi2c`
  sub-steps; the `cmd/zi2go/zir_go_lower.c` sub-step waits for the Kryon
  pay-per-import lane's fix (or rebases onto it).
- **Group D:** Step 4 (`std/context.zi`) — std only, anytime.
- **Group C:** Step 5 after Step 2 merges (both touch
  `cmd/zir/zir_parse_discover.c`).

## First step, and why

**Step 1, the per-frame temp arena**, with Step 3 launched in parallel and
Step 2 started in the same window where files allow. Justification: the temp
arena is the biggest win soonest by a wide margin. It is pure `std/` (zero
compiler risk, zero lane conflict), portable to every target on day one, and
Kryon's frame loop can adopt it immediately — Jai's per-frame
`Temporary_Storage` reset is exactly the pattern a retained-mode UI needs.
Module parameters are probably the bigger structural win overall, but they
cost 2–3 sessions across the loader, serializers, and two emitters and touch a
file another lane owns; starting them in parallel loses nothing, while making
them first delays the win every program can bank this week.

## Dropped and deferred

- **`#no_abc`-style bounds control — dropped.** It contradicts the safety
  rules this repository runs on: checked pointers with no fallbacks (owner,
  2026-11-14: "fallbakcs are illegal"), the roadmap rule that no backend
  quietly substitutes different bounds behavior, the `bounds` law kind, and
  the portable ZX model where every load is checked. A bounds-off spelling
  would fork the language's safety story for a speed win the optimizer and
  native lanes should chase instead. Nothing runs unchecked.
- **`#placeholder` — dropped.** Explicit constants and `#if` cover the use
  cases; a compile-time-forgiving unknown name hides real resolution bugs.
- **Notes/tags (`#note`), `#modify`, `type_info`, the compiler message loop,
  build-as-Ziran-program, `#bake_arguments`, SOA — deferred, unscheduled.**
  They all want metaprogramming infrastructure that should be designed for the
  self-hosted checker (post-S5) with explicit effect and resource limits, per
  the roadmap's compile-time-execution acceptance rules. `#bake_arguments` is
  convenience over defaults plus specialization — revisit after module
  parameters. SOA is real layout work across every backend; revisit against
  measured Kryon data-oriented hot spots. Notes only pay off with `#modify`.

Every step follows the roadmap's conformance workflow: smallest positive and
negative examples, source + saved-IR coverage, all applicable targets,
explicit diagnostics for unsupported target behavior, then `env -u DISPLAY -u
WAYLAND_DISPLAY make -j4 check` before any commit.
