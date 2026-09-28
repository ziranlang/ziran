# Language direction

This records the intended language requirements behind the two-repository
split. It is a target, not a list of features already shipped. See
[Implementation status](IMPLEMENTATION_STATUS.md) for the current compiler.

## General language first

Ziran must be useful for command-line tools, services, libraries, numerical
programs, and UI applications. Kryon is the first demanding library client,
not the definition of the language. Modules, typed records, ordinary calls,
procedure values, FFI, and host capabilities have the same meaning
whether a program imports Kryon or never uses a display.

The standard library belongs to Ziran and provides general facilities.
Kryon's widgets, layout, rendering, styling, and accessibility remain in
Kryon. No copied `kry_std` tree or UI-specific standard-library namespace is
part of the end state.

## Laws and LLM development

Laws are executable or mechanically checked constraints on a program's
behavior and interfaces. They must have stable names, source spans, and
machine-readable results, so an LLM-driven edit can identify what failed and
repair it. A law is a build gate: an unsupported or unproved obligation cannot
be silently treated as passed. Tests complement laws by checking examples and
integration behavior; they are not presented as proofs.

The intended development loop is to state invariants, implement code, check
laws, run conformance tests, and retain the checked result in `.zir` and
`.zib` metadata where needed for independent validation. Diagnostics should
identify the violated law and the smallest relevant source span. This is a
tooling requirement for LLM-generated code and human-written code alike.

### Law contract

A law obligation is a named, mechanically decidable check bound to a source
span. The contract has four parts.

1. **Declaration.** `#law NAME kind payload;` binds the stable identifier
   `NAME` to one obligation at file scope. `kind` selects the checker:
   `type`, `effect`, `bounds`, `size`, `abi`, `custom`, or `forall`. `payload` is kind-specific
   checked source — a boolean procedure for `custom`, a shape description
   for the built-in kinds. A law with no checker for its kind is an error,
   never a pass.
2. **Result schema.** Every check emits one JSON object per obligation:
   `{"law": NAME, "kind": K, "span": {file, line, column},
   "status": "proved" | "disproved" | "unknown", "detail": string}`.
   `unknown` is emitted when the checker lacks the assumption or budget to
   decide; `detail` then names the missing capability. Build gates treat
   `disproved` as an error and `unknown` as an error unless explicitly
   waived for that law name.
3. **Gate semantics.** `ziran check` evaluates every reachable law and exits
   nonzero on any `disproved` or unwaived `unknown`. Generated artifacts
   embed the result set: `.zir` stores per-module law tables and `.zib`
   stores the transitive closure of laws its linked modules reached, so a
   consumer can refuse a bundle whose laws were not all `proved` at save
   time. Waivers are explicit `#law_waive NAME reason;` declarations with
   their own spans and ride along in the same tables.
4. **Determinism.** A law result depends only on the checked program text
   and declared kinds, never on wall-clock time, iteration order, or backend
   choice. The same module set must produce byte-identical law tables in
   repeated runs, which the test matrix enforces the same way it enforces
   `.zib` byte equality.

The compiler now implements named `type`, `effect`, `bounds`, `size`, `abi`,
`custom`, and `forall` laws, writes law tables to checked `.zir` and linked `.zib`, and
reports results as JSON. This remains narrower than the intended proof
contract: `custom` evaluates closed compile-time expressions, `forall`
(`#law N forall x: 0..4, k: SomeEnum => condition;`) checks every combination
of integer ranges and enum members through the same evaluator (a domain over
1,000,000 cases or an undecidable condition is `unknown`, a failure reports
the counterexample), `abi` checks
the supported signature shape rather than proving foreign behavior, and law
identity and waiver validation still need tightening. The contract above
describes the intended guarantees where it exceeds this implementation.

## Parallel computation

