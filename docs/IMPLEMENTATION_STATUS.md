# Implementation status

This page reports the local repository as it exists now. [Architecture](ARCHITECTURE.md),
[`.zir`](ZIR.md), and [`.zib`](ZIB.md) define the target state.

## Working now

- The Python backend (`zi2py`, `ziran build --target=py`) writes one Python
  3.10 file that uses only the standard library: `__main__.py` for
  `--exe --entry`, otherwise `__init__.py`. Every program the test suite
  runs on the portable VM prints the same output and returns the same
  result as Python, and every program it builds for C also lowers to Python
  that compiles. Integer arithmetic wraps at each type's width, division
  truncates toward zero, and shifts check their count. Records and fixed
  arrays copy by value; pointers are objects that reach a list item, a
  field, or a boxed local; unions share a byte buffer; C foreign functions
  go through `ctypes`, with `string` parameters and results passed as C
  text and the shared libraries named in `LDLIBS` loaded first, and host
  capabilities through a `host` object the embedding program sets. A view of mutable bytes is a snapshot.
  `tests/py_backend.sh` covers it, and `tests/numeric_semantics.sh`,
  `tests/print.sh`, `tests/examples.sh`, and `tests/named_imports.sh` compare
  it with the other targets.

- The Rust backend builds every program the test suite builds for C, and
  every program the suite runs on the portable VM prints the same output
  and returns the same result as a Rust executable. It lowers unions,
  pointers, slices, owned Vec storage, record and procedure templates,
  host and variadic C foreign functions, labeled loops, and checked
  shifts; `tests/numeric_semantics.sh` covers it in the numeric
  differential matrix. Generated Rust keeps only the runtime a program
  uses and wraps a body in `unsafe` only when it touches raw pointers or
  globals.
- `ziran features` publishes a versioned, stable-ID feature registry with
  syntax, status, target support, limits, accepted and rejected examples,
  rejection behavior, and evidence links. The initial registry covers
  compile-time assertions, checked scalar `print`, named module imports,
  integer width conformance, local `TextView` mutation checks, and plain
  record-field `Vec` moves.
  `docs/FEATURES.json`
  is byte-identical to `ziran features --json`; `tests/features.sh` checks
  schema, sorted unique IDs, evidence, target entries, every accepted example,
  and every explicit rejection.
- Source and saved-IR portable runs use the declared width for integer
  conversions between all eight `s8`, `u8`, `s16`, `u16`, `s32`, `u32`,
  `s64`, and `u64` forms. Signed overflow and unsigned wraparound follow the
  operand width, the minimum signed integer divided by `-1` wraps to itself,
  and its remainder is zero. Shifting by a negative count or by the operand's
  bit width or more fails at execution on every target. Runtime `float32` and
  `float64` division can produce infinity or NaN on native and portable
  targets; NaN comparisons and infinities retain floating semantics, while
  converting a nonfinite float to an integer remains an error. Compile-time
  casts and arithmetic fold the tested boundaries with the same width rules.
  `tests/numeric_semantics.sh` checks these cases from source and saved IR on
  C, C++, Go, and `.zib`. Its stable conformance IDs are published by
  `ziran capabilities --json`, described by
  `tests/numeric_conformance.json`, and kept synchronized with markers in the
  differential test. Mixed-width narrowing/widening after addition and u8
  bitwise AND/OR/XOR are included.
- `std/format` builds text on a `Vec(u8)`: overloaded `Append(*builder,
  value)` for strings, integers, bools, and floats, and
  `BuilderPrint(*builder, "format", args...)`, which expands to those calls
  and imports `std/format` for its file. `tests/format_builder.sh` checks
  source and saved IR on C, C++, Go, and `.zib`.
- Statements may be longer than 4 KB, such as a table of records written as
  one array literal: source text is kept whole through parsing, checking,
  and saved IR (up to 1 MB per text), and C and C++ store an array literal
  too long for one initializer element by element. Before, such a
  statement failed with "expression is not supported by language checking"
  or silently. `tests/long_literals.sh` checks source and saved IR on C,
  C++, Go, and `.zib`.
- `ziran check --lint` warns (code `lint.cast`) about casts to a value's own
  type and casts that only widen a whole typed initializer, assignment,
  argument, or result; it leaves casts that set arithmetic width, choose an
  inferred type, or pick an overload. Inbe's sources have 51 such casts.
  `tests/lint_casts.sh` pins the rules.
- A generic record from another module may hold types only that module
  imports: the user's `Bag(string)` reads the library's `Vec(K)` field as
  `vec.Vec(string)`, and every copy of one application gets the same name.
  Records applied through a named import, `Pairs.Pair(s32, string)`, work
  too. `tests/generic_record_imports.sh` checks source and saved IR on C,
  C++, Go, and `.zib`.
- `std/file` imports the file operations for the native C target: file_linux
  on Linux, Android, the web, and Windows, file_plan9 on Plan 9, where
  `--target=plan9-c` now defines `PLAN9`. It adds `ReadEntireFile` and
  `WriteEntireFile`. `tests/file_portable.sh` runs it on C from source and
  saved IR and checks the Plan 9 build selects file_plan9.
- `New(T)` allocates a zeroed `T` and returns `*T`; `free(p)` releases it and
  ignores null. C and C++ use `calloc`/`free`, Go uses `new` and its
  collector, and the portable runner keeps the storage alive while a pointer
  reaches it and stops cleanly on a read after free or a second free. A
  procedure or foreign procedure the program declares as `New` or `free`
  (such as C's `free`) is called instead. Rust and Python report the
  expression as unsupported. `tests/heap_new.sh` checks source and saved IR
  on C, C++, Go, and `.zib`.
- A procedure may declare procedures inside it. The parser hoists each to
  file scope under a private name (`zi_local_Outer_Name`, kept out of API
  listings) and renames its uses in the rest of the enclosing procedure; as
  in Jai, it may recurse and nest but cannot use the enclosing procedure's
  locals, which the unresolved-name error says. `tests/local_procedures.sh`
  checks source and saved IR on C, C++, Go, and `.zib`.
- Records take Jai operator procedures: `operator + :: (a: V, b: V) -> V`
  for the binary operators `+ - * / % == != < <= > >= & | ^ << >>`. Several
  declarations of one operator are overloads; `a += b` uses operator +,
  `a != b` falls back to operator ==, and `#symmetric` also accepts the two
  arguments swapped. Operators reach callers through open and named imports,
  and a record operation without one reports that an operator procedure is
  needed. `tests/operators.sh` checks source and saved IR on C, C++, Go,
  and `.zib`.
- `print` shows a record as `{x = 1, y = 2}`: nested records in braces,
  strings quoted, enums by name, fixed arrays by element (up to 16), and
  Vecs and slices by count. The call becomes a call of a generated
  procedure taking the same arguments, so each is evaluated once; a value
  holding a Vec is passed by address instead of moved. `tests/print_records.sh`
  checks source and saved IR on C, C++, Go, and `.zib`.
- A local declaration may use `type_of(expression)` as its type, and a for
  range counts in its bounds' type (`0..count` with `count: s32` is `s32`;
  constant ranges stay `s64`). Indexing with `value[a..b]` reports that
  slices are written `value[start:end]`. `tests/range_types.sh` checks
  source and saved IR on C, C++, Go, and `.zib`.
