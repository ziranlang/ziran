# Go target checks grouped (2026-11-23)

Owner rule (2026-11-17): replace old and duplicate checks with fewer
table-driven Ziran checks, never drop coverage, and map every removed check to
its replacement.

## Open

- Ten shell files only launch one Ziran check program and could become rows
  of `check_programs` in `tests/check_plan.zi` (same check count, ten fewer
  shell files): constant_time, executable_bundle, float_integer_casts, gzip,
  native_cache_parallel, native_entry, packages, queue, regalloc,
  transport_view. Each needs its extra arguments and environment kept; test
  each with `make check CHECK_FILTER=<name>`.
- `tests/generic_multiple_results.sh` and `tests/proofs.sh` still compile
  `tests/saved_ir_mutations.zi` for each saved IR edit; once they are Ziran
  check programs they import `tests/saved_ir.zi` directly.

## Removed checks and their replacements

Every fixture moved file to file into `tests/fixtures/go/<case>/` (sources as
`.zi`, Go and C hosts with an `.in` suffix). Every command and rejection table
moved into the case procedure named below; the shared steps are in
`tests/go_case_support.zi`. `go_codegen` runs the two existing assembly
programs, whose `main` became `GoAssemblyMain` and `GoDirectAssemblyMain`.

| Removed check | Replacement check | Case |
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
| `go_assembly.sh` | `go_codegen` | `GoAssemblyMain` (`tests/go_assembly_cases.zi`) |
| `go_direct_assembly.sh` | `go_codegen` | `GoDirectAssemblyMain` (`tests/go_direct_assembly_cases.zi`) |

Two assertions became stricter: a command that must be rejected now has to
exit with a diagnostic (a crash or a timeout no longer counts as a
rejection), and a command whose output is compared must also exit 0.

## Numbers (Omega)

| | Before | After |
| --- | --- | --- |
| Checks in the suite | 276 | 263 |
| The 16 Go checks, summed | 192.2 s (2026-11-22 full suite, 16 wrappers) | 156.7 s (3 checks in the full suite below: `go_codegen` 86.4 s, `go_exports` 45.0 s, `go_semantics` 25.3 s) |
| Full suite wall time, 32 jobs | 93.7 s (2026-11-22, 276 checks) | 136.4 s (2026-11-23, 263 of 263 passed, private `BUILD_DIR`, load average 64 from other lanes) |
| Input fingerprint of `check-runner --incremental` | 11.0-12.1 s per run | 0.6-0.8 s per run |

The suite's wall time is set by its slowest checks (`zx_web_lifecycle.sh` and
`zx_wasm.sh`), so grouping the Go checks removes work and startup, not wall
time. The full-suite runs were on different days under different machine
load and are not a like-for-like comparison.

The fingerprint now hashes its inputs with one `sha256sum` per 256 files
instead of one per file, and drops duplicate paths after sorting. A
`make check-incremental` target was added and removed again the same day:
the fingerprint covers every input, so after any one-file edit no result is
reused (measured: `1 executed, 0 reused`), and only `fuzz_regressions.sh` is
audited for reuse. The target could not be faster than `make check` after an
edit; `make check CHECK_FILTER=<name>` serves that rerun.
