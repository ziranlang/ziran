# Legacy removal plan

Open legacy work in ziranlang/ziran only. Finished items are deleted from this
file; `CHANGELOG.md` and git history record them. Owner rules: "remove" means
delete, never disable, hide or put behind a flag; no alias or compatibility
layers; cut unreached or unused code, never merge files to save size.

Every item below was proven with a caller search over Ziran (code, tests,
scripts, bench drivers, build files) and, for public names, over
`kryonlabs/*`, `harness/harmony`, `ziranlang/packages` and the apps under
`waozixyz/`. `bench/results/`, `CHANGELOG.md` and other plans are history,
not callers.

## Self-hosting C replacement (planned work, not dead code)

The hand-written compiler and backend C is live. It is replaced, not deleted,
by `plans/2026-11-14-self-hosting.md`; each file goes in the step that ports
it, with byte-identical generated output first and `make check-bootstrap` as
the final gate. No alias or compatibility runner survives a step.

- `cmd/zir/` compiler C: 62 `.c` files, about 71,500 lines with headers
  (largest: `zir_emit_range.c`, `zir_check_expr.c`, `zir_parse_typed.c`,
  `zir.c`, `zir_parse_source.c`, `zir_link.c`). Host files
  (`zir_host*.c/.h`) belong to the native runtime lanes.
- Backend drivers: `cmd/zi2c/` (`zir_c_lower.c`, `zir_c_prune.c`, `main.c`),
  `cmd/zi2go/` (`zir_go_lower.c`, `zir_go_asm.c`, `zir_go_amd64.c`,
  `main.c`) and `cmd/zi2zx/` (`main.c`, `zx_exec_run.c`), about 9,400 lines.

## Not legacy (do not delete)

- Old-ZX rejection: `cmd/compiler_bundle.zi` names revisions 26 and 27 and
  refuses them; `tests/zx_executable.sh` and `tests/executable_bundle_cases.zi`
  keep that guard.
- Backend `Auto` in `std/bundle_host.zi` is a whole-instance platform choice;
  `Engine` and `Native` stay strict.
- Bench Python/C drivers and kernels (`bench/*.py`, `bench/*.c`) are live
  measurement and receipt contracts, not duplicates of the Ziran benchmarks.
- `tests/tool_names.sh` and the `Makefile` clean/uninstall rules remove and
  reject stale `zi2asm`/`zi2wasm` installs.
- The old-tier labels in `bench/assembly_compare.zi`, `bench/assembly_collect.zi`
  and `tests/assembly_compare_cases.zi` name the preserved M0 receipts of the
  before/after comparison.
- `site/generated/json-text/` is the generated code `bench/json_text.py`
  published with its measured Alpha run; it changes only with a new run of
  that benchmark, like `bench/results/`.
- The ELF writer's fixed-base relocation checks and the native loader's
  per-architecture relocation code serve different live paths (AOT ELF and
  in-process loading); merging them is a refactor for
  `plans/2026-11-14-native-zx-players.md`, not a deletion.