- `std/hash_map` provides `HashMap(K, V)` with `HashMapSet`, `HashMapGet`,
  `HashMapHas`, `HashMapRemove`, and `HashMapFree` over string and integer
  keys, written in portable Ziran. Polymorphic procedures bind type
  parameters from generic record parameters such as `*HashMap($K, $V)`;
  record instances a specialization names after the program pass get their
  fields then. A bundle keeps a record field every module's copy of one
  type application uses, and Go compares copies of `Vec(string)` by their
  slice type. `tests/hash_map.sh` checks source and saved IR on C, C++, Go,
  and `.zib`.
- `print` shows an enum value by its member name, and a value outside the
  enum as `(invalid Enum)`. The checker routes the argument through a
  generated name procedure in the enum's module, created only for enums a
  program prints, so every target and `.zib` agree. Flag enums still need
  a cast. `tests/print_enums.sh` checks source and saved IR on C, C++, Go,
  and `.zib`.
- Polymorphic procedures may bind several type parameters, each with its
  own `$Name`, as in `Pick :: (a: $A, b: $B) -> A`. Every call infers each
  parameter and gets its own specialization, placed with the caller when one
  of its types is only visible there. A name bound twice is an error, and
  polymorphic procedures cannot yet have several results.
  `tests/generic_parameters.sh` checks source and saved IR on C, C++, Go,
  and `.zib`.
- Procedures may be overloaded: several declarations may share a name when
  their parameters differ. A call chooses the overload whose parameters take
  its arguments best (exact types, then an untyped literal's usual type,
  then the closest widening) and gets that overload's defaults; no match or
  an equal match is an error. Overloads work through open and named imports.
  The choice is made in checking, so saved IR and every target call the
  chosen procedure directly. Exported and polymorphic procedures cannot be
  overloaded. `tests/overloads.sh` checks source and saved IR on C, C++, Go,
  and `.zib`.
- Jai resizable arrays: `[..]T` is `std/vec`'s `Vec(T)`, imported for the
  file automatically, and `array_add(*a, x)`, `array_reset(*a)`,
  `array_free(a)`, and `array_reset_keeping_memory(*a)` are the matching Vec
  operations. Indexing, `.count`, and `for` work as on Vec. A local `[..]T`
  passed where `[]T` is expected lends a view (`VecSlice`) instead of moving.
  Vecs of fixed arrays and nested Vecs are rejected; wrap the element in a
  record. `tests/resizable_arrays.sh` checks source and saved IR on C, C++,
  Go, and `.zib`.
- Procedures return several results as in Jai: `-> s32, s32` or named
  `-> (low: s32, high: s32)`, `return a, b`, `q, r := F()`, `q, r = F()`, and
  `_` to skip one. A call used as one value gives its first result. The results
  travel in a generated record shared by procedures with the same result types
  (`Results__s32__s32`, fields `value_0`...), so every target and `.zib` run
  them unchanged. Returning another module's results directly needs binding
  them first; polymorphic procedures cannot yet have several results.
  `tests/multiple_results.sh` checks source and saved IR on C, C++, Go, and
  `.zib`.
- Lossless numeric conversions are implicit: a narrower integer into a wider
  one of the same signedness, an unsigned integer into a wider signed type,
  and `float32` into `float64`, at initializers, assignments, returns,
  arguments, and binary operators. The checker inserts an explicit cast into
  the checked graph, so C, C++, Go, Rust, Python, and `.zib` see the same
  conversion. Narrowing, sign changes, and integer-to-float conversions still
  need `cast`. `tests/implicit_widening.sh` checks source and saved IR on
  C, C++, Go, and `.zib`.
- Jai `using record: Type` parameters and local declarations, plus `using
  record;` and nested paths such as `using entity.position;` in a procedure
  body, promote record fields into lexical lookup. Explicit local bindings
  shadow promoted fields; conflicting promoted
  names are diagnosed. Struct fields declared `using field: Record` promote
  contained fields through nested records and concrete generic applications.
  Reads and writes retain their nested storage layout in checked IR; source
  and saved IR agree in C, C++, Go, and `.zib`. Pointer-backed `using` fields
  work in native targets.
  Polymorphic procedures preserve parameter, local, and imperative `using`
  declarations in saved templates and lower them for each concrete call.
  File-scope `using value;`, nested paths, and `using value: Record;` promote
  fields of module-local record and scalar-union globals in procedure bodies.
  Forward global declarations, filters, local shadowing, ambiguity diagnostics,
  and file-private `#load` boundaries are tested from source and saved IR in
  C, C++, Go, and `.zib` by `tests/data_scope_using.sh`.
  Public globals from open and named imports can also be read, assigned, and
  opened with record `using` in procedure bodies. `tests/imported_globals.sh`
  checks source and saved IR across C, C++, Go, and `.zib`, along with
  ambiguous imports and private global visibility. Named and open imported
  globals keep their declaring record type when a consumer defines a
  same-named type; direct and `using` field access agree across all four
  targets. Checked IR retains open-import type ownership with a private alias.
  `using,only(...)`/`except(...)`/`map(...)` modifiers filter and rename
  promoted record fields and opened enum members on local and data-scope
  using declarations, in source and saved IR across C, C++, Go, and `.zib`;
  a hidden or unmapped name stays unresolved. File-scope enum filters also
  apply in constants, global initializers, `#run`, `#assert`, and `#if`;
  conflicting maps of one exposed name are diagnosed even when they open
  members from the same enum. `using Alias :: #import
  "Module";` re-exports the module's public procedures, types, and constants
  into unqualified scope beside the alias: local declarations shadow the
  re-export, and two using-imports exposing one name are ambiguous.
  `Name :: #as (source: Type) -> Result;` declares a checked conversion that
  the checker applies implicitly at initializers, assignments, returns, and
  call arguments when the types are otherwise incompatible: the expression
  is rewritten into an ordinary call, the graph is re-laid out for saved IR,
  and two applicable conversions are ambiguous. Literals that already widen
  natively never trigger a conversion.
- Jai-style `#program_export` on its own line or inline exports a procedure's
  native symbol. A quoted linker name works in C/C++; Go reports that override
  as unsupported. File-scope variables use `name: Type` declarations.
  `#export`, `#global`, and C-style `static` source forms are rejected. Source
  and saved IR retain these exports and globals across native and portable
  builds.
- File-scope variable initializers and constants pass through the Jai
  expression parser and shared type checker during checking, including nested
  record fields and fixed-array elements. A mismatched string, boolean, or
  nested initializer now fails before native output or portable bundling;
  `tests/portable_globals.sh` exercises these diagnostics. C-style compound
  literals and ternary expressions are rejected there before native output,
  including from saved IR. The C++ and Go backends no longer carry C-style
  compound-literal rewrites; Go also no longer translates C casts, `NULL`, or
  C scalar type aliases in file-scope expressions. The source reader no longer
  joins C-style adjacent string literals or multi-line `?:` fragments.
