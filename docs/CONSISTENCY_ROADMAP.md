# Consistency, tooling, and performance roadmap

This page contains unfinished work. It does not duplicate shipped behavior:
use [implementation status](IMPLEMENTATION_STATUS.md) for detail,
[`ziran capabilities --json`](CAPABILITIES.md) for target facts, and
[`ziran features --json`](FEATURES.md) for tested feature contracts and
examples.

## Priority 1: one safe meaning across targets

1. **Automatic ownership cleanup.** Lower the remaining ownership decisions
   into checked IR rather than separate backend paths, broaden allocation-count
   and AddressSanitizer coverage, and retain source/saved-IR tests across
   backends. Reclaiming the bytes detached by `BuilderFinish` requires an
   explicit string-ownership, reference-counting, or region policy that is
   sound across assignment, calls, returns, ABI boundaries, and cleanup.
2. **Borrowed views.** Choose one observable rule for `TextView` of mutable
   bytes, then make native and portable execution agree. Complete lifetime
   tracking for unknown heap aliases and host-backed views, rejecting aliases
   that cannot be proved safe at boundaries.
3. **Numerical contract coverage.** Extend the stable conformance-ID matrix to
   operators and mixed-width combinations that are not yet represented.
4. **Target boundaries.** Give checked programs explicit target-capability
   errors for unavailable FFI, pointer, union, host, or GPU behavior. Replace
   remaining textual declaration/import lowering with typed IR. A CPU fallback
   must be reported as CPU execution, never as measured GPU work.

Acceptance for this phase: every supported execution path produces the same
observable result for the conformance corpus, and the memory-sensitive corpus
accepts no program that unintentionally leaks, double-frees, or returns a
dangling view.

## Priority 2: one discoverable language contract

- Expand the initial machine-readable feature registry to the full language
  surface, including procedures, modules, ownership, effects, target-specific
  restrictions, and every standard module contract. Drive guide, website,
  reference, capabilities, and feature documentation from one checked source
  rather than parallel hand-maintained copies.
- Finish JSON diagnostics across loading, parsing, checking, cleanup, laws,
  lowering, verification, and execution. Add related spans, expected and actual
  types, target-capability context, and reliable machine-readable suggested
  edits. Test every negative fixture as a JSON line.
- Clarify law evidence. Require unique qualified law IDs, validate waiver
  targets and scope, and report waived obligations explicitly. A quantified
  proof system remains a separate project requiring a trusted kernel,
  termination and effect rules, and explicit unsafe/FFI boundaries.
- Publish exact fixed-size name and source-buffer limits, diagnose excess input
  precisely, then intern type identities and replace spelling-sized buffers
  with structured syntax.

## Priority 3: a fast agent and editor loop

Extend the compiler-local loop with ownership preconditions, target
availability, tested examples, per-code conformance IDs, definition, hover,
completion, rename, and structured edits. A watch mode must safely reuse
checked work and expose an explicit state-versioning contract. The formatter
needs an idempotence and semantic-preservation corpus covering strings and
newer syntax.

Measure agent usability rather than assuming it. Pin model, prompt, context,
tool budget, and tasks; record first-attempt compilation, hidden-test success,
repair attempts, invented APIs, elapsed time, tokens, and cross-target
agreement. Include multi-file edits, ownership mistakes, and requests for an
unavailable feature.

## Priority 4: useful language growth

| Area | Next useful step |
|---|---|
| Errors | Checked active-case access and exhaustive handling |
| Generics and procedures | Multiple independent type parameters, `[N]$T` and generic-record binders, constraints, then capturing closures if needed |
| Collections | Map/set, a stable sort or comparator-based sort for records, more portable I/O and error utilities |
| Parallel work | Deterministic reduction or disjoint output ownership with omitted-iteration tests |
| GPU | A real device backend with transfer and lifetime rules plus differential tests |

Select features from tested CLI, parsing, service, numerical, and Kryon
application programs. Kapsule-specific terminal behavior belongs in Kapsule,
not the general language runtime.

## Performance evidence to collect next

Existing benchmark evidence is linked from [implementation
status](IMPLEMENTATION_STATUS.md); this section records only missing evidence.
Add validated workloads for generic specialization, JSON scanning,
sorting/maps when available, local I/O, compiler-input scaling, and real
applications.

Scale compiler inputs by module count, source size, generic instances,
compile-time evaluation, and laws. Instrument compiler phases and allocations;
do not present separately sampled phase sums as measured end-to-end builds.
Retain raw samples, source/input hashes, tool versions, flags, cache state,
hardware, device, and unsupported cases. Run a short semantic and compilation
corpus on changes and a broader runtime matrix on a stable machine.

Benchmarks for Bend or another implementation must use the same validated
workloads and hardware setup. Record unavailable tools as unavailable rather
than estimating results. No finite suite can rank every language for every
workload.
