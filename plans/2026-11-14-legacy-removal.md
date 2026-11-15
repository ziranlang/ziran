# Legacy removal plan — 2026-11-14

Audit-only plan. This document makes no source change. It consolidates every
confirmed legacy deletion in ziranlang/ziran into lane-owned, independently
provable batches, with statuses refreshed against the working tree at commit
`c43f2ba4` plus its uncommitted changes.

Owner rules applied verbatim: 'remove' means remove, never disable or hide;
there are no fallbacks between backends or formats — unsupported inputs get
clear named gaps; rejection tests that enforce current formats are preserved.

## Audit method

- Caller searches with `rg` across `cmd/ tests/ scripts/ bench/ bootstrap/`,
  `Makefile`, and `assembly.mk`. Every 'unused' claim records the search that
  returned no live caller; `bench/results/` logs and `CHANGELOG.md` entries are
  historical records, not callers.
- Supersession evidence from the newer replacements: `cmd/compiler_x86_64.zi` +
  `cmd/compiler_x86_bytes.zi` + the machine pipeline (`compiler_machine_ir`,
  `compiler_machine_lower`, `compiler_machine_verify`,
  `compiler_machine_verify_allocated`, `compiler_machine_elf`,
  `compiler_machine_image`, `compiler_machine_pipeline`) replace the old
  duplicate encoder; `std/bundle_host.zi` `HostValue`/`HostCall` already use
  the post-VM names at the Ziran surface; `cmd/zi2zx`, `cmd/zx2wasm`,
  `cmd/zx2native`, `cmd/zi2c`, `cmd/zi2go` replaced the old tool folders.
- Build wiring read from `Makefile` and `assembly.mk` in this tree.
- Line anchors and byte sizes below were refreshed immediately before writing
  against the dirty working tree. Refresh them again immediately before each
  implementation session.

## Concurrency note

The working tree carries another task's uncommitted native-runtime and
compiler work. Do not edit any of these files in this plan's batches until
that work lands and the anchors are re-checked:

- `cmd/compiler_machine_image.zi`, `cmd/compiler_machine_verify.zi`,
  `cmd/compiler_machine_verify_allocated.zi`, `cmd/compiler_machine_elf.zi`,
  `cmd/compiler_machine_lower.zi`, `cmd/compiler_x86_64.zi`
- `cmd/zir/zir_host.c`, `cmd/zir/zir_host_exec.c`, `cmd/zir/zir_host_exec.h`,
  `cmd/zir/zir_host_native.c`, `cmd/zir/zir_host_native.h`,
  `cmd/zir/zir_simd.h`, `cmd/zir/zir_runtime.c`, `cmd/zir/zir_runtime.h`
- `cmd/zx2native/main.c`, `cmd/zi2c/main.c`, `cmd/zi2c/zir_c_lower.c`
- `scripts/embed_headers.sh`, `tests/host_capability.sh`,
  `tests/host_capability_test.c`, `tests/native_runtime_headers.sh`
- untracked `cmd/compiler_runtime_emit.zi` and `bootstrap/compiler_runtime_emit/`

Items whose evidence overlaps those files are marked `recheck-concurrent` and
carry no deletion action yet.

## Status vocabulary

`done` — verified absent or replaced in this tree. `in progress` — concurrent
uncommitted work covers it. `pending` — confirmed, not started. `pending
audit` — candidate that still needs its caller-search proof. `recheck` —
blocked on the concurrent tree; do not touch. `preserve` — looks legacy, is
contract or history; never delete.

## Lane summary

| Lane | Items | Done | In progress | Pending | Pending audit | Recheck | Preserve |
|---|---:|---:|---:|---:|---:|---:|---:|
| Engine | 1 | 0 | 0 | 0 | 0 | 0 | 1 |
| Native compiler | 5 | 4 | 0 | 0 | 0 | 1 | 0 |
| Native runtime | 4 | 0 | 0 | 2 | 0 | 1 | 1 |
| Self-hosting | 4 | 2 | 0 | 2 | 0 | 0 | 0 |
| Free | 11 | 10 | 0 | 0 | 0 | 0 | 1 |
| Total | 25 | 16 | 0 | 4 | 0 | 2 | 3 |