- Portable file-scope array initializers use `.[...]` literals with the
  declared array type for context: scalar elements and record elements
  (`Type.{.field = value}`) fold in the VM, serialize through `.zib`, and
  lower to brace initializers in C99 (designated, with string fields expanded
  to `{data, length}` pairs), C++ (positional with the same string pairs),
  and Go composite literals (native strings).
- Jai `#must` after a procedure result type requires callers to use the
  result. The checker enforces it for ordinary, imported, polymorphic, and
  foreign procedures in source and saved IR.
- Jai `#compile_time` evaluates to true in supported compile-time procedure
  execution and false in C, C++, Go, and portable runtime code. It is a
  boolean expression, not a declaration constant or direct `#if` condition.
- Jai `#caller_location` in a parameter default supplies the calling file's
  full path and physical line as a `Source_Code_Location` record. Inferred and
  explicitly typed defaults work through imports, polymorphic calls, and
  `#load`; explicit arguments override the default. Source and saved IR agree
  in C, C++, Go, and portable builds. Compile-time `#run`, `#assert`, and
  local or imported `#if` calls capture the same caller path and line,
  including nested calls and explicit overrides; `tests/caller_location.sh`
  covers source and saved IR on all four targets.
- Jai `#string DELIMITER` raw multiline literals preserve their body bytes,
  including whitespace and the newline before the closing delimiter. Source
  and saved IR execute identically on the portable runner, C, C++, and Go.
  `zi-fmt` preserves raw bodies and closing delimiter columns. The existing
  source line and string literal size limit applies after the literal is
  lowered.
- Nested block comments and inline `//` comments are stripped without changing
  quoted strings or source line locations. Unclosed comments are diagnosed in
  entry and loaded files; source and saved IR agree across portable, C, C++,
  and Go execution.
- Legacy `# ` comments are rejected at file, type, and function scope. Use
  `//` or nested `/* ... */` comments.
- Compile-time `#if` branches and `#assert` are resolved by the frontend.
  Declaration guards and target-specific assertion fallbacks have been removed;
  a selected failing assertion stops source or saved-IR checking before C,
  C++, Go, or portable output.
- Semicolons separate multiple declarations or statements on one source line,
  including one-line record bodies and function bodies. The parser retains
  `#ifx` arm separators and still rejects C-style `for` headers. Source and
  saved IR execute alike in the portable runner, C, C++, and Go.
- `if` and `else if` accept Jai's optional `then` before a block or one
  statement. Inline `else` statements also work, including after a closing
  brace. Multiple braced arms, nested arms, and empty blocks may share a
  source line; source and saved IR agree in portable, C, C++, and Go execution.
- Source foreign declarations use Jai's `#system_library` and `#foreign`
  forms. The compiler resolves those library declarations to its existing
  host, C, or Go import model before saving IR. A trailing `..any` parameter
  declares a C variadic foreign procedure: calls must provide its fixed
  arguments and may provide additional arguments. Source and saved IR lower
  tested variadic calls to C and C++; Go rejects the declaration. The old
  `#extern` source modifier is rejected.
- Source accepts Jai `#scope_file`, `#scope_module`, and `#scope_export` for
  following procedures, globals, types, and constants. Private declarations
  are excluded from imported lookup, native headers hide private types and
  constants, and private globals use internal linkage. Scope visibility
  survives saved IR. `#private` is rejected. Loaded files join their caller's
  module; file-private declarations, imports, and system libraries are visible
  only to their own source file, including compile-time conditions and size
  queries. Separate loaded files may reuse private procedure, constant, global,
  and type names; checked IR assigns distinct identities before native or
  portable output. Constant aliases, native global initializers, record
  construction, and local shadowing retain the correct file-local binding in
  source and saved IR.

- The compiler frontend and C/C++/Go backends have been extracted into a separate
  Ziran repository. `zi2zir` checks and saves `.zir`; `zi2c`, `zi2cpp`, and
  `zi2go` compile `.zi` or saved `.zir` input.
- `make check` passes standalone non-UI programs, a two-module import, and an
  imported typed record call named `Button` through generated C, C++, and
  native Go. A local declaration binds its non-void result, with source and
  saved `.zir` builds across those targets and matching source/saved `.zib`
  execution. The call is resolved from its declaration, not its name.
- Jai procedure type aliases such as `Child :: #type (s32) -> ();` and
  `Compute :: #type (s32) -> s32;` can be passed as callbacks to imported
  functions. Source and saved `.zir` builds execute named callbacks with void,
  scalar, and record results in C, C++, Go, and `.zib`. Capture-free values
  can be stored in records, fixed arrays, and globals, and returned from
  functions. Entry builds retain their named targets and remove unread record
  fields from closed-program layouts. The former `#slot` declaration and
  capturing body syntax are rejected.
- Context-inferred `.{...}` record literals work in declarations, assignments,
  returns, record fields, and arguments to imported functions. Source and saved
  IR produce the same result in C, C++, Go, and `.zib`; missing type context
  and unknown fields are rejected.
- Jai `defer { ... }` blocks run their statements in order on scope exit,
  including early returns and loop exits. Source and saved IR agree in C,
  C++, Go, and `.zib`. Control transfer or a nested `defer` inside a deferred
  block still needs lowering support.
- Jai inclusive integer ranges accept named binders, implicit `it`,
  `it_index`, and `for <` reverse iteration. Bounds run once, and `break`,
  `continue`, nested ranges, and `defer` work across source and saved IR,
  C, C++, Go, and `.zib`. Fixed arrays and borrowed slices also support
  value and index bindings, reverse iteration, and nested loop control across
  those targets. C, C++, Go, and `.zib` support `for *` pointer iteration for
  in-place element changes. Collection
  expressions run once and value bindings copy elements. C-style three-clause
  headers are rejected. Jai `for_expansion` iterables remain unsupported.
- Fixed arrays, borrowed slices, and strings now expose read-only Jai
  `.count` values of type `int`, including in range bounds. The inherited
  `.length` source spelling is rejected. Single-statement `if`, `while`, and
  `for` bodies now normalize to checked blocks. Named `break` and `continue`
  target enclosing `for` binders or named `while` condition binders; defer
  cleanup and range advancement apply when those controls cross nested loops.
- `check`, `ir`, `build`, and `bundle` discover extensionless imports
  transitively. Repeated `--module-path DIR` options locate ordinary libraries
  outside an app root; explicitly supplied modules take precedence. A separate
  app root and two-module library are tested from source and saved `.zir`,
  including generated C and `.zib`. Kryon's drag policy test loads its `src/ui/`
  library from an app entry file through the same path, across C, C++, Go,
  and portable bundles.
