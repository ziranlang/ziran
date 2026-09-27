# Consistency, tooling, and performance roadmap

This records the remaining work found in the 2026-09-27 source, documentation,
and cross-target audit. It distinguishes current behavior from desired
behavior. [Implementation status](IMPLEMENTATION_STATUS.md) covers shipped
features in detail; [`ziran capabilities --json`](CAPABILITIES.md) gives a small
machine-readable summary from the installed compiler.

## What exists now

Ziran checks source and saved `.zir`, emits C/C++/Go, runs portable `.zib`,
supports compile-time evaluation, named laws, partial ownership checks,
structured diagnostics, a formatter, packages, and a tested set of standard
modules. `ziran guide` supplies a compiler-local starting point. The
[benchmark harness](../bench/README.md) measures source/saved-IR compilation,
downstream native builds, and validated runtime output for all four current
execution paths. Its [first baseline](../bench/results/2026-09-27/benchmark.md)
contains 206 samples across 50 cases on one machine. A second
[vector-growth baseline](../bench/results/2026-09-27-vector-growth/benchmark.md)
adds 160 samples for collection growth and scanning. Both are workload-specific
smoke checks, not a general speed ranking.

Recent differential tests aligned narrow integer shifts, narrowing casts,
and floating division between native and portable execution. Cleanup and one
emitter path now use structured diagnostics. All four execution paths move and
recursively drop vectors owned by records and fixed arrays, preventing the
reproduced record-copy double-free case, and the checker rejects repeated moves
within an expression. Global `TextView` storage now tracks its backing-global
origin and rejects cross-function mutation through direct and chained global
aliases. These fixes do not complete the ownership or diagnostic contracts.

## Priority 1: one safe meaning across targets

1. **Automatic ownership cleanup.** All four execution paths now release direct
   owned `Vec` locals and parameters on return, break, continue, and normal
   scope close; direct and aggregate moves clear every reachable native source
   field. Lower the remaining ownership decisions into checked IR rather than
   separate backend paths, broaden allocation-count and AddressSanitizer
   coverage, and keep source/saved-IR tests across backends. Reclaiming the
   bytes detached by `BuilderFinish` needs a separate string ownership policy.
2. **Borrowed views.** `TextView` of mutable bytes observes later mutation in
   generated C/C++ but retains the earlier bytes in Go and `.zib`. Choose one
   language rule, then enforce it consistently. The checker now rejects direct
   mutation of local backing storage while a direct or record-held `TextView`
   is live and releases that borrow at scope close. Recursive origin checks
   reject a record literal, nested record, field assignment, or checked-call
   result that returns a view of a local byte array. The mutation gate also
   follows bindings, fields, aliases, checked-call results, arrays, and views
   nested in returned records. View-bearing globals track the global storage
   they alias and reject local-to-global escapes plus cross-function mutation
   through direct and chained global aliases; this rule is intentionally
   program-lifetime and currently supports 64 globals. Views through local
   address-taking pointers protect both their pointee and pointer aliases.
   Unknown heap pointers and other unmodeled mutable aliases still need a
   complete lifetime model; reject escapes that cannot be proved safe. Host
   boundaries remain untracked.
3. **Numerical edge cases.** Expand differential tests beyond the corrected
   8/16-bit shifts and floating zero division: signed overflow, minimum
   integer divided by minus one, all cast widths, negative and oversized
   shifts, NaN comparisons, infinities, and compile-time evaluation. Record
   the specified trap or value for each operation. A saved-IR path must agree
   with source before the result is called portable.
4. **Target boundaries.** Give checked programs an explicit target capability
   error for unavailable FFI, pointer, union, host, or GPU behavior. Replace
   remaining textual declaration/import lowering with typed IR. A CPU fallback
   must be reported as CPU execution, never as measured GPU work.

Acceptance for this phase: the conformance corpus has the same observable
result on every supported execution path, and the memory-sensitive corpus has
no accepted safe program that unintentionally leaks, double-frees, or returns
a dangling view. The currently process-lifetime `BuilderFinish` storage needs
its own reclaimable-string or region policy.

## Priority 2: one discoverable language contract

- Maintain a versioned feature registry with syntax, target support, known
  limits, examples, and conformance-test IDs. Use it to verify the guide,
  website, reference docs, and `capabilities` output. Compile accepted examples
  and verify explicitly rejected examples. Keep roadmap goals separate from
  shipped behavior.
- Finish JSON diagnostics across loading, parsing, checking, cleanup, laws,
  lowering, verification, and execution. Some error paths still print plain
  text under `--diagnostics=json`. `ziran explain` now exposes a tested registry
  of every emitted code, but diagnostics still need related spans,
  expected/actual types, target capability, and machine-readable suggested
  edits where reliable. Test every negative fixture as JSON lines.