Top 10 legacy items by current byte size (the old encoder's dedicated test
moved to `tests/spec/` since the first audit):

1. `cmd/compiler_lowered_x64.zi` — 131,010
2. `tests/spec/compiler_lowered_x64_test.zi` — 124,810
3. `bench/zx_metadata.py` — 9,156
4. `tests/native_services_cases.zi` — 7,981
5. `tests/vec_capacity.zi` — 7,496
6. `tests/spec/vm_packed_arrays_test.zi` — 4,761
7. `tests/spec/sequence_header_test.zi` — 3,530
8. `tests/spec/file_copy_new_test.zi` — 3,075
9. `bench/zx_metadata.c` — 2,199
10. `tests/spec/calendar_test.zi` — 2,178

## Engine lane (`cmd/compiler_exec*.zi` and bundle execution)

### E1 — old-ZX rejection — preserve/rejection (source wording done, bootstrap regen recheck)

- Paths: `cmd/compiler_bundle.zi:595` (the `.Legacy` message now says 'rebuild
  with zi2zx'; previously zi2asm), rejection test
  `tests/executable_bundle_cases.zi:386`.
- Evidence: semantic-IR ZX must stay rejected; the `.Legacy` problem kind is
  the current-format guard, not dead code.
- Replacement: none needed. Keep the enum case, message, and test.
- Risk: none; deleting it would accept old containers.
- Proof: `make check CHECK_FILTER=executable_bundle`.
- Follow-up: generated `bootstrap/compiler_bundle/compiler_bundle.c:1792`
  still contains the old 'zi2asm --emit zx' string; regenerate bootstrap
  (never hand-edit generated C) — see F9.

## Native compiler lane

### N1 — delete the duplicate x86-64 encoder — done (2026-11-15)

- Done: both files and `tests/compiler_lowered_x64.sh` are deleted; no `LirX64`
  or `compiler_lowered_x64` reference remains in `cmd/`, `tests/` or the build.


- Paths/ranges: delete `cmd/compiler_lowered_x64.zi` (whole file, 131,010
  bytes) and its dedicated test `tests/spec/compiler_lowered_x64_test.zi`
  (124,810 bytes) with runner `tests/compiler_lowered_x64.sh`.
- Evidence: duplicate implementation. The newer route is
  `cmd/compiler_x86_64.zi` + `cmd/compiler_x86_bytes.zi` + the machine
  pipeline. Remaining `LirX64` consumers: `cmd/zx2native/main.c:6,1035,1039`,
  `tests/native_services_cases.zi:2,13-17,26-38`, the dedicated test, wiring
  `assembly.mk:4,25-26`, and prose `ASSEMBLY/native-aggregate-abi.md`.
- Replacement: machine pipeline exclusively (N2/N3 first).
- Risk: platform-service, float-format, and encoder-bound coverage currently
  proves against the old encoder; N3 must port it first. Docs referencing
  `LirX64` update in the same batch.
- Owning lane: native compiler.
- Proof: `make check CHECK_FILTER=machine_elf`, then full `make check`.

### N2 — cut `zx2native` over to machine-only and delete old branches — done (2026-11-15)

- Done: `zx2native` always runs the machine pipeline behind the shared
  preflight; `--machine`, the dead Wasm branch, the old encoder and
  `assembly_direct_map` are gone.


- Paths/ranges: `cmd/zx2native/main.c` — old includes `:3` (`zx_wasm_lower.h`)
  and `:6` (`compiler_lowered_x64.h`); `--machine` flag `:780` and usage
  `:29-31` (redundant once machine is the only path); dead Wasm branch
  `:936-944` (local `wasm` is initialized `:810` and never set true; no flag
  parses it); old-encoder default branch `:984-1040` (`LirX64Elf` calls at
  `:1035,:1039`); old direct map `assembly_direct_map` `:585-743`, called at
  `:1042` and `:1073`.
- Evidence: the machine branch exists (`:946-983`), the machine map exists
  (`assembly_machine_map` `:357`), and `--machine` merely selects it today.
- Replacement: run `assembly_machine_gap` + `MachineNativeBuild`
  unconditionally; keep machine map output; unsupported forms keep the
  existing named `assembly.gap.*` diagnostics with no alternate engine.
- Risk: map-output parity for the non-machine `--emit map` path; covered by
  machine ELF/map tests. Cross-repo callers of removed flags: none found in
  this repository; the VS Code stage graph is the sibling repo's pending work
  (tool-names Session 7).
- Owning lane: native compiler.
- Proof: `make check CHECK_FILTER=machine_elf`,
  `make check CHECK_FILTER=tool_names`, full `make check`.

### N3 — port `tests/native_services_cases.zi` to the machine API — done (2026-11-15)

- Paths/ranges: `tests/native_services_cases.zi:2` now imports
  `compiler_machine_pipeline`; `Encode` and `Symbols` use machine owners.
- Evidence: old-encoder section/symbol layout has no machine-ELF equivalent;
  machine ELF is deliberately sectionless. The retained suite proves the typed
  platform-service declaration, writable global identity and machine-program
  calling contract. `machine_elf.sh` retains named route-gap rejection coverage.
- Replacement: machine pipeline records plus shell route gaps; no compatibility
  symbol table was invented.
- Owning lane: native compiler.
- Proof: direct native-services `ziran run` passed. The combined `machine_elf`
  filter remains blocked by concurrently owned machine-ELF verifier work.

### N4 — clean `assembly.mk` old-encoder wiring — done (2026-11-15)

- Done: `ASSEMBLY_SOURCES` and the generation command no longer name the old
  encoder; the bootstrap matches (`make check-bootstrap`).


- Paths/ranges: `assembly.mk:4` (`ASSEMBLY_SOURCES` contains
  `cmd/compiler_lowered_x64.zi`) and `:25-26` (the zi2c generation command
  passes it).
- Evidence: the only build wiring left for the old encoder.
- Replacement: machine-only source list; regenerate generated C.
- Risk: bootstrap regen required (`update-bootstrap`); see F9.
- Owning lane: native compiler.
- Proof: `make check CHECK_FILTER=tool_names`, then full `make check`.

### N5 — relocation duplication and host-image conversion dedup — recheck-concurrent

- Evidence: dirty `cmd/compiler_machine_elf.zi` (+118 lines),
  `compiler_machine_image.zi`, `compiler_machine_lower.zi`,
  `compiler_machine_verify*.zi`, `compiler_x86_64.zi` are another task's
  in-flight work on exactly the 'fixed-base duplicate relocation logic in the
  ELF writer' and 'duplicated interface/host-image conversion' ledger rows.
- Action: none now. Re-anchor after that work lands; do not edit these files.

## Native runtime lane

### R1 — rename VM-era public C names, no alias layer — pending

- Paths (current `VmHost`/`VM_HOST_` occurrence counts in parentheses):
  `include/ziran_host.h:17-47,68-70,75,103` (15); `cmd/zir/zir_host.c` (13);
  `cmd/zir/zir_host_exec.c` (21); `cmd/zir/zir_host_exec.h` (5);
  `cmd/zir/zir_zx_wasm.c` (3); `cmd/zir/zir_zx_wasm.h` (2);
  `cmd/zir/zir_zx_wasm_web.c` (1). Repo total outside plans: 244 (was 403 at
  the first audit). Tests: `tests/host_capability_test.c` (18),
  `tests/zx_wasm.sh` (15), `tests/process_host_test.c` (14),
  `tests/host_array_test.c` (12), `tests/zx_web_lifecycle_host.c` (11),
  `tests/record_host_test.c` (11), `tests/bundle_component_test.c` (11),
  `tests/host_slice_test.c` (10), `tests/vm_value_payloads_test.c` (9),
  `tests/record_fields_by_name_test.c` (9), `tests/host_bindings_test.c` (2),
  `tests/bundle_step_limit_test.c` (1), `tests/zx_web_suspending_host.c` (1),
  `tests/host_capability.sh` (1). Bench: `bench/zx_compare_host.c` (1),
  `bench/zx_compare_apk/engine.c` (1), `bench/host_capabilities.c` (1).
- Evidence: `std/bundle_host.zi` already exposes `HostValue`/`HostCall`; the C
  names are the VM-era spellings of the same ABI surface.
- Replacement: rename `VmHostValue*`/`VmHostCall`/`VM_HOST_*` to the `Host`
  spellings in one coordinated change. No alias layer, no compatibility
  macros.
- Risk: Kryon and external embedders compile against these names; the sibling
  repositories need the same rename in the same coordinated window (compare
  the tool-names plan coordination). The self-hosting plan's older note about
  keeping C ABI names indefinitely is superseded by the owner's remove-legacy
  rule.
- Owning lane: native runtime.
- Proof: `make check CHECK_FILTER=host_capability`,
  `make check CHECK_FILTER=zx_wasm`, full `make check`.

### R2 — backend `Auto` semantics — preserve/design

- Paths: `std/bundle_host.zi:49-52`.
- Evidence: `Auto` is a whole-instance platform choice (browsers get Wasm,
  native hosts get the machine backend, everything else the portable engine);
  `Engine` and `Native` are strict. It is never a per-program fallback.
- Replacement: none. Do not 'fix' Auto into per-program fallback and do not
  delete it; strict modes stay strict.

### R3 — rename docs/site VM-era names — pending (after R1)

- Paths: `site/api.html:38-47`, `site/guide/zx.html:92,175`,
  `docs/ZX.md:104,198`.
- Evidence: prose and code samples still present `VmHostCall`/`VmHostValue`.
- Replacement: update to the R1 spellings when R1 lands. Historical
  `CHANGELOG.md` mentions stay untouched.
- Proof: no behavioral check; verify with `rg VmHost` returning only
  changelog/history matches, and `make check CHECK_FILTER=zx_wasm` for the
  underlying behavior.

### R4 — concurrent host work — recheck-concurrent

- Evidence: dirty `cmd/zir/zir_host_native.c` (+676), `zir_host_exec.c`
  (+331), `tests/host_capability*`, `tests/native_runtime_headers.sh`,
  `scripts/embed_headers.sh`, `cmd/zi2c/*` are another task's in-flight work
  (native host execution, capability tests, header embedding).
- Action: none now. Re-anchor R1's rename onto the landed result.

## Self-hosting lane (remaining hand-written compiler C)

### S1 — `cmd/zir/zir_runtime.c/.h` deleted — done (2026-11-15)

- Evidence: the working tree shows deleted `cmd/zir/zir_runtime.c` (274 lines)
  and `cmd/zir/zir_runtime.h` (25 lines). Update the self-hosting ledger's
  'What is deleted' to record it as done when that work lands.

### S2 — runtime-emitter replacement — done (2026-11-15: `cmd/compiler_runtime_emit.zi`)

- Evidence: untracked `cmd/compiler_runtime_emit.zi` (378 lines) plus
  generated `bootstrap/compiler_runtime_emit/` (c_string, file, file_linux,
  vec, compiler_runtime_emit) replace the deleted runtime for bootstrap
  generation. The old `cmd/zir/zir_runtime.c` removal is its deletion half.
- Action: none from this plan; do not touch the untracked work.

### S3 — remaining `cmd/zir` compiler C — pending

- Current working-tree sizes (refresh after the concurrent host work lands):
  `zir.c` 2,517 lines; `zir_host.c` 1,081; `zir_host_exec.c` 1,789;
  `zir_host_native.c` 694; plus `zir_asm.c`, `zir_zx_wasm.c`, SIMD headers and
  the rest of the directory. These are the self-hosting plan's S11–S17
  deletion targets.
- Replacement: generated Ziran modules per the self-hosting plan's replacement
  map; no new deletions are invented here.
- Proof: per its plan sessions; final gate `make check-bootstrap` and full
  `make check`.

### S4 — generator/tool C backends — pending

- Current sizes: `cmd/zi2c/zir_c_lower.c` 2,886, `zir_c_cache.c` 828,
  `zir_c_build.c` 519, `main.c` 477; `cmd/zi2go/zir_go_lower.c` 2,754,
  `zir_go_asm.c` 645, `zir_go_amd64.c` 595, `main.c` 221; `cmd/zi2zx/
  zir_asm_lower.c` 5,402, `main.c` 528, `zx_exec_run.c` 180.
- Evidence: these are the self-hosting plan's S1–S10 targets; the folders were
  already renamed (tool-names cutover), the C bodies remain.
- Replacement: per that plan. No alias or compatibility runners.
- Proof: per its plan sessions; byte-identical generated output before each
  deletion.

## Free lane (ordered, independent small batches)

### F1 — delete nine zero-reference spec tests — done (2026-11-15)

- Paths: `tests/spec/calendar_test.zi` (2,178), `directory_entry_test.zi`
  (2,019), `file_copy_new_test.zi` (3,075), `file_link_test.zi` (616),
  `file_new_test.zi` (1,096), `sequence_header_test.zi` (3,530),
  `time_windows_test.zi` (1,688), `timer_linux_test.zi` (498),
  `vm_packed_arrays_test.zi` (4,761).
- Evidence: searching those nine names across `cmd tests scripts bench
  Makefile` returns only `bench/results/**` historical logs. The check runner
  only executes `tests/*.sh`; nothing runs these files.
- Replacement: none needed; delete.
- Risk: minimal; coverage they duplicated lives in the retained host/file
  suites.
- Owning lane: free.
- Proof: full `make check` after deletion (the suite shrinks; no consumer
  breaks).

### F2 — delete `tests/vec_capacity.zi` — done (2026-11-15)

- Path: `tests/vec_capacity.zi` (7,496 bytes).
- Evidence: searching `vec_capacity` across `cmd tests scripts bench Makefile`
  excluding the file itself returns nothing live.
- Replacement: none needed.
- Proof: full `make check`.

### F3 — delete `scripts/build-android-host.sh` — done (2026-11-15)

- Path: `scripts/build-android-host.sh` (925 bytes).
- Evidence: repo-wide search for `build-android-host` outside plans returns
  nothing; Android host builds are driven by the bench Android scripts and
  Makefile rules instead.
- Replacement: none needed.
- Proof: full `make check` (the script is not referenced).

### F4 — delete `bench/zx_metadata.py` and `bench/zx_metadata.c` — done (2026-11-15)

- Paths: `bench/zx_metadata.py` (9,156), `bench/zx_metadata.c` (2,199).
- Evidence: the only references to either are inside the pair itself
  (`bench/zx_metadata.py:71,121`); no Makefile, test, or doc consumer.
- Replacement: none needed.
- Proof: `rg zx_metadata` returns zero after deletion; full `make check`
  unaffected.

### F5 — remove thin wrappers after direct runner support — done (2026-11-15)

- Paths: `tests/artifact_consumer.sh` (166 bytes) around
  `tests/artifact_consumer_cases.zi`; `scripts/test_native_package.sh` (250
  bytes) around `scripts/test_native_package.zi`.
- Evidence: `tests/check_plan.zi:104-120` (`DiscoverChecks`) lists only
  `tests/*.sh` plus three hard-coded non-shell cases; both wrappers are bare
  `ziran run` launchers.
- Replacement: extend `tests/check_plan.zi` to run `.zi` cases directly
  (tests and `scripts/`), then delete both wrappers.
- Risk: check discovery changes; keep `CHECK_FILTER` semantics.
- Owning lane: free.
- Proof: `make check CHECK_FILTER=artifact_consumer` before/after byte-identical
  results; the package runner is verified by invoking the runner the way the
  old wrapper did until discovery covers it.

### F6 — GPU CPU-fallback wording and behavior become explicit Gap — done (2026-11-15; unreachable GPU helper removal needs the concurrently dirty Makefile)

- Paths/ranges: `cmd/package_guide.zi:53`; `cmd/package_features.zi:252-254`
  (c/go/zx rows 'CPU fallback only'), `:280` (`execution.gpu-fallback` id),
  `:285` (rejection text); `cmd/package_capabilities.zi:319`
  ('gpu_execution':'cpu_fallback'); `docs/CAPABILITIES.md:38`;
  `docs/FEATURES.json` (`execution.gpu-fallback` entry);
  `ASSEMBLY/feature-matrix.md:62`;
  `tests/compiler_inventory_cases.zi:51` (asserts
  `gpu_execution == 'cpu_fallback'`).
- Evidence: owner rule — no fallbacks; a GPU selection must produce a clear
  named gap, never silently run on CPU.
- Replacement: GPU capability rows report an explicit Gap/rejection on every
  target; the inventory test asserts the rejection; ordinary documented CPU
  parallel execution stays (it is a feature, not a GPU fallback).
- Risk: consumer expectations flip; update the inventory assertions in the
  same batch.
- Owning lane: free (package/docs) with native-compiler sign-off that no GPU
  dispatch exists to preserve.
- Proof: `make check CHECK_FILTER=capabilities` and
  `make check CHECK_FILTER=features`.

### F7 — stale 'portable VM' wording in retained files — done (2026-11-15)

- Paths/ranges: `tests/examples.sh:4,36`; `tests/parallel_results.sh:5`;
  `tests/parallel_slices.sh:7`; `tests/json_scan.sh:39`;
  `tests/bit_builtins.sh:2,73,78-79`; `cmd/package_explain.zi:81,94,100,101,103`.
  The follow-up sweep also renames the active spelling in benchmark guides,
  benchmark Python receipts, retained tests, compiler comments,
  `package_features`, `docs/FEATURES.json`, and the generated site target copy.
- Evidence: wording describes the retired VM tier in messages/comments of
  retained active tests and diagnostics.
- Replacement: update wording to the engine/native tier names. Wording only;
  no behavior change. Historical changelogs, `bench/results/`, and other
  plans are preserved untouched.
- Owning lane: free.
- Proof: `make check CHECK_FILTER=examples`,
  `make check CHECK_FILTER=json_scan`, `make check CHECK_FILTER=bit_builtins`.

### F8 — stale `native-zx.mk` references — done (2026-11-15)

- Paths/ranges: `ASSEMBLY/08-assembler-objects-and-linker.md:13`;
  `bench/compiler_build.zi:54`.
- Evidence: `native-zx.mk` is already deleted from the working tree (done by
  the concurrent work); these two references now dangle. `assembly.mk` is the
  live build-selection file.
- Replacement: the doc line points to `Makefile`/`assembly.mk`; drop the name
  from `bench/compiler_build.zi`'s packed-paths list.
- Owning lane: free.
- Proof: full `make check` (build wiring unaffected) plus re-running the bench
  packager manually; `rg native-zx` returns only historical records.

### F9 — regenerate bootstrap after engine wording fix — done (2026-11-15)

- Done: every `bootstrap/compiler_*` module, `compiler_runtime_emit`,
  `compiler_backend_cache`, `compiler_backend_process` and
  `compiler_native_names` included, matches its generator
  (`make check-bootstrap`); `rg zi2asm bootstrap` finds nothing.


- Path: `bootstrap/compiler_bundle/compiler_bundle.c:1792` (stale
  'zi2asm --emit zx' string).
- Evidence: source `cmd/compiler_bundle.zi:595` already says 'zi2zx'; the
  generated tree predates it. Never hand-edit generated C.
- Replacement: run `update-bootstrap` after the concurrent bootstrap work
  lands so the regeneration includes `compiler_runtime_emit`.
- Proof: `make check CHECK_FILTER=tool_names` and `rg zi2asm bootstrap`
  returning only the retained rejection-guard strings.

### F10 — remove ignored `--opt` on ZX input — done (2026-11-15)

- Done: `zi2zx` rejects `--opt` on ZX input like any unknown flag, and its
  usage no longer lists it. `tests/assembly_cases.zi` and
  `scripts/inspect_assembly.py` stopped passing it. ziran-vscode routes
  `zi2zx`/`zx2native` and passes `--opt` only with source input, so it is
  unaffected.


- Paths/ranges: `cmd/zi2zx/main.c:329-331` (the inspect path silently accepts
  and ignores `--opt`), usage strings `:24-25`.
- Evidence: saved ZX is canonical; the flag cannot change verified payloads;
  it exists only for the former inspect/re-emit contract.
- Replacement: reject unknown `--opt` like any other flag once external
  callers stop passing it. The ziran-vscode stage graph still references the
  old tools (`targets.cjs:615-626` routes `zi2asm`); finish that repo's
  tool-names Session 7 first, then flip this to a hard error.
- Owning lane: free (this repo) with ziran-vscode coordination.
- Proof: `make check CHECK_FILTER=tool_names`,
  `make check CHECK_FILTER=zx_executable`.

### F11 — bench Python/C vs Ziran duplicates — audited preserve

- Paths: `bench/bundle_assets.c/.py`, `bundle_instances.c/.py`,
  `host_capabilities.c/.py`, `json_text.py` (vs `json_document.zi`),
  `vm_native.py`/`vm_execution.py` (vs `zx_execution.zi`),
  `zx_compare.py` (vs `zx_compare.zi`), `reachable_types.c/.py`,
  `type_exports.c/.py`, `native_cache_names.c/.py`, plus the ignored
  `bench/__pycache__/` (1.3 MB).
- Evidence: the paired Python drivers in `bench/README.md` are active
  orchestration, receipts and oracle/report writers; their C files are the
  measured kernels they build and invoke. `json_document.zi` measures one direct
  parser, not `json_text.py`'s six-workload C/Go/ZX/Wasm matrix.
  `zx_execution.zi` measures the published portable-bundle matrix, not
  `vm_execution.py`'s synthetic workload suite or `vm_native.py`'s generated
  C/Go comparison. `zx_compare.py` drives the checked `zx_compare.zi` fixture
  and its backend comparison.
- Replacement: none; these are complementary measurement surfaces, not duplicate
  implementations of one benchmark. Do not delete the live drivers or kernels.
- Risk: none from deletion; future consolidation must first replace an entire
  documented measurement and receipt contract in Ziran, not just one side.
- Owning lane: free.
- Proof: per file, a caller search plus `make check` unchanged; bench scripts
  re-run manually for the affected benchmark.

## Preserve list (false positives and contracts)

- `cmd/zir/zir_asm.c:558-566`: the local array named `legacy` is the x86
  register-name table — live code, not legacy. Optionally rename the local
  later; never delete.
- Engine old-ZX rejection (E1), `tests/tool_names.sh:13-17` rejection guards
  (`zi2asm`/`zi2wasm` must not exist), and all other current-format rejection
  tests.
- Portable engine, Wasm tier (`zx2wasm`), generated C/Go tools, AOT ELF
  output, `ziran bundle`, M0 receipts — per the native-zx-players plan.
- `CHANGELOG.md`, `bench/results/**`, dated reports and other plans:
  historical records.
- `Makefile:221,243` `zi2asm` clean/install rules and the `tests/tool_names.sh`
  guards: they remove and reject stale installs; keep until no install could
  ever contain them.
- Check-runner-discovered `tests/*.sh`: live suite unless named as a deletion
  above.

## Existing-plan status sections

### `plans/2026-11-14-native-zx-players.md` — Final deletion ledger

- `cmd/compiler_lowered_x64.zi`, its dedicated shell/spec test, and all
  `LirX64*` consumers — pending (N1–N3; the dedicated test moved to
  `tests/spec/compiler_lowered_x64_test.zi` with runner
  `tests/compiler_lowered_x64.sh`).
- Fixed-base duplicate relocation logic in the ELF writer — in progress by the
  concurrent task (N5 recheck).
- Engine-specific public instance dispatch in `zir_host.c` — recheck (R4
  concurrent dirty file).
- Duplicated interface/host-image conversion in engine, native, and Wasm
  backends — recheck (N5/R4 concurrent).
- Kryon duplicate desktop binding/instantiation loop — out of this repository;
  belongs to the Kryon lane; not audited here.
- x86-only assumptions in target-neutral lowering, allocation, images, loader,
  cache, and host trampolines — recheck (concurrent machine files).
- Android runtime artifacts reused across ABIs — pending; keep as a native
  runtime follow-up once the concurrent host work lands.
- Preserve rows (portable engine, Wasm selection, M0 receipts, generated C/Go
  tools, AOT ELF) — intact.

### `plans/2026-11-14-self-hosting.md` — What is deleted / replacement map

- Old folders `cmd/zir-asm`, `cmd/zir-zx`, `cmd/zir-wasm`, `cmd/zir-ir`,
  `cmd/zir-c`, `cmd/zir-go` — done (verified absent).
- `cmd/zir/zir_runtime.c` (274 lines) and `zir_runtime.h` (25 lines) — done in
  the working tree, uncommitted (S1).
- Runtime replacement — in progress: `cmd/compiler_runtime_emit.zi` +
  `bootstrap/compiler_runtime_emit/` (S2).
- Everything else in the replacement map (checker, emitters, parser, IR,
  frontend, host API, tool drivers) — pending (S3/S4). The plan's citation of
  `zi2c` dirty files and line totals must be refreshed after the concurrent
  work lands.

### `plans/2026-11-14-tool-names.md` — What is deleted

- Old folders — done.
- `zi2asm`, phony `assembly`, stale installed `zi2asm`/`zi2wasm` — done;
  `Makefile:221,243` keep cleaning stale installs and
  `tests/tool_names.sh:13-17` keeps rejecting them.
- `--input zx` — done (no matches in `cmd/zx2native/main.c` or
  `cmd/zi2zx/main.c`).
- `zi2zx bundle` — done (rejected at `cmd/zi2zx/main.c:523`).
- `--machine` — pending (N2; `cmd/zx2native/main.c:780`).
- The monolithic old driver's multi-output branches — pending (N2).
- VS Code `zi2asmStep`/discovery — pending in the ziran-vscode repository
  (`targets.cjs:615-626` still routes `zi2asm`); Kryon obsolete source globs —
  done (no `cmd/zir*` globs remain in Kryon `mk/` files).
- Active docs/current-site old-tool text — mostly done; remaining: F8 and F9.

## Implementation order for free-lane batches

1. F1, F2, F3, F4 — pure zero-reference deletions; each independent.
2. F8 — dangling name fixes (doc + bench list).
3. F7 — wording-only sweep.
4. F5 — runner support, then wrapper deletions.
5. F6 — GPU Gap semantics plus inventory assertions in one batch.
6. F11 — audit first, then delete proven files in small batches.
7. F10 — after the ziran-vscode cutover lands.
8. F9 — after the concurrent bootstrap work lands; run `update-bootstrap`.

Native-compiler order: N3 → N2 → N1 → N4, each with its proof filter, then
full `make check`. Native runtime order: R1 with its cross-repo coordination,
then R3 docs, after R4's concurrent work lands.

## Standing instruction

Do not edit the concurrently owned native-runtime and machine-compiler files
listed in the concurrency note until their owning task lands and the affected
items (N5, R4, S1/S2, F9) are re-checked against the new tree.