- Jai `#import, file "relative/path.zi";` resolves a source file relative to
  its importer. Source and saved IR link the same module, and C, C++, Go, and
  portable builds agree. Named `Alias :: #import "module";` and
  `Alias :: #import, file "relative/path.zi";` expose public procedures,
  constants, and record types through `Alias.Name`; named imports do not leak
  unqualified names. `Alias :: #import, dir "relative/directory";` loads the
  directory's `module.zi`; ordinary module lookup also finds
  `name/module.zi`. Source and saved IR produce the same C, C++, Go, and
  portable output for this form. Nested `#load "relative/file.zi";` adds
  declarations to the current module, keeps source file diagnostics, and
  resolves imports relative to each loaded file. Cycles and missing files are
  rejected; inactive compile-time branches skip loads. Unconditional nested
  loads are discovered before compile-time branch
  selection, so earlier conditions can use later public constants and type
  layouts from those files. File-private names remain local to their loaded
  file; `tests/forward_load_conditions.sh` checks source and saved IR on C,
  C++, Go, and `.zib`. `#import, string`
  compiles quoted or raw `#string` source as an embedded module, with named
  and unqualified imports. Compile-time branches inside embedded source can
  call modules imported earlier in that source. Source and saved IR bundles
  agree, and C, C++, and Go execute the generated modules.
- Ordinary `#import` now accepts only module identifiers. C header paths and
  C-style angled imports are rejected, including from saved IR; unknown calls,
  fields, and stored types no longer pass checking merely because a C header
  was imported. Downstream C-backed modules that used header imports require
  source migration.
- Module inputs accept `.zi` source and `.zir` IR only; `.kry`, `.kir`, and
  `.krb` are rejected by the shared loader. Kryon owns the UI test fixtures;
  Ziran keeps a non-UI array and UTF-8 byte law fixture that runs from source
  and saved IR through native C, C++, and Go.
- `zi2zir` writes experimental binary `.zir` version 41 after checking all
  input modules together. C, C++, and Go can read saved modules without reparsing
  `.zi`; their imports are relinked from serialized module identities. The
  reader rejects malformed headers, versions, truncated data, invalid
  structural references, expression cycles, retired
  statement kinds. Retained-state fields are absent from the IR schema. C, C++, Go, and bundle builds
  rerun strict language checking on saved expression graphs and compare the
  resulting serialization byte for byte with the input. The tested typed
  subset takes behavior from those graphs and serialized branch flags even
  when stored statement text differs. Deterministic output is checked on a
  sample, and a mixed source/IR
  C build runs.
- Jai `union` declarations, including generic unions, retain their shared
  field storage and maximum-field layout in checked `.zir`. C and C++ compile
  source and saved IR unions with overlapping fields. Unions whose fields are
  all scalars or enums also build and run on Go, through an aligned byte
  backing with `unsafe` typed access, and in portable bundles, where the VM
  keeps raw bits in one slot and reinterprets every member read, write, and
  compound assignment through the declared field type. Go union storage now
  uses the largest scalar field's alignment; `tests/union.sh` checks the exact
  1-, 2-, 4-, and 8-byte sizes and alignments on C, C++, and Go, plus
  independent union value copies across all four targets. Unions with string,
  record, array, or pointer fields remain C/C++-only.
