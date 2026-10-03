# Compiler capability output

`ziran capabilities --json` prints one JSON object describing the currently
supported output targets and shared limits. Use `--target=c`, `cpp`, `go`,
`rust`, `zib`, or `plan9-c` to select one target. The command works without a project or network
access, so an editor or coding agent can query the installed compiler before
generating code. The `numeric_conformance` object is a stable-ID registry. Its `ids` cite the
numerical cases proven by [numeric semantics](../tests/numeric_semantics.sh)
for C, C++, Go, Rust, Python, and `.zib`; they are described by [numeric_conformance.json](../tests/numeric_conformance.json).
The [width matrix](../tests/numeric_width_matrix.sh) adds 592 independently
generated boundary conversions across all 64 integer type pairs, from source
and saved IR on those six targets.
`schema_version` starts at 1; consumers should reject schema
versions they do not understand and ignore unknown fields within a version.

`tier` says how complete a target is. `primary` targets, C and the portable
`.zib` runner, carry every language feature and gate every change. The
downstream applications build through them. `secondary` targets, C++, Go,
Rust, and Python, may lag behind a new feature, but must reject a program they
cannot run faithfully during checking or lowering, never emit code with a
different meaning. `experimental` is Plan 9 C, described below. New language
work lands on the primary targets first.

`plan9-c` is the canonical interface for Plan 9 C output. Its current
implementation is explicitly reported as `experimental-post-pass`: Ziran still
generates the shared C form and applies a Plan 9-safe rewrite before writing
files. The initial architecture contract is 386 with Plan 9 libc, serial
execution, and no graphics dependency. The post-pass must eventually be
replaced by target-aware lowering and a dedicated Plan 9 runtime before this
target can be declared first-class.

`source_and_saved_ir` reports that the target accepts checked source and saved
`.zir` inputs. `parallel_execution` is `threads` for C/C++ forward CPU regions
and `serial` for Rust, Python, Go, and the portable VM.
It does not promise a parallel result reduction. `gpu_execution` is `cpu_fallback` for every target: there is no
device backend. `text_view_mutable_bytes` reports the current observable
divergence when backing bytes are mutated after making a view: native C/C++/Rust
borrow those bytes, while Go, Python, and `.zib` copy them. Checking rejects mutation
of local backing storage while a direct, field-held, aliased, or
checked-call-produced view is live. Field-sensitive checks protect the
borrowed field or array while allowing sibling record fields to change. Global
views track their backing-global origins and alias chains, and views through
local `*value` pointers protect their pointee and pointer aliases. Unknown
local pointer views are rejected when their backing lifetime cannot be proved.
Local copies of pointer parameters preserve their backing identity. Separate
pointer or slice parameters of the same type are possible aliases. Checked
calls include transitive parameter/global writes, and earlier text arguments
stay live while later arguments and the procedure body execute. Unknown
callbacks conservatively write reference arguments. Differently typed pointer
aliases and arbitrary host-backed aliases still need further coverage; raw
native FFI has no checked memory-effect contract.

`automatic_vec_drop` is `true` for every target: direct owned `Vec` locals
and parameters are released at scope exit or return, as are vectors reachable
through record fields and fixed arrays. Global storage remains shared and is
not automatically released.
`text_view_local_mutation_check` is `true` for every target: checking rejects
mutation of local backing storage while a direct, field-held, aliased, or
checked-call-produced `TextView` is live. Checks are field-sensitive, include
global backing origins, local `*value` pointer aliases, and local copies of
pointer parameters.
`text_view_opaque_pointer_check` reports rejection of `TextView` through opaque
local pointers with unproven backing lifetimes.
`target_preflight` reports the common foreign identity/import and Go/portable
scalar-union checks available through `ziran check --target=...` and builds;
the target's lowerer and verifier still enforce its other limits.
`aggregate_vec_transfer` is `true`: whole local aggregates, fresh aggregate
call results, and vector-bearing record-literal fields can move through
assignment, argument passing, and return, with native targets recursively
clearing every moved vector field. `diagnostics_json` is `partial`: compiler
tools, the package launcher, and formatter report ordinary argument, input,
output, allocation, and native-tool failures as schema-versioned JSON, but
arbitrary external package tools and fatal process signals can still produce
unstructured output. These are current
boundaries, not feature requests or guarantees about unlisted behavior. See
[implementation status](IMPLEMENTATION_STATUS.md) and the
[owned-value contract](OWNED_VALUES.md) for detail.