Independent computation should be expressible without hand-written thread or
GPU management in every call site. The language and runtime must define which
operations may run concurrently, their memory and effect rules, and the
observable ordering guarantees. CPU and GPU implementations must preserve
those semantics; unsupported execution paths must report a capability or
target error instead of silently changing the result.

### Parallel execution contract

1. **Effect classes.** Every procedure has one of four effect classes derived
   from its checked body: `pure` (reads only its arguments and immutable
   globals, allocates, and calls only `pure` procedures), `observing` (also
   reads mutable globals or host observations), `mutating` (also writes
   through pointers or mutable globals it does not own), and `external`
   (calls `#foreign` code whose effects the checker cannot see). The class
   is recorded in `.zir` signatures and is part of the ABI: a caller sees
   the callee's declared class, not its implementation.
2. **Concurrency granularity.** `#parallel for` over a range or fixed array
   executes iterations concurrently when every called procedure in the loop
   body is `pure` or `observing` and the body writes only to storage whose
   lifetime is one iteration. Any other body is rejected at check time with
   the offending span; the compiler never silently serializes a loop marked
   `#parallel`.
3. **Memory rules.** Concurrent iterations share no mutable storage. Writes
   to `Vec(T)` storage, host slices, and pointer-backed records inside a
   parallel body are rejected unless the storage is declared `#local` to the
   iteration. Allocation inside an iteration is private to it; a result
   moved out of the loop body must be owned (`Vec` moves satisfy this).
4. **Observable ordering.** The only ordering guarantees are: effects of an
   iteration appear between the loop's entry and exit; `#parallel` regions
   do not nest; and the loop completes before any statement after it runs.
   Interleaving between iterations is unobservable: any program whose
   result differs between two legal interleavings is rejected by the effect
   and memory rules above, not by runtime detection.
5. **Backend duties.** A backend that cannot execute a legal `#parallel`
   region concurrently must still produce the same result serially and
   report the downgrade through a capability flag, never through a changed
   value. The portable VM executes regions serially with the same effect
   checking; native targets may use threads; a GPU target must reject
   `external`-class calls and pointer arguments before offloading.

Parallel execution is a target requirement. Effect classes and the
`#parallel for` acceptance rules above are implemented and checked on every
target. Native C and C++ execute forward regions on pthreads with
iteration-local workers and read-only captures; the portable VM and Go run
regions serially with a reported downgrade, which the backend-duties clause
permits. `#parallel_gpu for` regions enforce the GPU clause — pointer
storage and pointer-bearing arguments are rejected before offload — and, with
no GPU device capability in the repository, run on the CPU worker path with
an emitted downgrade marker. Offload to a concrete GPU API remains future
work.

## Jai source syntax