- `zi2zib bundle --root DIR --entry module:function -o FILE` builds an
  experimental version 24 `.zib` from source or saved IR. `zi2zib run FILE`
  loads and executes the validated scalar, plain record, enum, and fixed-array
  subset without a
  display or Kryon.
  The test runs an imported two-module call and compares bundle bytes from
  source and saved IR. The current runner supports zero-argument entry
  functions with `s32`/`int`/`bool`/`void` results, and helper functions with
  `s64`/`u8`/`u32`/`u64`/`float32`/`float64`/`string`, enum, or plain record results. It supports scalar, enum,
  string, record, and fixed-array parameters and locals, nested record fields,
  defaults and record and array literals, value copies, member and array reads
  and writes, scalar compound assignments,
  `s64`, `u32`, and `u64` bitwise operations and shifts, calls,
  arithmetic, comparisons, casts, assignments, `if`/`else`, `while`, lexical
  blocks, `break`, `continue`, `unreachable`, Jai-style enum `if #complete`
  cases, and returns. Complete enum cases lower to checked branches with a trap for values
  created by an out-of-range integer cast. It rejects host imports outside the
  scalar/string/plain-record subset and
  unsupported statements before writing a bundle. Loop, branch, and real
  arithmetic programs are compared against generated C, C++, and Go in
  `make check`. The bundle loader also reruns the language checker and laws
  before execution. The linker follows direct calls and drops unreachable
  functions and modules, while retaining reachable record and enum declarations.
  Entry linking now removes effect-free constant branches, short-circuited
  expressions, false loops, and statements after unconditional exits before
  tracing calls or stored procedure values.
  Fixed arrays with numeric or resolved constant-expression bounds, including
  imported constants and nested arrays in records, are tested through source
  and saved IR in C, C++, Go, and `.zib`.
  Tests cover ASCII values in nested `char` arrays, value-copy isolation, and
  portable bounds failures. Linked bundles retain needed
  compile-time definitions for bound expressions, including constants-only
  imports.
  Integer `NAME :: value` definitions in the checked arithmetic subset can be
  used in function expressions. Imported and negative values fold to checked
  IR literals. String literal definitions and aliases also fold into checked
  expressions, including in saved IR and portable bundles. The checker rejects
  local bindings that would collide with
  generated C/C++ definitions.
  Portable slices borrow fixed-array storage. Source and saved IR bundles and
  generated C, C++, and Go agree on range creation, nested views, function
  parameters and returns, `.count`, indexed reads and writes, and array
  replacement while a view is live. Bounds failures reject execution. The
  checker rejects slices in records and globals, host slice returns, and
  returns that borrow a local array. Unresolved fixed-array bounds remain
  outside the portable subset.
  Native C, C++, and Go targets also permit a temporary slice of a fixed
  array field reached through a direct record-pointer parameter. The caller
  owns that record for the call; the checker rejects returning the slice.
  Portable bundles still reject pointer programs.
  Borrowed arrays retain record elements written by called functions after
  those functions return; the portable slice test covers replacing a nested
  record in a live view across calls.
  The test runs record and enum functions across imported modules.
  Source `#enum`, `variant`, payload `match`, postfix `?`, `guard`, C-style `switch` and
  `goto` with labels, `state` blocks, C-style locals, and raw C statements are
  rejected. Native C, C++, and Go builds check source unconditionally and reject
  `--strict` and `--no-strict`. The standard `Option` and `Result` templates
  are now generic records with explicit status fields. The checked native
  emitters escape target keywords used as parameter or local names; source and
  saved IR tests run `switch` and `goto` bindings, including a generated-name
  collision, across C, C++, and Go. Go globals keep source spelling when
  legal; native emitters escape target keyword globals, including collisions
  with the generated escape name. Same-named globals in linked modules, and
  globals whose target keyword escapes collide across modules, receive
  distinct native names. `tests/imported_global_names.sh` checks reads,
  writes, source, and saved IR on C, C++, Go, and `.zib`. A Go global and type
  whose names differ only by case or underscores remain distinct. Constants
  use the same keyword and collision handling without defining C keywords as
  macros; source and saved IR tests include dependent constants and fixed-array
  bounds. Constants shared by linked modules, or whose target keyword escapes
  collide across modules, receive distinct C macro and Go constant names;
  `tests/imported_constant_names.sh` checks source and saved IR on all four
  targets. Linked globals and constants with the same native name also remain
  distinct; `tests/imported_value_names.sh` checks source and saved IR on all
  four targets. Globals and constants whose native names collide with linked
  functions, including C/C++ program exports, receive distinct names;
  `tests/native_function_value_names.sh` checks source and saved IR on all four
  targets. Generic record templates use named
  `Name :: Generic(Type)` declarations or direct
  `Generic(Type)` annotations; `std/pair.zi` also provides `Pair`. Concrete
  record fields work in source, saved IR, C, C++, Go, and `.zib`. Top-level boolean
  `&&` and `||` expressions in simple statements and loop conditions retain
  short-circuit evaluation, including nested forms. Jai `ifx` conditional
  expressions, with or without `then`, replace C-style `?:`; top-level
  conditional expressions with
  typed destinations or loop conditions preserve selected-arm evaluation.
  Conditional expressions inside positional and named call arguments run
  across the native targets and portable bundles. Entry linking preserves
  argument positions when it folds a constant
  conditional or short-circuit expression. `tests/nested_lazy_call.sh`
  compares source and saved `.zir` across C, C++, Go, and `.zib` while
  checking that unselected arms have no effects.
  Jai `#ifx` selects a constant, global initializer, or function expression
  arm before strict checking and saved-IR generation. It accepts the Jai
  semicolon before `else`, nested selections, boolean and integer constants,
  constant aliases, and compiler-host `OS` comparisons with `.WINDOWS`,
  `.MACOS`, or `.LINUX`; unused arms are not type-checked.
  `#ifx` also selects record and fixed-array values in file constants, global
  initializers, local definitions, and call arguments on C, C++, Go, and `.zib` from source
  and saved IR. `#run`, `#if`, `#ifx`, and `#assert` evaluate pure procedures
  with signed integer (up to
  `s64`), unsigned integer (up to `u32`), or boolean parameters/results. Local
  declarations and assignments, branches, bounded `while` and numeric range
  loops, nested calls, and imported calls work in `#run`, `#if`, `#ifx`, and
  `#assert`. `#if` can resolve imports declared before its condition while
  parsing, including imported `size_of` layouts. Other imported calls and
  dependent definitions resolve after module linking; their literal values
  and selected arms are saved in `.zir`. Pure string and floating-point
  expressions, including imported calls, local assignments, and default
  parameters, work in `#run`; `#assert` can compare those results. String
  comparisons use the same escape and UTF-8 decoder as the portable runner.
  Source-selecting `#if` can use earlier `#run` definitions when their
  procedures and imports are available while parsing. Named imported real
  and string constants resolve in `#if` and `#run` even when the import
  declaration follows the condition. Their public visibility and private
  exclusion are checked from source and saved IR on C, C++, Go, and `.zib` by
  `tests/imported_typed_constants.sh`. Pure compile-time procedures can now
  return named records and fixed arrays. Their evaluated fields, array
  elements, and array counts can feed `#run` scalars and `#assert`. Local
  record and fixed-array `#run` definitions also bind to checked literal
  graphs in procedure bodies, and direct file-scope initializers can use
  them. `tests/compile_values.sh` checks nested records, fixed arrays,
  bounds rejection, runtime record arguments and globals, and source/saved-IR
  execution on C, C++, Go, and `.zib`. Public aggregate definitions from
  named, open, and using imports retain their declaring record types in
  runtime expressions and direct globals; `tests/imported_aggregate_constants.sh`
  checks nested records, arrays, compile-time `#run`, `#assert`, and `#if`
  field selection, a later named import in `#if`, private visibility, and source/saved IR
  across the same targets. Imported procedures with record parameters or
  results map those types into the caller's namespace; the same test covers
  direct calls, including parameter and result types shadowed by local records.
  Qualified record literals
  such as `Lib.Point.{...}` work in globals and bodies, including nested
  inferred record fields; `tests/imported_aggregate_constants.sh` checks
  source and saved IR on all four targets. Conflicting consumer record names
  retain the imported owner through named, open, and using imports; nested
  record constants, arrays, calls, and parameter passing agree on all targets.
  Pure file-scope initializers now fold imported record-field and fixed-array
  selections, scalar expressions built from them, and tested `#ifx` selections
  before native or portable output. Bounded pure procedure calls also fold in
  globals, including a call declared after the global; effectful calls are
  rejected by the portable bundle. Named, open, and using imports retain the
  aggregate type owner. General file-scope initialization remains open;
  effects, unbounded execution, and general metaprogramming are unsupported.
  Integer conditions and typed expressions use the same bounded procedure
  evaluator, including a shared instruction budget across nested calls; the
  separate integer-only procedure interpreter has been removed.
  One-line function bodies are parsed as ordinary statements instead of
  being silently skipped.
  Jai `#if ... else #if ... else` selects file-scope declarations and function
  statements at parse time. Unselected branches do not enter source or saved
  IR; source and saved bundles, C, C++, and Go agree for nested constant
  branches. The old `#else_if` and `#else` forms are rejected. Conditional
  record fields and enum members are selected before their layouts are checked.
  Explicit enum members accept Jai `Member :: value` declarations. Source and
  saved IR produce the same values in C, C++, Go, and portable bundles.
  Named enums now use Jai's default `s64` storage or an explicit integer
  backing type. `enum_flags` assigns successive bits to members without an
  explicit value. The checker rejects members outside their backing range,
  and `size_of` uses the backing width. Source and saved IR agree across the
  portable runner and native targets. Jai `#specified` requires explicit
  member values for both `enum` and `enum_flags`, and saved IR records the
  modifier. Values above `s64` remain unsupported
  in the current portable enum evaluator, even for `u64` backing. Typed flags
  accept integer initializers and assignments, qualified `Type.Member`, and
  contextual `.Member` references. The checker validates flag arithmetic,
  bitwise operations, equality, and corresponding compound assignments.
  Portable execution and C, C++, and Go output agree on these operations.
  Regular enum members also have typed `Type.Member` and contextual `.Member`
  expressions. Within a procedure, `using Enum;` opens enum members for bare
  lookup with lexical scope. Data-scope `using Enum;` also opens them in
  procedures, file-scope constants and globals, `#run`, `#assert`, and later `#if`
  conditions. File-private using declarations respect `#load` boundaries.
  Bare members without `using` are rejected in procedure bodies and file-scope
  global initializers; checked IR lowers opened members to values before native
  or portable execution. File-scope record and union `using` fields also work
  in pure global initializers that read earlier initialized globals. The
  checker folds those startup values for C, C++, Go, and `.zib`, respects
  local shadowing, and rejects ambiguous or file-private names. General global
  initialization and promotion from imported globals still need coverage.
  Call arguments and returns accept typed and contextual members. Enum and scalar
  `if`/`case` arms support terminal `#through;` with source, saved IR,
  portable, C, C++, and Go agreement. Integer, boolean, floating, and string
  cases accept a final `case;` default. Deferred actions run at case boundaries
  and before `#through;`. Unknown function-body directives are rejected.
  `size_of` supports scalars, pointers, `string`, borrowed slices, fixed
  arrays, records, unions, named concrete generic types, and nested
  applications such as `Wrapper(Box(s32))`. It folds in constants, array
  bounds, globals, `#run`, `#if`, and `#ifx`. Function-body expressions stay
  symbolic through source checking and saved IR, execute in portable bundles,
  and lower to target-native size operations in C, C++, and Go; tests cover
  the target-dependent slice layout. The old `sizeof` spelling is rejected in
  source. Jai `int`
  maps to `s64`. In function bodies, `size_of(type_of(expression))` uses the
  checker's inferred type and does not evaluate its operand. File-scope
  `size_of(type_of(expression))` works for previously declared same-module
  globals, record fields, and procedure calls in constants, global
  initializers, `#run`, `#assert`, `#if`, and `#ifx`; the operand is not run.
  Forward and imported operands also work in tested file-scope constants,
  global initializers, `#run`, and `#ifx`; imported operands work in `#if`.
  Unconditional procedure headers and single-line typed globals in the same
  file or a later `#load` resolve forward `#if size_of(type_of(...))` queries
  without evaluating the operand. Procedure parameter lists may span lines;
  global member queries can use a later record type. Inactive and
  file-private names stay hidden.
  Unconditional later `enum_flags` types also resolve in `#if` and `#run`,
  including implicit flags, explicit values, `#specified` backing types, and
  a later `#load`. Inactive and file-private flags stay hidden;
  `tests/forward_type_conditions.sh` checks source and saved IR on C, C++, Go,
  and `.zib`. The same test covers a later generic record and a one-line
  concrete alias without a semicolon in `#if size_of` and `#run size_of`;
  aliases inside inactive branches or private loaded files stay hidden.
  Later records, enums, and generic records with `#if`-selected bodies also
  resolve for supported forward size queries and opened enum members, including
  a type from a later `#load`. The selected branch alone reaches checked IR;
  private loaded types stay hidden. Acyclic forward dependencies between
  conditional records or enums resolve after their prerequisites are found;
  unresolved conditions fail without selecting a branch.
  Forward `#if` calls to unconditional procedures with local declarations,
  assignments, braced `if`/`else if`/`else`, and bounded `while` loops run
  through the compile-time evaluator. `break`, `continue`, calls to another
  later procedure, and returns work in the tested subset; `#run` accepts these
  calls too. Multiline headers, defaults, named
  arguments, later constants, and later `#load` files work. A procedure in a
  loaded file can use that file's private constant; executed effects are
  rejected, and recursion and loops share an instruction budget. Other body
  forms still need discovery.
  A file-scope `#run` that calls such a later procedure now folds before a
  following `#if` tests the result; loaded file-private procedures remain
  hidden from callers in other files.
  `tests/file_type_of.sh` checks source and saved IR on C, C++, Go, and `.zib`.
  Standalone `type_of` values and broader forward `#if` queries remain
  unsupported.
  Foreign-record size queries still require their Jai layouts.
  Jai `#line` resolves to the physical source line before checking and remains
  stable through saved IR, native targets, and `.zib`, including `#load` files.
  `#file` and `#filepath` resolve to the full source filename and containing
  directory, including in loaded files. `#procedure_name()` resolves to the
  declared procedure name in bodies, defaults, and pure `#run` calls; source
  use outside a procedure is rejected.
  Untyped integer locals inferred with `:=` now use `s64` storage, including
  when their values exceed 32 bits, across saved IR and all supported targets.
  Source, saved IR, native targets, and `.zib` agree on tested success and error
  paths. Kryon's headless suite exercises these paths through ordinary imports;
  widget and platform coverage is tracked in the Kryon repository.
