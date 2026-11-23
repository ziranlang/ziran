# Go target checks grouped (2026-11-23)

Owner rule (2026-11-17): replace old and duplicate checks with fewer
table-driven Ziran checks, never drop coverage, and map every removed check to
its replacement.

## Open

- `go_codegen`: merge `tests/go_assembly.sh` and `tests/go_direct_assembly.sh`
  into one check that runs `go_assembly_cases.zi` and
  `go_direct_assembly_cases.zi` through `RunCases` (`tests/case_pool.zi`),
  264 -> 263 checks. Blocked on the compiler: `ziran features --json`
  (`cmd/package_features.zi`, mirrored in `docs/FEATURES.json`) names both
  wrappers as evidence and `tests/compiler_inventory_cases.zi` requires every
  evidence path to exist. First point that evidence at the two `_cases.zi`
  files (they exist today), then delete the wrappers and register the check in
  `tests/check_plan.zi`; test with `make check CHECK_FILTER=go_` and
  `CHECK_FILTER=compiler_inventory`. Prose that names the wrapper:
  docs/USAGE.md, ASSEMBLY/feature-matrix.md.
- `make check-incremental` pays for itself only after more checks are audited
  in `tests/check_cache_audits.json`. Measured on Omega: computing the input
  fingerprint takes 12-15 s per run, and the one audited check
  (`fuzz_regressions.sh`) runs in under 0.1 s, so today the target is slower
  than `make check`. The checks that set the suite's wall time compile C, run
  Go, Wine or a browser, none of which the fingerprint covers, so they stay
  unaudited and always run. To make it useful: hash the inputs in one pass in
  `tests/check_cache.zi`, then audit the checks that use only the toolchain and
  the fingerprinted shell tools; test with `check-runner --self-test`.
- Twelve shell files only launch one Ziran check program and could become
  rows of `check_programs` in `tests/check_plan.zi` (same check count, twelve
  fewer shell files): constant_time, executable_bundle, float_integer_casts,
  go_assembly, go_direct_assembly, gzip, native_cache_parallel, native_entry,
  packages, queue, regalloc, transport_view. Each needs its extra arguments
  and environment kept; test each with `make check CHECK_FILTER=<name>`.
- `tests/generic_multiple_results.sh` and `tests/proofs.sh` still compile
  `tests/saved_ir_mutations.zi` for each saved IR edit; once they are Ziran
  check programs they import `tests/saved_ir.zi` directly.

## Removed checks and their replacements

Every fixture moved file to file into `tests/fixtures/go/<case>/` (sources as
`.zi`, Go and C hosts with an `.in` suffix). Every command and rejection table
moved into the case procedure named below; the shared steps are in
`tests/go_case_support.zi`.

| Removed check | Replacement check | Case procedure |
| --- | --- | --- |
| `go_bind.sh` | `go_semantics` | `GoBind` |
| `go_defer.sh` | `go_semantics` | `GoDefer` |
| `go_error_values.sh` | `go_semantics` | `GoErrorValues` |
| `go_fields.sh` | `go_semantics` | `GoFields` |
| `go_foreign_types.sh` | `go_semantics` | `GoForeignTypes` |
| `go_many_parameters.sh` | `go_semantics` | `GoManyParameters` |
| `go_maps.sh` | `go_semantics` | `GoMaps` |
| `go_results.sh` | `go_semantics` | `GoResults` |
| `go_select.sh` | `go_semantics` | `GoSelect` |
| `go_slice_nil.sh` | `go_semantics` | `GoSliceNil` |
| `go_c_headers.sh` | `go_exports` | `GoCHeaders` |
| `go_method_exports.sh` | `go_exports` | `GoMethodExports` |
| `go_named_exports.sh` | `go_exports` | `GoNamedExports` |
| `go_package_init.sh` | `go_exports` | `GoPackageInit` |

Two assertions became stricter: a command that must be rejected now has to
exit with a diagnostic (a crash or a timeout no longer counts as a
rejection), and a command whose output is compared must also exit 0.

## Numbers (Omega)

| | Before | After |
| --- | --- | --- |
| Checks in the suite | 276 | 264 |
| The 14 Go checks, one run on an idle machine, 18 jobs | 93.7 s summed over 14 wrappers (semantics 55.2 s, exports 38.5 s) | 17.5 s summed over 2 checks (`go_semantics` 6.9 s, `go_exports` 10.6 s) |
| Full suite wall time, 32 jobs | 93.7 s (2026-11-22, 276 checks) | 121.2 s (2026-11-23, 264 of 264 passed, private `BUILD_DIR`, load average 58 from other lanes) |

The suite's wall time is set by its slowest checks (`zx_web_lifecycle.sh` took
121.2 s in the second run), so grouping the Go checks removes work and
startup, not wall time. The two full-suite runs were a day apart under
different machine load and are not a like-for-like comparison.
