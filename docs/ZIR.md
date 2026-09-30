# Ziran intermediate representation (`.zir`)

This is the target contract. An experimental binary version 52 now exists for
the tested C/C++/Go and portable scalar/record/enum subsets. It is not yet the complete contract below; see
[Implementation status](IMPLEMENTATION_STATUS.md).

`.zir` is a versioned, checked, serializable module representation. It is the
shared input to native backends and the portable linker. It records module and
symbol identities, imports/exports, resolved types, constants, function
bodies, control flow, source locations, declared FFI, and host capability
requirements. Backends may attach target-specific output metadata outside the
language model, but they must not reinterpret source text to recover program
meaning.

The Ziran compiler provides a canonical writer and a validating reader. The
reader rejects unknown incompatible versions, truncated or malformed data,
invalid symbol references, and invalid types with useful diagnostics. The
writer produces deterministic bytes for equivalent checked modules. A backend
given saved `.zir` has the same language semantics as one given the `.zi`
source that produced it. Diagnostics retain source paths and spans where
available.

The IR represents general calls, records, slots, imports, and capabilities.
Scalar types use the current source names (`s32`, `s64`, `float32`, and
`float64`, for example); `int` and `float` aliases are canonicalized before
serialization. Earlier internal `i32`/`i64`/`f32`/`f64` names are not accepted.
It has no widget kind, `#ui` flag, DOM attribute cache, KSS node, or implicit
Kryon module. A Kryon widget call has the same representation as a call to an
unrelated imported function with the same type shape.

`.zir` is the compiler interchange and cache format. Portable distribution
uses a linked `.zib`, not an unlinked `.zir`. See [Bundle format](ZIB.md).

## Experimental version 52

Foreign imports retain typed parameters, return types and a checked
`go_results` flag for packing native Go results into record fields in order.
The checked `go_field` flag describes typed foreign Go field getters and
receiverless package-value getters. Package-value getters have no parameters
and read the target variable or constant on each call. Native interface
identity is preserved, and getters cannot return owned vector storage.
`go_defer` schedules a native void call at the calling Go function's exit,
including panic unwinding. These flags require explicit Go foreign targets
and cannot be combined. Deferred calls must be standalone statements with
no owned arguments or variadic parameters. Go
method/field targets retain their receiver type, for example
`go:net/http.(*Request).RemoteAddr`. Native emission uses these checked fields;
the original foreign declaration string is diagnostic metadata.

Overloaded procedures each carry a unique `name` and share their source
name in `overload_name`; checked calls already name the chosen overload.

A record type flagged `is_results` holds the results of a procedure with
several results, in fields `value_0`, `value_1`, and so on; the source's
multiple returns and bindings are already lowered to that record.

Law records include structured evidence. Scalar theorem propositions and
`#proof` certificates have separate expression graphs and ordered proof steps;
they are not executable functions. Consumers retype those graphs and recheck
each certificate against the checked procedure bodies. Cached evidence and
annotations cannot authorize a proof. See [Laws and scalar proofs](LAWS.md).

The current writer emits `ZIR` followed by a zero byte, a little-endian
version number, and length-prefixed checked module records. Strings and
integers are encoded explicitly; process pointers and C struct padding are
not serialized. The reader rejects wrong versions, truncated records,
oversized fields, invalid references, and trailing bytes. The C, C++, native Go, and experimental Plan 9 C
commands can build saved `.zir` modules, including an imported two-module
typed record call example. The experimental `.zib` bundler also consumes saved `.zir`
for its restricted scalar, record, and enum subset. Native and bundle builds rerun
the language checker on the saved expression graph. The reader rejects invalid
references and cycles, and the checked serialization must match the saved bytes.
Every build uses strict checking, including native builds from source.
Checking is unconditional; `--strict` and `--no-strict` are rejected.
Call argument expressions retain source order and store their source name and
checked parameter index. Native emitters and the portable VM evaluate in source
order, then pass values in parameter order. Procedure declarations retain their
default expressions; omitted arguments are materialized as checked call
expressions before serialization. Concrete defaults use checked procedures in
the declaring module so their names keep that module's scope.
Runtime-dependent global initializers lower to private checked startup
functions. Their startup flag is serialized and validated; the linker retains
them with reachable modules and their call dependencies.
For checked functions, statement text is source metadata; the expression graph
and explicit branch flags and loop target IDs determine behavior. The checker
requires each named loop exit to point to a unique enclosing loop. Exhaustive
enum `if #complete value == { ... }` is lowered to typed branches and an
`unreachable` guard before writing IR, so saved modules contain no case selection
statement to reinterpret. Source `variant`, payload `match`, postfix `?`,
`guard`, C-style `switch` and `goto` with labels, `state` blocks, C-style
locals, and raw C statements are rejected. The validating reader rejects
retired statement kinds. The postfix increment/decrement expression kind has
been removed; version 41 rejects earlier files before graph validation.
Variant fields, generated variant function slots,
retained-state fields, and unused statement callee/argument text are absent
from the IR schema.
Generic record templates retain their parameter and field declarations. Explicit
specializations are stored as ordinary concrete types. Direct applications
such as `Vec(u8)` retain their template and argument identities in version 44,
so checked types have the same identity across importing modules and saved IR.
Older binary versions are rejected.
Record field bodies preserve checked `#go_tag` string literals for Go reflection
metadata. These tags do not change field storage in other native targets or
the portable runtime.
Record bodies retain `using` on contained fields. A checked member expression
that names a promoted field records its concrete contained-field path; the
reader verifies each intermediate field is a `using` field. Native backends
and the portable runtime follow that path through the stored record layout.
Union types preserve their shared-storage flag. C and C++ use that flag for
native declarations; Go and the portable linker currently reject union storage.
Polymorphic procedures retain their `$T` signature and structured body.
Templates also retain `using` parameters and statements until concrete
specialization; checked instances contain ordinary member accesses.
Concrete call specializations carry their type and generated identity; imported
saved modules can instantiate them when a source caller uses a new type.
Procedure and foreign declarations retain `#must`, and the checker rejects
discarded results when reading source or saved IR.
Specialized `Vec(T)` records carry an owned-storage bit. The reader validates
their storage shape before allowing vector operations.
Compile-time `#if` selection is complete in the frontend. Saved declarations
carry no C preprocessor guard, and `#assert` failures are diagnosed before
native or portable output. Assertions store their condition and message only;
checking evaluates the condition again when reading saved IR.
Type and constant records preserve export and file-private visibility for
imported-name and loaded-file checking.
Exported procedures retain an optional quoted linker symbol separately from
their Ziran source name.
Native function bodies use checked statement and expression graphs; statement
text remains diagnostic metadata. Version 33 is experimental and has no
compatibility promise.
Expression records preserve whether `#this` selected the enclosing procedure,
so checking saved IR keeps that binding even when a parameter has the same name.

Version 45 stores opaque foreign Go type targets (`go:package.Type`). These
are checked named types and lower to Go aliases, preserving the imported
type's methods and JSON behavior. They have no Ziran record fields or known
layout. Other native targets reject them, and the portable runtime cannot
store or execute them. Older binary versions are rejected.

Version 46 distinguishes native Go maps from records. Their checked body
retains key and value types; Go emits a map alias with shared storage and a
nil zero value. Other targets reject reachable maps. Predeclared `any` and
`error` interfaces use the `go:builtin` foreign namespace without an import.