- Reachable `#foreign host_api` calls now appear as module/function
  capabilities in `.zib`. The loader verifies the capability list against the
  linked IR. `build/libziran.a` and `include/ziran_host.h` expose an opaque
  bundle handle, capability enumeration, and bindings for integer, real, string,
  enum, plain record, and void calls. Record fields retain declared names and
  types, including nested records; returned fields are checked before use.
  Numeric, boolean, string, and plain-record slice parameters can cross a synchronous host
  call. The host edits a typed element array, which the VM validates and copies
  back into the borrowed Ziran array. Source and saved bundles and generated
  C, C++, and Go run the same buffer and record slice examples. The VM rejects
  overlapping mutable host slice arguments and malformed returned elements.
  Empty fixed arrays can be borrowed as zero-length slices for synchronous
  host calls. Host-returned zero-length slices may have no elements; oversized
  returned lengths fail before allocation.
  Fixed arrays also cross the portable host boundary by value, including
  zero-length arrays and arrays of records with array fields. Returned shapes
  are checked recursively; `tests/host_arrays.sh` covers source and saved
  bundles, malformed host results, and native C, C++, and Go foreign calls.
  `BundleRun` checks all required bindings before execution.
  The CLI runner also checks required bindings before execution. Source and
  saved-IR bundles, unused extern pruning, list tampering, missing binding
  preflight, malformed record returns, and pointer dereference rejection
  are tested. Declared pointer types cross host calls as opaque handles with
  kind `VM_HOST_POINTER`: the VM stores the address, compares handles against
  `null` and each other, and never dereferences them, so the host owns the
  storage behind a handle. Pointer-bearing records, including host-returned
  records, bundle and run with that contract. Array and slot host calls
  remain unsupported.
- Portable strings now carry immutable UTF-8 bytes with exact byte lengths,
  including embedded nulls. The verifier and interpreter cover literals,
  equality, read-only byte indexing and ranges, `.count`, parameters, returns,
  and record fields. `text[low:high]` returns a borrowed byte range as a
  `string`; bounds are checked in the portable VM and generated C, C++, and
  Go. Callers must choose UTF-8 codepoint boundaries when slicing text.
  `TextView([]u8)` borrows a byte slice as text in native C and C++; Go and
  the bundle runner copy it into managed storage. The bounded
  `std/text_buffer.zi` helper reads a NUL-terminated buffer through this
  operation. The dedicated gate exercises C, C++, Go, bundles, and invalid
  non-byte arguments. Borrow origins now flow through record/array values and
  checked calls, so returning a record containing a `TextView` of a local
  byte array is rejected; `tests/aggregate_view_lifetime.sh` covers literal,
  nested, array-field, assigned-field, and call-result escapes. The mutation
  gate follows local bindings, field paths, aliases, checked-call results,
  and record-return aliases. It permits writes to sibling record fields while
  protecting the borrowed field, its array storage, and whole-aggregate
  replacement. View-bearing globals retain their global-backing origin across
  functions and alias chains: local-backed views cannot escape into global
  storage, and mutation of a tracked backing global is rejected in any
  function; tracking scales to the program's global count. A view through a
  local `*value` pointer tracks its pointee and rejects mutation through either
  the original binding or the pointer while live. Unknown heap-pointer and
  host-backed origins still need a complete cross-target contract.
  `make check` compares source and saved-IR bundles with C, C++, and Go on a
  string program and rejects out-of-range indexing.
