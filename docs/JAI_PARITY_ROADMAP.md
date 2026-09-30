# Jai syntax parity roadmap

Status: 2026-09-27. This is a work plan, not a claim that Ziran already accepts
every Jai program. [Implementation status](IMPLEMENTATION_STATUS.md) records
what works today; [Language direction](LANGUAGE_DIRECTION.md) records the broader
design. The current checked source grammar aims to use Jai syntax. Ziran is
experimental: breaking changes are welcome, and there is no requirement to
load old source, `.zir`, `.zib`, flags, or runtime entry points.

## The finish line

For each supported Jai construct, the same program must parse, type-check, and
behave consistently when built from `.zi` source or saved `.zir`, on C, C++, Go,
and the portable `.zib` runner. A backend may explicitly reject a capability
it cannot implement yet. It must never quietly substitute a different layout,
evaluation order, visibility rule, or bounds behavior. Every removed Ziran-only
spelling needs a source rejection test, and saved IR must reject forged obsolete
forms. No compatibility shim is needed.

"Jai syntax parity" is a larger claim than passing the existing Ziran suite.
Build a conformance corpus from runnable Jai examples and compare diagnostics,
inferred types, constant values, layouts, and observable execution with a Jai
compiler whenever one is available. The public [OpenJai language
specification](https://github.com/withlang-dev/open-jai/blob/main/docs/open_jai_spec.md)
is useful for finding candidate cases, but it describes an independent
implementation and cannot settle differences from Jai itself. Record uncertain
behavior as an open question and probe it before codifying a language rule.

## Current baseline

The current branch has Jai-style declarations, lexical `using` for local and
parameter records, nested and generic `using` fields, lexical enum `using`,
data-scope enum `using`, and data-scope promotion from module-local record and
scalar-union globals in procedure bodies, including public globals from open
and named imports. It supports typed array literals, including empty arrays, typed and
inferred record literals, named and default arguments, direct polymorphic
calls, scopes, `#load`, several `#import` forms, bounded pure `#run`, and
checked `.zir` graphs. The compiler rejects many inherited spellings,
including `#enum`, `variant`, postfix `?`, `#global`, `#export`, `#private`,
`#extern`, C pointer suffixes, C conditional expressions, and old Kryon file
extensions. The rejection tests should stay.

Re-run the full local `make check` suite after any change to
the checker, IR, or emitters, with `DISPLAY` and `WAYLAND_DISPLAY` unset.

| Area | Current subset | Next proof of parity |
| --- | --- | --- |
| Names and scopes | Local/field record `using`; local/data enum `using`; imported record globals in procedures; named imports | File-scope record lookup, full imported type ownership, order independence |
| Values and expressions | Record literals, typed arrays including zero capacity, direct generic calls, `ifx` in typed expressions and call arguments | Jai ABI verification and full `.data` contract, broader lazy-expression audit, fuller type queries |
| Procedures | Named values, defaults, named arguments, direct polymorphism, overloads, binary `operator` procedures with `#symmetric` | Variadic, `operator []`, unary operators, and broader procedure-form audit |
| Compile time | Bounded pure scalars, strings, floats, and record/array results used at compile time, in checked runtime expressions, and in selected file-scope initializers, including public imports | Full file-scope initialization semantics, verified effect rules |
| Native output | C/C++/Go checked body graphs; C/C++ unions and scalar Go unions; imported and keyword record, union, enum, global, and constant names, including global/constant and function/value collisions; C/C++ keyword exports and header guards; Go module filenames and prefixes; valid C/C++ private names for punctuation in root filenames | Declaration lowering, remaining whole-program names, non-scalar Go union layout |
| Portable output | Verified `.zib` scalar/record/array subset, scalar unions, and host calls | Remaining globals, aggregates, pointer casts, non-scalar unions, host shapes |
| Ownership | Scoped `Vec(T)` operations | Move, drop, pop, lookup, builder, borrowing across calls |

This matrix records known implementation work, not an exhaustive catalog of
Jai syntax. Each completed item should move from “next proof” to a tested
current subset with an exact test name and target list.

## Priority 1: close source grammar and name-resolution gaps

### 1. Data-scope `using` for record values

Top-level `using value;`, nested paths, and `using value: Record;` now promote
module-local record and scalar-union global fields inside procedures. Public
globals from open imports and named imports resolve for reads, writes, and
record `using` in procedures. `tests/imported_globals.sh` exercises source and
saved IR on C, C++, Go, and `.zib`, including ambiguity and private visibility.
The
checker lowers references to ordinary checked member access before `.zir` is
saved. `tests/data_scope_using.sh` covers reads, writes, filters, forward
declarations, local shadowing, collisions, `#scope_file` across `#load`, and
source/saved-IR execution on C, C++, Go, and `.zib`. `using StructType;`
still rejects a type in place of a value.

Pure global initializers now resolve fields opened by file-scope record
`using` and fold references to earlier initialized globals. Source and saved
IR agree on C, C++, Go, and `.zib`; `tests/data_scope_using.sh` covers nested
paths, filters, local shadowing, ambiguity with another record or enum, and
file-private visibility across `#load`. Checked runtime-dependent global
initializers now run once per module instance; broader startup effects and
record promotion in compile-time conditions remain open. Verify whether Jai
opens exported globals from open imports unqualified. Named and open imported
globals now retain their declaring record type when the consumer has a
same-named type; `tests/imported_globals.sh` checks direct and `using` field
access on all four targets, from source and saved IR. Open imports gain a
private alias in checked IR so the type owner remains unambiguous. Verify the
exact record and union rules with Jai before claiming syntax parity. Keep
visibility and ambiguity diagnostics consistent with local `using`.
Qualified imported enum type annotations and `Module.Enum.Member` expressions
now lower across native targets and the portable runner; see
`tests/imported_enum_names.sh`.

### 2. Declaration-order-independent compile-time lookup

The parser now discovers unconditional, single-line file constants and
`using` declarations before selecting `#if` branches. `#if` and file-scope
`#run` can refer to later constants, including chains of references; cycles
report an explicit error. A condition can also see enum members opened by a
later `using` when the enum type is already declared. The filter and file
visibility rules still apply. `tests/compile_if_syntax.sh` covers these cases
on source and saved `.zir` for C, C++, Go, and `.zib`, and checks that names
from inactive branches stay hidden. Ordinary named and open `#import`
declarations are also discovered before condition selection, so a later import
can provide a pure procedure, scalar integer constant, or imported type layout
to `#if` and `#run`.
`tests/forward_import_conditions.sh` covers source and saved IR on all four
targets, and excludes imports in inactive branches. Unconditional later record
and plain enum declarations with known bodies are discovered for conditions;
`tests/forward_type_conditions.sh` covers multiline records, Jai scalar aliases,
`size_of` in `#if` and `#run`, an enum opened by a later `using`, source/saved IR
on all four targets, and inactive type isolation. Unconditional later
`enum_flags` types provide implicit and explicit members in conditions,
including `#specified` types and a later `#load`; the test also covers
file-private and inactive visibility. Direct `size_of(Box(s64))` and a later
one-line concrete alias `LaterBox :: Box(s64)` also resolve in `#if` and
`#run`; the alias works without a trailing semicolon, while inactive and
file-private aliases remain hidden. Multiline record fields with semicolons
lower the same way as inline fields. A later unconditional record, enum, or
generic record can have `#if`-selected fields or members when its condition
resolves from discovered declarations. The prepass selects only the active
fields before computing `size_of` or opening enum members, including a type
from a later `#load`; `tests/forward_type_conditions.sh` checks source and
saved IR on all four targets and file-private visibility. Conditional type
discovery retries unresolved bodies after later types become known, including
an earlier record depending on a later conditional record and an earlier enum
depending on a later conditional enum. An unresolved condition still fails;
inactive nested conditions are not evaluated. Unconditional nested
`#load` files also contribute public constants, types, `using` declarations,
and imports to earlier conditions; file-private names remain visible only in
their declaring file. `tests/forward_load_conditions.sh` checks source and
saved IR on all four targets, private visibility, and inactive loads. This is
still a limited pass: cyclic conditional type dependencies, other
generic-instance declaration forms, embedded source imports, and imports in
conditionally selected `#load` files have ordering limits. Named imported real
and string constants work in
`#if` and `#run`, including an import declared later in the file;
`tests/imported_typed_constants.sh` checks them on source and saved IR across
all four targets. Imported aggregate fields and array elements also work in
tested `#if` conditions, including a later named import;
`tests/imported_aggregate_constants.sh` covers this. Broader import forms
remain open.
Unconditional procedure headers and single-line typed globals in the current
file or a later `#load` now resolve forward `#if size_of(type_of(...))`
queries without evaluating the operand. Procedure parameter lists may span
lines; global member queries can use a later record type. `tests/file_type_of.sh`
checks source and saved IR on C, C++, Go, and `.zib`, and keeps inactive and
file-private names hidden. Branch conditions can also call an unconditional
pure procedure declared later when its body has local declarations,
assignments, braced `if`/`else if`/`else`, bounded `while` loops, and a return,
including a procedure in a later `#load` or with a multiline header. The body
can call another later procedure, and `#run` accepts the tested branch and
loop cases. Defaults, named arguments, and
later constants use the bounded evaluator, including a loaded procedure's
file-private constant; recursion and loops share an instruction budget. Other
forward body forms still require discovery; executed effects are rejected.
File-scope `#run` now uses the same discovered module scope, so a constant
computed by a later procedure can select a following `#if` branch.
Token normalization now waits for branch selection, so obsolete
primitive spellings in inactive top-level, type, and procedure branches are
ignored while selected `else #if` conditions still use Jai scalar aliases.
`tests/compile_if_syntax.sh` covers these cases. The OpenJai reference
describes top-level declaration order as
independent. Investigate Jai's actual behavior, then extend top-level discovery
or add bounded dependency resolution. Do not resolve a condition by parsing
code from an inactive branch or by changing file-private visibility.

Acceptance: a forward constant, import, type, and `using` in conditions and
`#run` resolve when legal; cycles produce an explicit diagnostic; source and
saved `.zir` contain only selected declarations; `#load` and embedded imports
retain their visibility rules. Relevant code is in `cmd/zir/zir_parse.c` and
the compile-time evaluator there, with `tests/compile_if_syntax.sh`,
`tests/compile_run.sh`, and `tests/using.sh` as starting points.

### 3. Import re-exports and composition modifiers

`using Alias :: #import "Module";` re-exports public procedures, types, and
constants beside the alias with local shadowing and ambiguity diagnostics.
`using,only(...)`/`except(...)`/`map(...)` modifiers now
filter and rename promoted fields and enum members with hidden names staying
unresolved, including file-scope enum uses in constants, global initializers,
`#run`, `#assert`, and `#if`. Conflicting maps of one exposed name are
diagnosed. `#as (source: Type) -> Result` conversions apply implicitly at
every coercion site through a checked call rewrite shared by all targets. In particular, `#as` affects
conversions, so a text substitution in a backend is insufficient. Model
namespace import and conversion metadata explicitly, resolve it in the
checker, and serialize the checked result.

Acceptance: importing through a re-export preserves public/private scopes;
`#as` conversions work for values and pointers where legal; all targets agree
or explicitly reject an unsupported ABI. Cover nested imports and saved
`.zir`. The [OpenJai `using`
examples](https://raw.githubusercontent.com/withlang-dev/open-jai/main/docs/open_jai_spec.md)
give candidate syntax, to be checked against Jai.

### 4. Complete the remaining procedure and expression grammar

Audit Jai examples against the parser instead of inferring coverage from a
handful of demos. Overload resolution and binary operator declarations and
calls are in (`tests/overloads.sh`, `tests/operators.sh`); `operator []`,
unary operators, variadic arguments, local and anonymous procedures,
more general procedure values, polymorphic procedures as values, and more
forms of type application. These have not been certified by Ziran's present
conformance tests. Implement one family at a time with a typed IR shape and
clear evaluation-order rules. Extend the existing direct polymorphic call
support to field expressions in specialized record defaults and defaults on
procedure-type values.

`ifx` and `#ifx` now work as positional and named call arguments in the
tested scalar subset, including source/saved-IR builds and entry pruning.
`#ifx` also selects record and fixed-array results in file constants, global
initializers, local definitions, and call arguments; `tests/compile_ifx.sh` checks source and
saved IR on C, C++, Go, and `.zib`. Audit remaining lazy-expression placements
and aggregate forms.
Forward and imported `size_of(type_of(...))` operands now resolve in tested
file-scope constants, global initializers, `#run`, and `#ifx`; imported
operands also work in `#if`. `tests/file_type_of.sh` checks source and saved
IR on C, C++, Go, and `.zib`. A local declaration may name its type with
`type_of(expression)`, as in `b: type_of(a + 1) = 5`; the checker replaces it
with the concrete type (`tests/range_types.sh`). `type_of` in parameters,
fields, and globals, broader forward `#if` queries, and foreign-record
`size_of` remain open.
Put each accepted expression through source, saved IR, and all applicable
targets. Rejection for an unavailable construct must occur in checking, not
as a later C/Go compiler error.

### 5. Array syntax and true zero capacity

`T.[]` and context-inferred `.[]` now work for zero-capacity fixed arrays.
`tests/zero_arrays.sh` checks locals, globals, aliases, nested arrays, records,
arguments, returns, open and named imported-bound aliases in globals,
records, signatures, and nested arrays, plus fixed-array aliases exposed
through two public `using` import re-exports. Private and ambiguous re-exported
aliases are rejected. The test also checks `size_of`, `.count`, null `.data`,
and index failures from source and saved IR on C, C++, Go, and `.zib`.
The native C/C++ output uses
GNU-compatible zero-array extensions, which compile under the strict C99 and
C++ modes used by the test. The portable VM allocates an array descriptor with
zero elements. No fake element contributes to layout or storage.

`tests/host_slices.sh` covers a zero-length slice borrowed from a zero-capacity
array and a host-returned empty slice across source and saved IR. Broaden the
slice borrowing audit. Generic records can contain zero-capacity arrays, and
generic procedures can take and return them; `tests/generic_record.sh` and
`tests/generic_procedure.sh` cover source and saved IR on all four targets.
Alias behavior in more complex generic signatures still needs a broader audit.
`tests/host_arrays.sh` checks fixed-array foreign arguments and returns on
C99, C++17, Go, and portable bundles; compare the native ABI with Jai before
claiming parity. Nonempty arrays also need the full Jai `.data` contract
rather than using native C array layout as proof.
The OpenJai reference describes an empty typed literal with count zero and
a null data pointer; verify that behavior with Jai itself before claiming
exact language parity.

## Priority 2: make the checked language portable and predictable

### 6. Finish compile-time execution

`#run`, `#if`, `#ifx`, and `#assert` currently evaluate a bounded pure subset.
Pure procedures can return named records and fixed arrays for compile-time
field selection, array indexing, `.count`, and assertions. Local aggregate
definitions now bind to checked runtime expression graphs; direct global
initializers can use them. Public aggregates resolve through named, open,
and using imports with their declaring record type preserved. The checker
now preserves imported aggregate type ownership even when the consumer
declares same-named records; native output assigns distinct names to them.
`tests/compile_values.sh` and
`tests/imported_aggregate_constants.sh` check source and saved IR across
all four targets, including imported record fields and array elements in
`#run`, `#assert`, `#if`, and pure file-scope initializers. Direct calls with
record parameters or results and qualified record literals also work in the
tested runtime expressions. File-scope field
selection, indexing, scalar arithmetic, and `#ifx` selection fold to typed
literals before native or portable output. Bounded pure procedure calls also
fold in globals; checked runtime-dependent calls execute during module startup
in the portable bundle. General file-scope initialization,
effects, and general metaprogramming remain open.
`#caller_location` defaults now capture the call site in compile-time
`#run`, `#assert`, and local or imported `#if` calls, including nested calls and
explicit overrides. `tests/caller_location.sh` checks those through source
and saved IR on the portable VM and C, C++, and Go targets. Integer
conditions now send procedure calls through the typed evaluator, and the old
integer-only procedure interpreter has been removed. The integer expression
parser remains for `#if` predicates and compiler-host probes such as
`defined`; `tests/compile_run.sh` covers overflow, defaults, imports,
recursion, and one work budget shared by nested calls. Continue consolidating
expression rules, then extend file-scope initialization and add
supported metaprogramming features with explicit effect and
resource limits. Avoid leaving two subtly different language interpreters.

Acceptance: supported compile-time programs produce the same typed constants
in source and saved IR on every target; non-constant or effectful code fails
with a source span; recursion and instruction limits are deterministic. Use
`tests/compile_run.sh`, `tests/compile_values.sh`, and
`tests/compile_time.sh`.

### 7. Complete checked IR and native backend equivalence

Function bodies use checked expression graphs, but declaration and import
lowering still contain target-specific text handling. File-scope record and
array initializers now share a parsed expression-tree lowering path on C,
C++, and Go, including nested values and reordered record fields;
`tests/portable_globals.sh` compares them with saved IR and `.zib` and checks
that mismatched scalar, record-field, and array-element initializers fail in
the shared checker. Replace the remaining textual lowering with typed
declaration/import nodes or a checked target-neutral lowering plan. Audit name
mapping across an entire program: top-level types, exported procedures,
foreign symbols, imports, target keywords, and names differing by case or
underscores. Test C99, C++, Go, mixed source/IR input, and malformed saved
IR. Bump the experimental `.zir` version whenever its contract changes and
reject earlier versions; there is no migration path to maintain.

Scalar and enum unions overlap storage on C, C++, Go, and `.zib`. Go uses a
byte backing with the largest field's alignment; `tests/union.sh` checks 1-,
2-, 4-, and 8-byte C, C++, and Go sizes and alignments, plus overlapping
scalar field access and independent value copies from source and saved IR on
all four targets. String, record, array, and pointer fields
remain unsupported on Go and `.zib`. Select layout-preserving representations
for those fields, then test nested layouts, copies, and host boundaries.

### 8. Expand `.zib` and host capabilities

The portable verifier and VM support a useful scalar/record/array subset, and
carry raw pointers as opaque host handles that can be stored, compared, and
passed across host capabilities. They still reject slices inside globals or
records, unions with non-scalar fields, and several host shapes. Literal
scalar, string,
record, and fixed-array global initializers run portably, including nested
record and array literals across native targets. Bounded pure procedure calls
in initializers fold before output. Checked runtime initializers now execute
once per module instance, with imports initialized first; `tests/runtime_globals.sh`
checks scalar, record, and array results on C, C++, Go, and `.zib` from source
and saved IR. Broader startup effects and cycle diagnostics remain open.
Go rejects startup calls that need a host binding before `init` runs.
Host calls support scalar, string, plain-record, pointer, and fixed-array
arguments and returns, including zero and nested arrays with checked shapes.
Synchronous scalar, string, and plain-record slice arguments and returns work
from source and saved IR on native C, C++, Go, and `.zib`; the VM rejects
malformed host results. Procedure slots remain open. Implement each boundary as
a versioned checked contract in `cmd/zir/zir_bundle.c`, `cmd/zir/zir_vm.c`, and
`include/ziran_host.h`. Retain preflight verification so a bundle cannot reach
an unsupported operation at runtime. Test ownership and repeated instance
runs, not only one-shot calls.

### 9. Owned storage and standard-library usability

`Vec(T)` has checked push, indexing, clear, free, swap, `VecPop`/`VecGet`
through `Option(T)`, a `Vec(u8)` string builder, and move/drop rules for
assignment, argument passing, and return, including use-after-move, leak, and
double-drop rejection with `defer`-based cleanup, an explicit `VecClone`, and
`VecSlice` borrowed views with live-borrow rejection, and automatic drops at
every scope exit with move zeroing. The scoped
[Owned values](OWNED_VALUES.md) contract is complete. Match C/C++/Go and VM behavior on failure and
destruction; document
where Go cannot recover from physical allocation failure. This is needed for
ordinary non-UI programs to use Ziran without writing a new storage layer.

## Priority 3: broader language ambitions

Jai syntax parity does not itself deliver Ziran's planned laws, automatic CPU
parallel execution, GPU target, or complete platform integration. Keep those
as separately gated milestones in [Language direction](LANGUAGE_DIRECTION.md).
First specify observable semantics and prove a serial reference path; then add
parallel and GPU implementations with race, determinism, and backend tests.
Kryon and downstream application cutover belong to their repositories and
must not introduce UI-specific compiler branches here.

## Conformance workflow for every item

1. Save the smallest positive and negative Jai examples, their expected types,
   diagnostics, and output. Mark unverified examples as hypotheses.
2. Add Ziran source tests and forged saved-IR rejection tests for the rule.
   Include imports, `#load`, file-private visibility, and a target keyword
   collision when relevant.
3. Make the parser, checker, serialization, emitters, linker, verifier, and VM
   agree. Unsupported target behavior must have an explicit diagnostic.
4. Run the focused test, then `env -u DISPLAY -u WAYLAND_DISPLAY make -j4
   check`. Any display-dependent test runs only under a private Xvfb/Xephyr
   display. Never inherit the developer's desktop display.
5. Update `IMPLEMENTATION_STATUS.md` and the conformance matrix with the exact
   newly supported subset. Commit directly to `master`; use breaking format
   versions and remove superseded execution paths rather than preserving them.

## Legacy-code removal gate

An audit of current source paths found no executing Kryon compiler/runtime
mode, `.kry`/`.kir`/`.krb` loader, or old directive implementation. Mentions
of old spellings in `cmd/zir/zir_parse.c` are diagnostics that enforce their
rejection; the corresponding tests should remain. Compile-time procedure
calls now use the typed evaluator; the older integer-only procedure
interpreter has been removed. An integer expression parser still handles
predicates and compiler-host probes such as `defined`. Continue consolidating
expression rules with behavior tests. Future audits should distinguish
rejection code from executable legacy behavior and remove the latter in the
same commit that replaces it.