- Clarify law evidence. `custom` laws evaluate closed compile-time expressions;
  `abi` laws check a supported shape, not foreign implementation correctness.
  Require unique qualified law IDs, validate waiver targets and scope, and
  report waived obligations explicitly. A quantified proof system would be a
  separate project with a trusted kernel, termination and effect rules, and
  explicit unsafe/FFI boundaries.
- Publish exact supported limits for fixed-size names and source-text buffers,
  then diagnose excess input precisely. Longer term, intern type identities and
  use structured syntax rather than relying on spelling-sized buffers.

## Priority 3: a fast agent and editor loop

The useful default cycle is query the compiler's guide, capabilities, and
[checked APIs](API_DISCOVERY.md), edit a small unit, format, check laws, run
tests, and inspect structured failures. The first API query includes visible
imports, constants, globals, types, signatures, defaults, and effects from
source or saved IR. Extend it with ownership preconditions, target
availability, and tested examples. Extend the initial `explain` registry with
per-code conformance IDs, then add editor diagnostics, definition, hover,
completion, and rename from compiler data. A watch mode
should reuse checked work safely; live state reload needs an explicit
state-versioning contract. The formatter should have an idempotence and
semantic-preservation corpus, especially for strings and newer syntax.

Measure agent usability rather than assuming it: pin model, prompt, context,
tool budget, and tasks; record first-attempt compilation, hidden-test success,
repair attempts, invented APIs, elapsed time, tokens, and cross-target
agreement. Include multi-file edits, ownership mistakes, and requests for an
unavailable feature. No such LLM task evaluation has yet been run.

## Priority 4: useful language growth

| Area | Current boundary | Next useful step |
|---|---|---|
| Errors | `Option` and `Result` are ordinary records; invalid payload reads are representable | Checked active-case access and exhaustive handling |
| Generics and procedures | Generic records take multiple type parameters; procedures bind one, and procedure values are capture-free | Multiple independent procedure parameters, constraints, then capturing closures if needed |
| Collections | `Vec`, text/UTF-8, JSON scanning, ZIP, and selected host contracts exist | Map/set, sort/search, more portable I/O and error utilities |
| Parallel work | C/C++ launch workers for legal forward regions, but useful loop results cannot be combined | Deterministic reduction or disjoint output ownership with omitted-iteration tests |
| GPU | Checked annotation reaches a CPU fallback | A real device backend with transfer/lifetime rules and differential tests |

Do not reintroduce rejected legacy syntax merely to grow the feature list.
Select features from tested programs in CLI, parsing, services, numerical work,
and Kryon applications. Kapsule-specific terminal behavior belongs in Kapsule,
not the general language runtime.

## Performance evidence to collect next

The current benchmarks cover an integer recurrence and vector growth followed
by a scan, with process startup in runtime timings. They separate Ziran
emission from host compiler work and mark Go build times as cache-warm. The
integer suite can compare Rust, Java, JavaScript, and Python when installed;
the vector-growth harness now has the same optional comparisons.
A [provisional multilanguage vector run](../bench/results/2026-09-27-vector-growth-multilang/README.md)
validated 206 samples across 50 phase/case combinations.
Generated C/C++ were close to handwritten equivalents on these kernels.
Generated Go took about 2.6 times handwritten Go on the integer kernel and
about 2.8 times on the original large vector-growth baseline. The integer
loop calls general numeric helpers five times per iteration. A
[paired vector-growth comparison](../bench/results/2026-09-27-vector-growth-go-fastpath/README.md)
found that inlinable wrapping add/multiply helpers halved generated Go's
elapsed time for that workload. The remaining vector-lowering gap needs a
profile before assigning its cause. These observations are workload-specific.

A [validated UTF-8 scanning matrix](../bench/results/2026-09-27-text-scan-multilang/README.md)
covers source and saved IR through C, C++, Go, and `.zib`, plus handwritten
C, C++, Go, Rust, Java, JavaScript, and Python. Add validated workloads for
arrays, records/calls, generic specialization, JSON scanning, sorting/maps
when available, local I/O, and real applications. Scale compiler inputs by module count, source size,
generic instances, compile-time evaluation, and laws. Instrument compiler
phases and allocations; do not present sums of separately sampled phases as
measured end-to-end builds. Retain raw samples, exact source/input hashes,
tool versions, flags, cache state, hardware, device, and unsupported cases.
Run a short semantic and compilation corpus on changes, and a broader runtime
matrix on a stable machine. Benchmarks for Bend or any other implementation
belong in the same validated workload and hardware setup; unavailable tools
are recorded as unavailable, not assigned estimated results. No finite suite
can rank every language for every workload.