- Portable bundles retain referenced module globals, including records and
  fixed arrays. Explicit initializers run before the first statement of
  every `BundleRun`: integer and boolean literals, compile-time constant
  expressions over defines, float literals, string literals, and nested
  `Type.{.field = value}` record and fixed-array literals fold into the
  global's slot, with
  mutable initialized globals across calls and byte-identical source and
  saved-IR bundles; C, C++, and Go emit the same initializers natively
  (designated initializers in C, positional in C++, struct literals in Go).
  Checked runtime initializers can call procedures and read globals. Source
  and saved IR agree on C, C++, Go, and `.zib`; imported modules initialize
  before consumers, including imports used only for startup effects, and each
  portable instance initializes once. String globals and string fields in
  record and array initializers decode
  escapes and retain their bytes for the instance lifetime. Tests compare
  contents across repeated runs and fresh instances. Native C, C++, and Go
  lower nested record and array initializers from the parsed expression tree;
  the cross-target test covers reordered fields and string escapes.
  The VM reads and writes
  globals across
  imported function calls, and reclaims replaced values without losing
  globals reached by a caller. Source and saved `.zir`, `.zib`, C, C++, and Go
  tests cover value isolation and repeated mutation. Each `BundleRun` starts
  fresh; `BundleInstantiate` preserves globals across repeated
  `BundleInstanceRun` calls on one instance.
- Portable execution borrows read-only record and array parameters and
  reclaims temporary values after ordinary function calls while retaining
  copied results. A repeated nested call over a 4,096-record array checks
  value isolation without exhausting the VM's allocation limit. Replaced
  record and array storage is reclaimed after a value copy; the portable VM
  currently allows up to 256 MiB each of tracked record and array allocations
  so large checked parsers such as Kryon's KSS parser can execute.
- The portable runner has pointers to its own storage: `*place` takes the
  address of a local, global, record field, or array element; `<<p` and
  `p.*` read and write through it; `p.field` reaches a pointed-to record; and
  pointers compare by target and with `null`. A record or array whose
  address was taken stays alive while a pointer can reach it, and a pointer
  at a local fails cleanly once its call has returned. Pointers returned by
  host capabilities stay opaque handles: a bundle can store, compare, and
  pass them back, but not read through them. Pointer casts and arithmetic
  remain outside the portable subset. `tests/portable_pointers.sh` checks
  source and saved IR on C, C++, Go, and `.zib`.
- Compiler functions use short names without `Zir` or `zir_` prefixes,
  including the IR serializer's `ProgramWrite`, `ProgramRead`, and `PathIsIR`.
  IR data types retain `Zir` names for now.
- The inherited runtime type and enum-member lookup stubs are gone. Local and
  imported declarations now supply those names without a compiler fallback.
- Native Go no longer treats ordinary imported record types as types from the
  legacy Kryon Go runtime. The standalone record-call test asserts that no
  Kryon package import is generated.
- Native Go now uses declared record fields and ordinary call resolution even
  when a record or function has a name previously reserved for Kryon UI. A
  standalone source and saved-IR test covers these name collisions. Generated
  Go defaults to package `ziran` unless `--pkg` overrides it.
- Native Go no longer has an implicit Kryon package import or a separate
  `--runtime-implementation` mode. Explicit Go extern targets generate package
  imports; host externs use a declared embedding interface. Source and saved
  `.zir` tests run both forms without Kryon.
- The shared emitter now has only C, C++, and Go targets. Its unused JavaScript
  path and hardcoded Kryon calls have been removed.
- Native C++ lowers Jai prefix pointer record fields through the same scalar
  type mapping as C. A non-UI source and saved-IR record with `*s32` and
  `*u8` fields compiles and runs on C++.
- Native C, C++, and Go accept Jai `null` for explicitly typed raw pointers, in
  comparisons, assignments, calls, and pointer-returning conditionals. The
  checker rejects untyped `null` bindings and scalar assignments.
  Native pointer expressions use Jai `*value`
  for address-of and `pointer.*` or prefix `<<pointer` for dereference,
  including writes through a pointer. `++`/`--` are rejected. The checker rejects
  addresses of temporaries and dereferences of non-pointers. Raw pointers
  can also be indexed on C, C++, and Go native targets when the caller knows
  the allocation bounds. Source and saved `.zir` agree. Raw pointers remain
  outside `.zib`.
- The shared checker resolves `record_pointer.field` against declared record
  fields, including imported records, and reports unknown fields. Checked
  pointer-member reads and writes now emit from source and saved `.zir` on
  native C, C++, and Go, including nested fields and computed pointer bases.
  The C-style `->` source operator is rejected. Type annotations require Jai
  prefix `*Type`; suffix `Type*` and `const` qualifiers are rejected in source
  and saved IR. Existing downstream Kryon source using those spellings still
  needs migration before cutover.
  Raw pointers remain outside `.zib`; broader host-bound native bodies still
  need checked lowering.
- The inherited `#intrinsic "web"` modifier and its two name-based browser
  shims are gone. Browser behavior now requires an explicit host or target
  extern. C/C++ output no longer injects Kryon's inspection header for
  ordinary functions, and generated temporaries and header guards use Ziran
  names. Retired intrinsic, capability, and host import tags have no slot in
  the new IR schema; host calls use ordinary extern imports.
- Unknown directives, function modifiers, declaration modifiers, and top-level
  forms are rejected generically; this includes old `#ui`, `#style`, and
  app/route forms. The embedded Kryon
  runtime declarations and copied `src/kry_std` implementation have been
  removed. JSON diagnostics no longer require Kryon code.
- `#instance` is rejected as a language modifier. Its special statement and
  capture fields, retained-state emission, and IR serialization have been
  removed; a state library can supply that behavior through ordinary calls.
- The parser no longer creates inline closures. Their capture fields, bundle
  cloning, backend wrappers, and VM parent frames have been removed. Named
  procedure values remain supported.
- UI tree and DOM property fields have been removed from the statement IR.
  App, route, style, and inspector metadata have been removed from the IR and
  backend entry paths. Generic calls named `Image` and record calls
  named `Button` are covered by standalone tests. The Go backend no longer
  drops standalone `BeginTree` and `EndTree` calls or maps `Canvas` and
  `CanvasResult` by name. The parser no longer recognizes old `app` and
  `route` block words. Source, saved-IR, and portable bundle tests cover
  these names as ordinary declarations and calls.

- Scalar `#law NAME theorem bindings => condition;` and `#proof NAME { ... }`
  certificates support reflexivity, modular polynomial normalization, exact
  order, boolean case splits, unfolding pure acyclic scalar procedures,
  equality rewriting, and checked theorem application. Missing/unsupported
  proofs and exhausted budgets are unknown; invalid certificates and cycles
  cannot be waived. Evidence distinguishes structural checks, concrete
  evaluation, finite exhaustion, and full-domain scalar kernel proofs.
  `.zir` and `.zib` store and recheck certificates and verification dependencies;
  forged graph references, steps, and evidence are rejected. Lean 4.34.1
  validates the mathematical rules and an independent interpreter checks
  generated C kernel claims. The C port and procedure projection remain
  trusted, and arbitrary-sequence induction remains future work. See
  [LAWS.md](LAWS.md) and `tests/proofs.sh`, `tests/law_integers.sh`,
  `tests/proof_kernel.sh`, and `make proof-model`.