Ziran's source grammar targets Jai syntax. Polymorphic type declarations use
`Box :: struct($T: Type) { value: T; }`, and named type values use
`IntBox :: Box(s32)`. Bracketed declaration parameters and the `specialize`
keyword are no longer accepted. The compiler rejects Ziran-only `#enum` and `variant`
declarations, `match` statements, postfix `?`, `guard`, C-style `switch` and
`goto` with labels, `state` blocks, C-style locals, and raw C statements in
source. Jai enum cases use `if value == { case ... }`. Native builds run the
language checker unconditionally; `--strict` and `--no-strict` are rejected. Other syntax
differences remain under audit.
Direct polymorphic procedure calls use `Identity :: (value: $T) -> T {
return value }`. A procedure may bind one type name, use it in later parameter
types and its result, and call another polymorphic procedure. Calls specialize
for concrete argument types, including records from an importing module.
Untyped numeric literals need a concrete context to infer `T`. Polymorphic
procedures cannot be used as procedure values or exported with
`#program_export`.
Calls accept Jai named arguments such as `Combine(second = 2, first = 1)`.
Names bind to declared parameters, and argument expressions run in written
order before the call. The checker rejects unknown, duplicate, and missing
arguments. Named arguments work with direct, imported, polymorphic, and
procedure-value calls. Procedures accept defaults in parameter declarations,
including inferred `value := expression` defaults and defaults before required
parameters. Inferred defaults use the checked expression type, including
imported constants and procedure results. A missing default expression is
evaluated after the caller's explicit argument expressions. Defaults on
procedure-type values remain unsupported. `#caller_location` is a parameter
default of type `Source_Code_Location`; omitted arguments receive the calling
source file and physical line, including through imports and `#load`. Concrete
procedure defaults use declaration-scope name
resolution. Polymorphic defaults with a concrete expression type also use a
checked helper in the declaring module. Scope-independent literals retain
their call context for type inference. Literal record defaults such as
`T.{value = 42}` use the specialized parameter type at the call. Defaults
such as `T.{value = Base()}` evaluate concrete field expressions in the
declaring module while retaining the specialized record type at the call.
Field expressions that themselves require the specialized type still need
specialization support.
Pure named calls with supported scalar values also work in `#run` and other
compile-time expressions.
Inside a procedure, `#compile_time` is a boolean that is true while the
procedure executes during compilation and false at run time. `#run` evaluates
it in the compile-time phase; it cannot be declared as a constant or used
directly as a `#if` condition.
`#must` after a procedure result type requires its value to be used. The
checker rejects discarded results from ordinary, imported, polymorphic, and
foreign calls before native or portable output, including when checking saved
`.zir`.
The `#slot` declaration, capturing body forms, and record block calls are
rejected. Record values use `Type.{field = value}` in ordinary calls.
`using record: Type` parameter or local, `using record;` after a local or
parameter declaration, or `using entity.position;` for a nested record,
makes that record's fields available by name
within the lexical scope. These references become ordinary checked member
accesses in saved IR. Explicit locals shadow promoted fields, and ambiguous
promotions are errors. A struct field declared `using field: Record` exposes
that record's fields while preserving the contained storage; nested and
generic record applications keep the same behavior. Native targets also
accept pointer-backed `using` fields. Polymorphic procedures retain `using`
parameters and body declarations through specialization and saved IR.
Inside a procedure, `using Enum;` opens an enum's members within lexical scope.
At data scope, `using Enum;` opens them for procedures and file-scope constants,
globals, `#run`, `#assert`, and following `#if` conditions. The opened names lower to
checked values in `.zir`. A `#scope_file` using stays in its source file across
`#load`; named imports accept `using Module.Enum;`. Without `using`, bare enum
members require a type qualifier or an expected enum type for `.Member`.
Data-scope `using` for records and unions, and `#if` before a later `using`
declaration, remain unsupported.
Public `using` import re-exports, checked `#as` conversions, and supported
`using` modifiers now work in the tested subset. Broader data-scope using and
forward discovery remain language gaps.
A procedure type can be named with `Child :: #type (s32) -> ();` or
`Compute :: #type (s32) -> s32;` and passed a named
function value. Capture-free procedure values can be stored in records, fixed
arrays, and globals, and returned from functions. The callbacks do not capture
caller locals, so callback code uses explicit state or a named function.
Procedure type results are checked across the portable runtime and generated
targets. `#c_call` procedure types use C callback pointers in native C and
C++ output and may be stored in native records. A named Ziran function with
the exact callback signature can supply a `#c_call` value in those targets.
Reachable `#c_call` values cannot enter portable bundles, and Go output rejects
these declarations.
Direct type applications such as `Box(s32)` now work in
record fields, function signatures, globals, and local declarations. These
extensions are tracked as syntax gaps rather than described as Jai features.
Type applications may nest, and whitespace around their arguments does not
change the resulting type.
The expression parser accepts Jai `cast(Type) value`, `Type.{field = value}`,
and inferred `.{field = value}` when a record type is known from a declaration,
assignment, return, call parameter, or enclosing record field.
`defer { ... }` blocks preserve statement order at scope exit and run after
later defers in the same scope. A single deferred expression or assignment
uses the same cleanup lowering.
Inclusive integer ranges use `for i: first..last { ... }`, with `for
first..last { ... }` binding the value as `it`. Each iteration also binds
`it_index`, starting at zero. `for < i: first..last { ... }` visits the same
range in reverse. Bounds are evaluated once; `continue` advances the range,
and the endpoints do not overflow when the last iteration completes. The
compiler rejects C-style three-clause `for` headers. Fixed arrays and
borrowed slices use `for values { ... }` or `for value, index: values { ... }`;
`for < values` visits elements in reverse. The default value binding is a
copy. `for * value: values` binds a pointer to each element in native builds,
so the loop can modify the collection. Collection expressions are evaluated
once. Custom Jai `for_expansion` iterables remain unsupported.
`break name` and `continue name` target an enclosing `for name: ...` loop or
`while name := condition` loop. Loop exits run defers for every scope crossed,
and range continuations advance the named range before starting its next
iteration.
`if`, `while`, and `for` may use one following statement as their body without
braces. `else` attaches to the nearest unmatched `if`; a brace on the next
line begins an ordinary block body. Jai's optional `then` separates an `if`
or `else if` condition from its body, on the same or a later line. An `else`
may also carry its single-statement body on the same line.
Semicolons may separate declarations and statements on one line. One-line
record bodies accept semicolon-separated fields; expression delimiters such
as the semicolon before a `#ifx` else arm retain their expression meaning.
Fixed arrays, borrowed slices, and strings expose Jai `.count`. Counts have
`int` type and cannot be assigned. The older `.length` source spelling is
rejected for strings, slices, and fixed arrays.
String ranges use `value[low:high]` and return an immutable borrowed `string`
view over those bytes. Bounds are checked, and callers must keep UTF-8
codepoint boundaries intact when they need valid text.
`TextView(bytes)` makes an immutable borrowed `string` view of a `[]u8` slice.
The caller keeps the byte storage alive while the text is used. The portable
bundle runner copies those bytes into its managed execution storage. The
standard library's `TextUntilNul(buffer)` accepts a bounded byte slice and
returns a view ending at the first zero byte, or empty text if none is found.
An ordinary record may still declare a field named `length`; generated C ABI
structures also retain their internal length fields.
Array declarations accept `.[values]` with an expected fixed-array type and
explicit typed literals such as `s32.[1, 2]` or `Module.Point.[first, second]`.
Saved `.zir` preserves these spellings. Zero-element typed and contextual
literals work in locals, globals, records, returns, defaults, and generic calls
across C, C++, Go, and portable bundles. C-style casts and compound literals
are rejected at the source boundary.
Jai `#this` selects the enclosing procedure for recursive calls and typed
procedure values, even if a local binding has the same name. In record field
types it names the enclosing struct, including a concrete application of a
polymorphic struct. Saved `.zir` keeps procedure self references explicit.
Source uses Jai primitive spellings `s8`, `s16`, `s32`, `s64`, `float32`, and
`float64`; checked IR keeps those spellings. Jai `float`
is an alias for `float32`. The old `i8`/`i16`/`i32`/`i64`/`f32`/`f64`/`double`
spellings are rejected in source.
`char` is rejected as a type; it can name a local binding, record field,
global, or constant.
Jai `int` is an alias for `s64`. `size_of(Type)` supports scalars, pointers,
`string`, borrowed slices, fixed arrays, records, unions, and concrete generic
type applications such as `Box(s32)`. File-scope constants, global
initializers, and compile-time directives fold supported layouts during
translation. Function-body expressions remain symbolic in checked source and
saved IR, then lower to the selected target's size operation; consequently a
borrowed slice is 16 bytes in C, C++, and the portable layout but 24 bytes in
Go. An untyped integer local inferred with `:=` also has `s64` storage.
The C `sizeof` spelling is rejected. In function bodies,
`size_of(type_of(expression))` uses the checked expression type without
evaluating the expression. File-scope queries also infer types from previously
declared same-module expressions in constants, globals, and compile-time
directives. Standalone `type_of` values and forward or imported file-scope
queries remain unsupported. `size_of` for foreign records still needs their
Jai layouts.
Jai `#line` is an integer expression containing its physical source line.
It works in declarations, compile-time conditions, defaults, and procedure
bodies, including files introduced by `#load`.
`#file` yields the full source filename and `#filepath` its containing
directory. `#procedure_name()` yields the enclosing procedure's declared name
as a string, including when used in a default or a `#run` call.
`#caller_location` is valid as a procedure parameter default. It supplies a
`Source_Code_Location` record with `fully_pathed_filename` and `line_number`
from the call site; an explicit argument overrides the default. The record is
available on C, C++, Go, and portable builds. Compile-time `#run` execution of
procedures that consume this record also captures the call-site path and line,
including imported and nested calls; `#assert` and `#if` can use those results.
ASCII byte characters use `#char "A"` and infer `s64`, as integer literals
do; single-quoted character literals are rejected.
Jai raw multiline strings use `#string END` followed by their exact text and
a closing `END` at the start of a line. The opening line contains only the
delimiter, and the newline before the closing line belongs to the string.
Quotes, backslashes, tabs, and UTF-8 text in the body are preserved. The
current source line and string literal size limit still applies after lowering
to the checked IR.
Nested `/* ... */` comments and `//` line comments may appear between tokens;
comment delimiters inside quoted strings remain text. Legacy `# ` comments
are rejected. An unclosed block comment is a source error, including in a
loaded file.
Raw pointer null values use Jai `null`; the old `nil` source spelling is
rejected.
Pointer expressions use Jai `*value` to take an address and `pointer.*` or
prefix `<<pointer` to read or write through a pointer. C-style `&value` is
rejected; binary `<<` and `&` remain shift and bitwise operations. Jai has no
`++` or `--` increment/decrement operators; use
`+= 1` or `-= 1`.
Member access through a record pointer uses `pointer.field`, with an implicit
dereference. C-style `pointer->field` is rejected.
Pointer type annotations use prefix `*Type`. C-style suffix `Type*` and
`const` qualifiers are rejected in source and saved IR.
Raw pointers support `pointer[index]` on native targets. The caller must
establish that the pointer is non-null and the index is within the allocation;
unlike fixed arrays and slices, a raw pointer carries no length. Portable
`.zib` bundles continue to reject reachable raw pointers.
Runtime conditional expressions use Jai `ifx condition then a else b`.
The optional `then` spelling is also accepted. C-style `?:` is rejected.
Compile-time conditional expressions use `#ifx condition then a; else b`.
The semicolon between arms is optional. The frontend selects one arm before
checking or serializing expressions; its condition evaluator currently accepts
boolean and integer constants, constant aliases, arithmetic and logical
operations, and compiler-host `OS` comparisons with `.WINDOWS`, `.MACOS`,
or `.LINUX`. During translation, `#run`, `#ifx`, and `#assert` can call pure
same-module or imported procedures with signed integer, up to `u32`, or
boolean parameters and results. These procedures can use local variables,
assignments, branches, bounded `while` and numeric range loops, and nested
calls. `#run` also accepts pure string and floating-point expressions, including
imported calls, local assignments, and default parameters; `#assert` can
compare their results. String comparisons decode escapes using the same UTF-8
rules as the portable runner. `#run` results and selected `#ifx` arms are written into checked
`.zir`, so native targets and bundles receive the same result. File and system
access, aggregate results, and general metaprogramming are not available to
this evaluator. `#if` selects
source branches while parsing. It can call earlier same-module procedures or
procedures from imports declared before the condition. Earlier `#run` values
can participate when those procedures and imports are already available.
Imported type layouts can also be used in `size_of` conditions.
`#assert` must resolve in the Ziran frontend; native backends do not defer its
condition to the C preprocessor.
Statement and declaration branches use Jai `#if`, `else #if`, and `else`.
Conditions must be compile-time constants;
the frontend removes unselected branches before checking or writing IR.
Conditions in unreachable nested arms and arms after a selected branch are
not evaluated.
The older `#else_if` and `#else` spellings are rejected. Conditional fields
and enum members are also selected while parsing a declaration.
Exported native symbols use Jai's `#program_export` before a procedure,
either on its own line or inline. A quoted symbol such as
`#program_export "entry" Start :: () { ... }` selects the C/C++ linker name;
the Go target reports that symbol override as unsupported. The old `#export`
signature modifier is rejected.
File-scope variables use `name: Type;` or `name: Type = value;`. The old
`name :: Type #global` and C-style `static name: Type` forms are rejected.
Visibility for following declarations uses Jai's `#scope_file`,
`#scope_module`, and `#scope_export` directives. The old `#private` suffix is
rejected. `#load` adds files to one module. File-private declarations and
imports remain visible to their own file; module-scoped declarations remain
visible across loaded files. Private procedures, constants, globals, and types
with the same source name in separate loaded files receive distinct checked
identities.
Foreign procedures use `lib :: #system_library "lib";` and
`Call :: (...) -> Ret #foreign lib;`. An alternate symbol may follow the
library name in quotes. `#extern` is rejected in source. Ziran's `host_api`
library name still denotes a host capability in its backends.
Unqualified explicit file imports use `#import, file "relative/path.zi";`
and resolve relative to the importing source file. Saved IR uses the imported
module name. Named `Alias :: #import "module";` and
`Alias :: #import, file "relative/path.zi";` expose public procedures,
constants, and record types through `Alias.Name`.
`Alias :: #import, dir "relative/directory";` loads `module.zi` within the
directory. `#load "relative/file.zi";` adds that file's declarations to the
current module and may nest. `#import, string "...";` and a raw `#string`
body compile embedded source as a module. The named form
`Alias :: #import, string "...";` exposes declarations through `Alias.Name`.
`#import, file` loads `.zi` source only. C headers are outside the Ziran
module graph; foreign declarations use `#system_library` and `#foreign`.
An entry source file derives its module name from its filename. The old
`#module` directive is rejected. Jai `if value == { case ... }` branches now
accept enums, integers, booleans, floating values, and strings. Scalar cases
may end with `case;` as a default. `#complete` requires every enum member and
source `match` on enums is rejected. A case may end with `#through;` to run
the next case body. The final case cannot use `#through;`. Explicit enum member values use Jai
`Member :: value` declarations and lower to checked values in saved IR.
Each case is a lexical scope, so its deferred actions run before the next
case starts, including when the arm ends with `#through;`.
Named enums default to `s64` storage, and declarations may specify an integer
backing type. `enum_flags` assigns successive powers of two to implicit
members. `size_of` follows the backing width. Unsigned values above `s64`'s
maximum remain an implementation gap. `#specified` after the optional enum
backing type requires every member to have an explicit `::` value; it also
works with `enum_flags`.
Typed `enum_flags` values accept integer initializers, `Type.Member`, and
contextual `.Member` references. Flag values support `&`, `|`, `^`, `+`,
`-`, equality, and the corresponding compound assignments. Member references
are checked against their enum before lowering to their numeric values.
Regular enum values also accept `Type.Member` and contextual `.Member`
references in typed declarations, comparisons, returns, and calls.

## Evolution

The language can grow without embedding one library's concepts in its core.
While Ziran is experimental, source syntax, library APIs, `.zir`, and `.zib`
have no backward-compatibility promise. Prefer a coherent language design over
compatibility shims; version incompatible binary formats and reject old input
with clear diagnostics. Reproducible compiler output and conformance suites let
new backends and LLM-authored code be checked against the same semantics.