- Named `#law NAME kind payload;` obligations are implemented for the
  `type`, `bounds`, `effect`, `abi`, `custom`, and `forall` kinds (`forall`
  is exhaustive over integer ranges, enum types, and bounded integer
  sequences (`[N]LO..HI`), with counterexamples
  and a 1,000,000-case budget): `ziran check`
  prints one JSON result object per obligation, `disproved` and unwaived
  `unknown` results fail the build, `#law_waive NAME "reason";` waives an
  `unknown`, and the tables serialize per module in `.zir` (byte-identical
  from source and saved IR) and as the linked closure in `.zib`, where
  `BundleLaw*` accessors expose them and laws keep the types, procedures,
  and externs they name. Effect laws compare the derived class
  (`#law Name effect Proc == pure;` against pure, observing, mutating, or
  external), bounds laws compare a fixed-array bound (`Type == N`, `>= N`,
  `<= N` over compile-time values), and the `size` kind compares a type's
  laid-out byte size the same way; comparison payloads keep their procedures
  and type fields in linked bundles. Every checked procedure also carries a
  derived
  effect class — `pure`, `observing`, `mutating`, or `external` — computed
  from its body to a fixpoint and serialized in its `.zir` signature as ABI.
  `#parallel for` regions ride the range lowering into the checked `while`
  and the region checker accepts a body only when every called procedure is
  `pure` or `observing`, writes target iteration-local bindings, Vec
  mutations stay on region storage, regions do not nest, and the shape is a
  `for`; source and saved IR build byte-identical bundles everywhere.
  Native C99 and C++ targets execute forward regions on pthreads through
  `ziran_parallel.h`: each region becomes a file-scope worker over
  contiguous index chunks with read-only captures passed by value, the
  thread count comes from `ZIRAN_PAR_THREADS` (default 4, capped 64), and
  files containing workers compile with `-pthread`. Reverse regions, the
  portable VM, and Go run serially with an emitted downgrade marker, as the
  backend-duties clause permits; region-leaving `return`/`break`/`continue`
  are rejected so no schedule can skip iterations. `#parallel_gpu for`
  regions validate before offload per the GPU clause: pointer storage,
  pointer-bearing call arguments, and allocator-backed Vec mutation are
  rejected, and external-class calls were already barred by the region
  rules; with no GPU device capability in this repository every GPU region
  runs on the CPU worker path with an emitted downgrade marker. Actual
  device offload to a GPU API remains future work.
- `ziran build --target=plan9-c` dispatches the canonical Plan 9 C target and
  `zi2c --target=plan9-c` accepts it directly. The shared C output is followed
  by a Plan 9-safe post-pass and native runtime header. The runtime supplies
  Plan 9 scalar, string, bounds, and vector helpers instead of hosted C headers;
  the capability contract marks it experimental, 386-only, and serial. An
  exported `main` becomes `ziran_plan9_main` and a Plan 9 `void main` wrapper
  passes its status to `exits`. `tests/plan9_target.sh` checks
  dispatcher/direct-tool equality, loop scoping, vector/string output, status
  propagation, and executes the dialect through a minimal fake Plan 9 libc
  when no Plan 9 compiler is installed.
- `ziran explain` exposes a stable registry for every diagnostic code currently
  emitted by the compiler, with textual and JSON output. `ziran explain --list`
  enumerates the registry, and `tests/explain.sh` fails if a new emitted code is
  omitted. Native C, C++, and Go lowering reports malformed module state and
  output-file failures through registered target codes, including JSON lines
  for unwritable output destinations.
- Whole local aggregates, fresh aggregate call results, vector-bearing
  record-literal fields, and plain record-member `Vec` paths rooted at local
  aggregates can move through declaration, assignment, argument passing, and
  return. C, C++, Go, and the portable VM recursively drop vectors reached
  through record fields and fixed arrays; a member move clears only the native
  source field and invalidates only the corresponding VM slot.
  `tests/aggregate_vec_safety.sh` checks direct and nested member paths,
  sibling-field usability, branch returns, use-after-move, global-source,
  indexed/pointer limits, overwrite rejection, allocator balance,
  source/saved-IR equality, and all four execution paths.
- Validated benchmark harnesses record compilation and runtime samples with
  command lines, versions, hashes, hardware, and checked outputs. The multilingual
  UTF-8 harness compares source and saved IR across C, C++, Go, `.zib`, and
  handwritten C, C++, Go, Rust, Java, JavaScript, and Python implementations.
  The record-call harness covers fixed arrays, record value parameters, field
  access, wrapping `s32` updates, and an ordinary call in every round. It
  compares source and saved IR through generated C, C++, Go, generated Rust
  when Cargo is installed, and `.zib`, plus the available comparison languages.
  The current clean-head baseline contains 238 validated samples across 58
  phase/case combinations in `bench/results/2026-09-28-record-calls-rust/`;
  the earlier 206-sample baseline in
  `bench/results/2026-09-28-record-calls/` predates generated Rust.
- Package manifests and lockfiles can pin source-only dependencies without a
  Ziran manifest. Resolution, checkout, map generation, drift detection, and
  `ziran add --source` are covered by `tests/packages.py`.
- Polymorphic procedures bind `$T` through a direct, slice (`[]$T`), or
  pointer (`*$T`) parameter. `tests/generic_slice_parameter.sh` runs slice
  binders over scalars, 64-bit values, records, and subslices through source
  and saved IR on `.zib`, C, C++, Go, Rust, and Python, runs pointer binders
  natively, and rejects non-slice and non-pointer arguments. `std/sort.zi`
  builds an allocation-free heapsort, `IsSorted`, `LowerBound`, and
  `BinarySearch` on them; `tests/sort.sh` checks edge cases on `.zib` and C.

## Still required

- Finish Jai parity and specialization: cover remaining expression forms,
  exact foreign array ABI behavior (including nonempty `.data`), generic fields
  whose types depend on specialization parameters, and broader procedure-value
  support.
- Complete ordinary-library procedure values and imports. Named capture-free
  callbacks and stored procedure values already work across native targets and
  `.zib`; procedure-valued defaults and the remaining general import forms do
  not.
- Move remaining target-specific declaration and import lowering from text into
  typed IR, then enforce target capability errors uniformly.
- Extend the `.zib` linker and verifier to all checked expressions and native
  features, including complete host capability coverage. The current bundle
  embeds checked `.zir` and follows direct calls, but it remains a subset.
- Complete ownership edge cases: unknown heap and host-backed view aliases,
  indexed and pointer-backed field moves, and reclaimable storage detached by
  `BuilderFinish`.
- Expand numerical conformance IDs and the initial feature registry to the full
  language surface, complete JSON diagnostic fields, and cross-target
  capability reporting.
- Complete native backend parity, FFI capability checks, law coverage, Plan 9
  target-aware lowering, and downstream application gates, then make the
  planned single breaking cutover. See [Migration](MIGRATION.md).
