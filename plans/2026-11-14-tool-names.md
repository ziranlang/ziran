# Tool names follow the input and output formats

Date: 2026-11-14

Owner request, 2026-11-14: “zir-asm should probabyl be renabel to just something
like zi2zx ... we wolud nede a clean naming to do the excats foramt we want”.

This implements the planning part of item 10 in [the native optimization and
cutover plan](2026-11-13-native-optimizations.md#next-native-players-phones-and-a-faster-pipeline-owner-2026-11-14).
That shared file is linked, not edited. No implementation, build, commit, push,
or installation was performed while writing this plan.

## Target design and command contract

Names describe one format boundary. There is no compatibility binary, alias, or
wrapper for `zi2asm` after cutover.

| Tool | Input | Output/action |
| --- | --- | --- |
| `zi2zir` | `.zi` | checked `.zir` |
| `zi2c` | source or `.zir` | generated C |
| `zi2go` | source or `.zir` | generated Go |
| `zi2zx` | source, `.zir`, or `.zx` for inspection/run | verified optimized ZX by default; LIR/optimization reports; portable-engine run |
| `zx2wasm` | exactly one verified `.zx` | one `.wasm` |
| `zx2native` | exactly one verified `.zx` | machine image, platform ELF, or map |

```text
zi2zx [ZX-BUILD-OPTIONS] -o FILE INPUT.zi|INPUT.zir ...
zi2zx --emit lir [-o FILE|-] INPUT.zi|INPUT.zir|INPUT.zx ...
zi2zx --opt --emit opt-report [-o FILE|-] INPUT.zi|INPUT.zir ...
zi2zx run [ENGINE-LIMITS] INPUT.zi|INPUT.zir|INPUT.zx ...
zx2wasm [-o FILE] INPUT.zx
zx2native --target x86_64|aarch64|armv7 [--platform linux|android]
          [--emit image|elf|map] [-o FILE] INPUT.zx
```

- `zi2zx` defaults to `--emit zx`; `zi2zx bundle` and `--input zx` are removed.
  `ziran bundle` remains the high-level project command.
- `zi2zx run` is the only engine spelling. Native/Wasm gaps never select it.
- `zx2wasm` has no target flag.
- `zx2native --target` is the requested CPU-only value. `--platform` carries
  Linux versus Android ABI/container policy; cross-Android calls must spell it.
- A parsed but unimplemented AArch64/ARMv7 row fails before creating output.
- Source-to-native is visibly composed:

  ```sh
  zi2zx --root src --entry app:main -o app.zx src/app.zi
  zx2native --target x86_64 --platform linux --emit elf -o app app.zx
  ```

### Exact command changes

| Old | New |
| --- | --- |
| `zi2asm ... --emit zx -o app.zx INPUT...` | `zi2zx ... -o app.zx INPUT...` |
| `zi2zx bundle ... -o app.zx INPUT...` | `zi2zx ... -o app.zx INPUT...` |
| `zi2asm ... --emit lir -o - INPUT...` | `zi2zx --emit lir ... -o - INPUT...` |
| `zi2asm ... --opt --emit opt-report` | `zi2zx --opt --emit opt-report ...` |
| `zi2asm --input zx --emit lir app.zx` | `zi2zx --emit lir app.zx` |
| `zi2asm --run LIMITS INPUT...` | `zi2zx run LIMITS INPUT...` |
| `zi2asm --input zx --emit wasm -o app.wasm app.zx` | `zx2wasm -o app.wasm app.zx` |
| `zi2asm SOURCE... --emit wasm` | `zi2zx ... -o app.zx SOURCE...` then `zx2wasm -o app.wasm app.zx` |
| `zi2asm --input zx --machine --target x86_64-linux --emit elf -o app app.zx` | `zx2native --target x86_64 --platform linux --emit elf -o app app.zx` |
| `zi2asm SOURCE... --emit elf` | `zi2zx ... -o app.zx SOURCE...` then `zx2native ... app.zx` |
| `zi2asm --input zx --emit map ... app.zx` | `zx2native --emit map ... app.zx` |
| `make assembly` | `make zx-tools` |

## Current state

- `cmd/zir-asm/main.c:27-40` advertises the current multiplexed `zi2asm` CLI;
  `main` at `:585-654` parses LIR/map/ZX/ELF/Wasm/report output, engine limits,
  and machine selection together.
- Native-only pieces are already identifiable: `assembly_target_supported`
  (`cmd/zir-asm/main.c:48`), `assembly_machine_source_gap` (`:123`), the
  pointer/aggregate checks (`:161-326`), and `assembly_map` (`:425`).
- Source/ZX inspection pieces are `assembly_read` (`:328`) and
  `assembly_opt_report` (`:407`).
- The other driver advertises `zi2zx bundle`/`run` at
  `cmd/zir-zx/main.c:17-31`; its reusable functions are `bundle_to` (`:31`),
  `open_source` (`:202`), `run_command` (`:223`), and `main` (`:265-278`).
- The Wasm boundary is already clean: `ZxWasmLowerProgram`
  (`cmd/zir-wasm/zx_wasm_lower.c:4573`) and `ZxWasmLowerExecutable` (`:4591`).
- The registry parses the three requested CPUs at
  `cmd/compiler_target.zi:170-176,198-203` and Linux/Android at
  `:178-185,206-213`; only Linux x86-64 passes
  `TargetMachineLoweringSupported` (`:165-168`).
- Native compilation enters `MachineNativeBuild` at
  `cmd/compiler_machine_pipeline.zi:105`, which currently owns ELF bytes, not
  a separately named raw image.
- `Makefile:193-216` builds/installs `zi2asm`; `:445-487` uses all six old
  folders; `:477-480` builds `zi2zx` from both `cmd/zir-zx` and `cmd/zir-asm`.
  `Makefile:160-174` links the Wasm lowerer from `cmd/zir-wasm` into libziran.
- `assembly.mk:1-47` generates the machine modules, builds `zi2asm`, and adds
  both old include paths.
- `cmd/package.zi:233,302` selects `zi2asm` for Wasm.
- VS Code has `zi2asmStep` at `ziran-vscode/targets.cjs:562-564`, discovery at
  `:572-574`, and LIR/report/ZX/downstream stages at `:591-601`.
- Kryon's `mk/zx-canvas.mk:14` and `mk/zx-raylib.mk:21` glob `cmd/zir*` and will
  miss `cmd/zi2*`/`cmd/zx2*`. Its project recipe
  (`mk/ziran-project.mk:366-373`) uses final-name `zi2zx` plus high-level
  `ziran bundle`. `tools/zx.zi` contains embedding APIs, not tool spellings.

## Complete reference ledger

Line numbers describe the 2026-11-14 checkout. Rerun the search immediately
before implementation and add later matches. This separates real update sites
from final names that only need review.

### Ziran build/source paths and executable spellings to change

- Build/source: `Makefile:160,162,193,194,207,216,311,445,468,473,477,486,487`;
  `assembly.mk:9,11,41,43,45,46,47`.
- CLI/library diagnostics: `cmd/zir-asm/main.c:29,30,31,53,64`;
  `cmd/zir-asm/zx_exec_run.h:3,4,21`; `cmd/zir-inspect/main.c:290`;
  `cmd/zir/zir_host_exec.c:481`; `cmd/zir/zir_zx_wasm.c:5,13`;
  `cmd/zir/zir_zx_wasm.h:11`; `cmd/zir/zir_zx_wasm_platform.h:7`;
  `cmd/compiler_bundle.zi:595`; `cmd/package.zi:233,302`;
  `cmd/package_explain.zi:81-87,95`.
- Moving include/file paths: `Makefile:160,162,174,477`;
  `assembly.mk:41,47`; `cmd/zir-asm/main.c:1-3`;
  `cmd/zir-asm/zir_asm_lower.c:1`; `cmd/zir-asm/zir_asm_lower.h:14`;
  `cmd/zir-asm/zx_exec_run.c:1-2`; `cmd/zir-asm/zx_exec_run.h:2`;
  `cmd/zir-wasm/zx_wasm_lower.c:2,18,23,2318`;
  `cmd/zir-wasm/zx_wasm_lower.h:9`; `cmd/zir-zx/main.c:1`;
  `cmd/zir/zir_host.c:12-13`; `cmd/zir/zir_zx_wasm.c:2,5,10,13-14`;
  `cmd/zir/zir_zx_wasm.h:11-12,49`; `cmd/zir/zir_zx_wasm_platform.h:5,7`;
  `cmd/zir/zir_zx_wasm_web.c:2,26-27`; `web/bundle.c:9`;
  `web/playground.c:6`; `scripts/build_playground.zi:90`;
  `bench/site_target_host.c:41`; `tests/zx_wasm.sh:268,333`.

### Ziran tests to change

- Commands: `tests/assembly_cases.zi:766`;
  `tests/assembly_compare_cases.zi:146`;
  `tests/assembly_local_addresses.sh:5,7,8,10`;
  `tests/assembly_local_addresses_cases.zi:43`; `tests/compile_targets.sh:68`;
  `tests/machine_elf.sh:5,62,64,65,91,102,104,110,114,115,130,139,197,200,233`;
  `tests/nested_vec_elements.sh:274`; `tests/print.sh:5,89,187,189,220,223`;
  `tests/wasm_value_cases.zi:31`;
  `tests/zx_executable.sh:4,6,15,18,21,26,30,37,44,53,63`;
  `tests/zx_lowered_parity_cases.zi:2,32,34,40,79,83,94`;
  `tests/zx_wasm.sh:4,50,51,52,56,93,143,144,145,238,239,264,298,546,569,597,777`.
- Folder moves: `tests/native_build_cache.sh:9,10,13,17`;
  `tests/native_build_parallel.sh:10,11,17,18,44,48`;
  `tests/native_build_workers_test.c:3`; `tests/native_cache_names_test.c:1`;
  `tests/native_cache_probe_test.c:1`.

### Ziran scripts and benchmarks to change

- Scripts: `scripts/inspect_assembly.py:178,179,191,219,235,251,266,283,297,402,403,404`;
  `scripts/site_targets.py:108,139,194,204`;
  `scripts/site_target_wasm.cjs:1`; `scripts/build_playground.zi:89-90`.
- Benchmarks: `bench/assembly_collect.zi:263`;
  `bench/codec_kernels.py:503,624,645,647,662,670,672`;
  `bench/json_text.py:95,117`; `bench/wasm_tier.py:5,62,113`;
  `bench/zx_compare.py:53,67`; `bench/zx_compare_apk/build.py:42,62`;
  `bench/zx_engine.zi:7,63,69,86`; `bench/zx_compare.md:24`.
- Folder moves: `bench/native_cache_names.c:3` and
  `bench/native_cache_names.py:46,48,50,57`.

### Ziran docs and site to change

- Current docs: `README.md:72,76`; `docs/ARCHITECTURE.md:61,63,64`;
  `docs/USAGE.md:347,356,374`; `docs/ZX.md:54,58-62,137,208,216`;
  `docs/research_notes/Ziran image and language roadmap/compiler-efficiency.md:18-19`.
- Assembly docs: `ASSEMBLY/04-portable-instruction-engine.md:3`;
  `ASSEMBLY/05-zx-format-and-linking.md:13`;
  `ASSEMBLY/06-optimization-and-native-backend.md:3,9,22`;
  `ASSEMBLY/07-architecture-and-platform-coverage.md:11,61`;
  `ASSEMBLY/08-assembler-objects-and-linker.md:11`;
  `ASSEMBLY/09-native-runtime-and-integration.md:30,46`;
  `ASSEMBLY/README.md:5,9,30,129,177,179,181,216,218,235,237-239,241,243,299-300`;
  `ASSEMBLY/execution-contract.md:3`; `ASSEMBLY/feature-matrix.md:25,81,89`;
  `ASSEMBLY/machine-ir.md:8,231,241,244`;
  `ASSEMBLY/milestones.md:57,97,107,121,150`;
  `ASSEMBLY/performance-report.md:99`; `ASSEMBLY/vscode-inspection.md:47,53`;
  `ASSEMBLY/portable-execution.md:156`;
  `ASSEMBLY/zx-executable-format.md:5,7,78,198`.
- Regenerate, do not hand-edit, current site outputs:
  `site/api.html:34`; `site/guide/architecture.html:70,72,73`;
  `site/guide/usage.html:308,316,331`;
  `site/guide/zx.html:50,52-56,122,184,192`; `site/status.html:6`;
  `site/target-abi.js:1`; `site/targets.html:35,93,95,97,99,101,103`.
- `CHANGELOG.md:5,9,15,19,25,67,71,73,79,124,148,610-612` gets a deliberate
  pass: current Unreleased text uses final names; older history is rephrased as
  the former standalone tool rather than presenting an obsolete command.
- `plans/2026-11-13-compile-time-targets.md:69-70` updates moved-folder links.
  The linked `plans/2026-11-13-native-optimizations.md` is excluded from edits.
- Companion implementation plans need a path-only coordination pass in the
  same cutover: `plans/2026-11-14-native-zx-players.md:86,255,273,386` and
  `plans/2026-11-14-self-hosting.md:290-314,323,335,376-406,496,510-537,651`.
  Preserve their dated current-state evidence and deletion history; update
  only operational paths/commands and add a note that the old path was the
  2026-11-14 source location.

### GitHub workflows

No workflow directly names an old tool/path. These indirect gates remain
generic and require no edit unless an implementation adds a binary list:

- `.github/workflows/check.yml:35-38,131-140`;
- `.github/workflows/site.yml:49-56`;
- `.github/workflows/proofs.yml:25-31`;
- `.github/workflows/toolchains.yml:71-81`.

Do not add an alias merely to make archived `build/` content pass.

### VS Code references

- Change `ziran-vscode/targets.cjs:63,562-564,572-574,591,593,595,601` and
  `ziran-vscode/tests/targets.test.mjs:108,138`.
- Review and retain final-name `zi2zx` in `ziran-vscode/run.cjs:283,285,859,860`,
  `tests/run.test.mjs:109,111-113`, and `CHANGELOG.md:9`.

The view becomes `zi2zir -> zi2zx (LIR/report/final.zx) -> zx2native
(map/ELF)` plus `zx2wasm`, with failures naming the actual executable.

### Kryon references

- Change dependency globs in `kryon/mk/zx-canvas.mk:14` and
  `kryon/mk/zx-raylib.mk:21` to include `cmd/zir/*.[ch]`,
  `cmd/zi2*/*.[ch]`, and `cmd/zx2*/*.[ch]` explicitly.
- Review and retain final names in `kryon/Makefile:25-27,33,59,62`;
  `kryon/mk/ziran-project.mk:164,171,339,341,343,345,366,369,392,401`;
  `mk/zx-canvas.mk:37`; `mk/zx-raylib.mk:46`;
  `cmake/KryonAndroid.cmake:64`; `scripts/android-build.sh:26,29,74`;
  `scripts/generate_android_sources.py:18,140`; and
  `templates/droid/droid/app/src/main/cpp/CMakeLists.txt:22,61`.
- Retain high-level `ziran bundle/run` at `mk/ziran-project.mk:370-373` and
  `Makefile:132-135`.
- `tools/zx.zi:395-417,424-481,499-659` is an audited no-change embedding
  surface. Relevant tests include
  `tests/android_renderer_codec_link_test.py:72,78,89,90,103`,
  `tests/android_source_generation_test.py:36,110`,
  `tests/gallery_modules_test.py:47`, and `tests/widget_matrix.sh:15,18`.

### Preserved evidence

Raw receipts under `bench/results/`, archived logs, and binary/zip content under
`site/generated/` record the commands that produced old evidence and are not
search/replaced. Current HTML is regenerated from source. Historical JSON keeps
its command field. The final old-name gate excludes those receipts,
`site/generated/`, `bootstrap/` generated C, and the linked 2026-11-13 plan—but
not maintained source, scripts, tests, docs, or current site pages.

## Target source layout and deletions

```text
cmd/zi2zir/main.c
cmd/zi2c/{main,zir_c_build,zir_c_cache,zir_c_lower,...}.{c,h}
cmd/zi2go/{main,zir_go_lower,zir_go_asm,zir_go_amd64}.{c,h}
cmd/zi2zx/{main,zir_asm_lower,zx_exec_run}.{c,h}
cmd/zx2wasm/{main,zx_wasm_lower}.{c,h}
cmd/zx2native/{main,zx_native_map}.{c,h}
```

The live files move; `cmd/zir-ir`, `cmd/zir-c`, `cmd/zir-go`, `cmd/zir-zx`,
`cmd/zir-asm`, and `cmd/zir-wasm` are then deleted with no forwarding headers,
wrappers, or symlinks.

## Coordination with the native-player and self-hosting plans

Use this naming cutover first, after the already queued encoder/Toolchains work
reaches its quiet point. It establishes the permanent paths before tens of
thousands of C lines are translated. Native-player Sessions 1-4 may land before
the rename if they do not overlap the moved drivers, but its player Session 5
and every self-hosting session use the final names after this cutover.

If self-hosting has already deleted one of the C inputs, move/import the Ziran
replacement into the final folder instead; never recreate C just to follow this
plan. The stable semantic mapping is:

| Pre-cutover owner | Permanent owner |
| --- | --- |
| `cmd/zir-ir` | `cmd/zi2zir` |
| `cmd/zir-c` | `cmd/zi2c` |
| `cmd/zir-go` | `cmd/zi2go` |
| source/ZX portions of `cmd/zir-asm` + `cmd/zir-zx` | `cmd/zi2zx` |
| Wasm portion of `cmd/zir-asm` + `cmd/zir-wasm` | `cmd/zx2wasm` |
| native/map portion of `cmd/zir-asm` | `cmd/zx2native` |

This ordering avoids two directory migrations and makes the self-hosted driver
names match their final executable names from their first generated bootstrap.

## Ordered implementation sessions

There are no intermediate commits. Each session runs its focused local proof;
the last session runs all repositories' full gates before the coordinated
commits.

### Session 1 — Add the test vocabulary and freeze the command contract

Add/change:

- Add `tests/tool_names.sh` with the inert helper functions `expect_missing`,
  `expect_usage`, and `expect_same_bytes`, plus comments spelling out the six
  final command grammars and the exact old forms that must disappear. Do not
  register it yet: registering end-to-end assertions before the binaries move
  would knowingly make the retained local gate red.
- Add no behavior assertions in this session. Session 5 fills the helpers with
  cases for all six final binaries, rejection of `zi2asm`, `--input zx`, and
  `zi2zx bundle`; source/saved-ZIR byte-identical ZX; source→ZX→Wasm and
  source→ZX→x86-64 parity; parsed-but-unimplemented ARM rows; and no native
  fallback to the engine, then registers the finished test in
  `tests/check_plan.zi`.

Delete: nothing. Redundant old-spelling assertions move only when the finished
test owns them in Session 5; no production source changes in this session.

Proof:

```sh
build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY sh -n tests/tool_names.sh
```

Risk: an unregistered helper can drift. Keep this session immediately adjacent
to Sessions 2–5 in the same uncommitted feature set, and make Session 5's
registered test the acceptance gate. When adding byte comparisons there,
compare final verified optimized ZX separately from human-readable LIR.

### Session 2 — Move the stable generators to format-named folders

Add/change:

- Move `cmd/zir-ir/main.c` to `cmd/zi2zir/main.c`, keeping `main`.
- Move `cmd/zir-c/` to `cmd/zi2c/` and `cmd/zir-go/` to `cmd/zi2go/` as units.
  Retain `zir_c_*`/`zir_go_*` APIs because they name backends, not tools.
- Update `Makefile:445-487`, native-cache tests/bench helpers, and every
  folder reference in the ledger.

Delete: old three directories and include paths.

Proof:

```sh
build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY make check CHECK_FILTER=native_build CHECK_JOBS=1
```

Risk: cached dependency files hiding a stale path. Remove only affected object
stamps, then prove a second no-op make does not rebuild the moved sources.

### Session 3 — Create `zx2wasm`

Add/change:

- Move `cmd/zir-wasm/zx_wasm_lower.{c,h}` to `cmd/zx2wasm/`.
- Add `cmd/zx2wasm/main.c` functions `usage`, `read_zx`, `replace_output`, and
  `main`; call `ZxWasmLowerExecutable` and atomically replace output on success.
- Update `ZxWasmLowerExecutable`, `ZxWasmLowerProgram`,
  `ZxWasmLowerProgramSuspendable`, `ZxWasmGlobalImage`, and
  `ZxWasmGlobalStore` include/build paths without changing embedding ABI.
- Update `Makefile:160-174`, `assembly.mk:43`, `cmd/zir/zir_zx_wasm*`, web and
  playground includes, and Wasm tests.
- Remove the Wasm mode/options from `cmd/zir-asm/main.c`.

Delete: `cmd/zir-wasm/` and the old driver's Wasm branch/include.

Proof:

```sh
build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY make check CHECK_FILTER=zx_wasm CHECK_JOBS=1
```

Risk: two lowerer copies with different engine symbol prefixes. Link the thin
CLI and embedding tier to the same library object/API.

### Session 4 — Create verified-ZX-only `zx2native`

Add/change:

- Add `cmd/zx2native/main.c`: `usage`, `parse_target`, `parse_platform`,
  `preflight`, `read_zx`, `write_image`, and `main`.
- Move `assembly_target_supported`, `assembly_machine_gap`, and native emit
  selection out of the old driver. Replace source-specific preflight with
  verified-ZX preflight; never reopen ZIR.
- Add `cmd/zx2native/zx_native_map.{c,h}`. Move
  `assembly_map_selected`, `assembly_select_origin`, and `assembly_map` into
  `ZxNativeMapWrite`.
- Call `MachineNativeBuild` only. Do not expose a switch to the direct
  `compiler_lowered_x64` encoder while native-plan item 2 removes it.
- Update `tests/machine_elf.sh` target/help/negative fixtures.

Delete: `--machine`, target/native emit/map branches, and direct source→native
behavior from `cmd/zir-asm/main.c`.

Proof:

```sh
build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY make check CHECK_FILTER=machine_elf CHECK_JOBS=1
```

Risk: `MachineNativeOwner` currently means ELF. Reject `--emit image` with a
named gap until the native-player work exposes a real relocatable image view.

### Session 5 — Merge ZX creation, inspection, and engine run into `zi2zx`

Add/change:

- Build `cmd/zi2zx/main.c` from `bundle_to`, `open_source`, `run_command`, and
  `main` in `cmd/zir-zx/main.c` plus source/LIR/report logic from the old driver.
- Rename `bundle_to` to `build_zx`; add `emit_lir`, `emit_opt_report`, and
  `open_verified_zx`; default to ZX output and use `run` as the only subcommand.
- Move `zir_asm_lower.{c,h}` and `zx_exec_run.{c,h}` to `cmd/zi2zx/`.
- Serialize and decode ZX at every downstream boundary; do not hand pre-ZX
  structures directly to engine/Wasm/native paths.
- Update `Makefile`, `assembly.mk`, `cmd/compiler_bundle.zi`,
  `cmd/zir-inspect/main.c`, and `cmd/zir/zir_host_exec.c` diagnostics.
- Replace phony `assembly` with `zx-tools`, depending on all three ZX tools.

Delete: `cmd/zir-asm/`, `cmd/zir-zx/`, `build/bin/zi2asm`, install entries,
`--input`, `--machine`, and `zi2zx bundle`. `install-user` removes stale
installed `zi2asm` and `zi2wasm` rather than preserving aliases.

Proofs (the runner's filter is substring-only, so run separately):

```sh
build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY make check CHECK_FILTER=tool_names CHECK_JOBS=1
build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY make check CHECK_FILTER=zx_executable CHECK_JOBS=1
build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY make check CHECK_FILTER=machine_elf CHECK_JOBS=1
build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY make check CHECK_FILTER=zx_wasm CHECK_JOBS=1
```

Risk: source and saved ZX accidentally taking different optimized programs.
The byte-identity fixture must feed both later tools from the saved bytes.

### Session 6 — Update all Ziran consumers, docs, and current site output

Add/change:

- Apply every command substitution in the test/script/benchmark ledger. Name
  variables for the real tool rather than a generic compiler.
- Change `cmd/package.zi` Wasm builds to write temporary ZX with `zi2zx`, then
  invoke `zx2wasm`, preserving atomic output and diagnostics.
- Change `scripts/site_targets.py:main`, `bench/json_text.py:main`, and
  `scripts/inspect_assembly.py:main` to compose/hash both relevant stages.
- Update current docs and ASSEMBLY pages, then regenerate `site/guide/*.html`.
- Regenerate current target pages on physical Alpha so command metadata uses
  final names; reuse samples for a naming-only change and preserve raw JSON.
- Audit the cited workflows and keep their generic `make`/tool-directory use.

Delete: old command variables/helpers and active site/doc text naming an old
tool. Do not delete historical receipts.

Proof on Omega:

```sh
build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY make check CHECK_FILTER=site_pages CHECK_JOBS=1
build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY make check CHECK_FILTER=site_targets CHECK_JOBS=1
build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY make check CHECK_FILTER=compile_targets CHECK_JOBS=1
```

Proof on Alpha's physical host:

```sh
build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY sh tests/site_targets.sh build/bin/ziran
```

Risk: target-page regeneration replacing measurements. If measurement is
actually required, run only on physical Alpha and retain raw samples/exclusions.

### Session 7 — Update the VS Code stage graph

Add/change:

- In `ziran-vscode/targets.cjs`, replace `zi2asmStep` with
  `toolStep(tool, label, stage, args, cwd)`; resolve `zi2zx`, `zx2native`, and
  `zx2wasm` independently.
- Produce `final.zx` once; use `zi2zx` for LIR/report, `zx2native` for map/ELF,
  and `zx2wasm` for Wasm. Name the actual missing/failing tool and suggest
  `make zx-tools`.
- Update `tests/targets.test.mjs`. Retain `run.cjs`'s valid `zi2zx` version-
  mismatch path and its tests.

Delete: `zi2asmStep`, old discovery, and expectations for `build/bin/zi2asm`.

Proof:

```sh
build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY npm test --prefix /mnt/storage/Projects/ziranlang/ziran-vscode
```

Risk: partial installs. Return completed views plus a precise unavailable-stage
diagnostic; never substitute engine/C output.

### Session 8 — Update Kryon dependency tracking

Add/change:

- Change `ZX_CANVAS_COMPILER` and `ZX_RAYLIB_COMPILER` to include exact
  `cmd/zir`, `cmd/zi2*`, and `cmd/zx2*` source families.
- Audit the Makefile, `mk/ziran-project.mk`, Android scripts/templates, and
  `tools/zx.zi`; retain stable executable and embedding names.
- Add a focused make-fragment regression proving a source under each new
  folder invalidates the appropriate runtime target.

Delete: the `cmd/zir*/*.[ch]`-only globs; no player source.

Proof:

```sh
build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY make -C /mnt/storage/Projects/kryonlabs/kryon test TEST=zx_headless
```

Risk: depending on all `cmd/` sources makes every UI rebuild too broad. Keep
the shared frontend plus three exact format-tool families.

### Session 9 — Full gates and coordinated commits

Add/change:

- Rerun the three-repository inventory. Maintained source has no old executable
  or folder spelling outside the explicit preserved-evidence exclusions.
- Verify `build/bin` and a temporary install prefix contain all final tools and
  no `zi2asm`/`zi2wasm`.
- Run all retained suites without a live display.

Proofs:

```sh
build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY make check CHECK_JOBS=2
build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY make check-bootstrap
build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY npm test --prefix /mnt/storage/Projects/ziranlang/ziran-vscode
build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY make -C /mnt/storage/Projects/kryonlabs/kryon test
build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY git diff --check
```

Git cannot make one commit span three repositories. Treat this as one cutover,
with no intermediate commits and exactly one coherent commit per repository:

1. `ziranlang/ziran` — tools, folders, consumers, docs/site, tests;
2. `ziranlang/ziran-vscode` — extension stages/tests against local Ziran;
3. `kryonlabs/kryon` — dependency globs/regression against local Ziran.

Push only after every local gate passes; report pushed, CI not checked yet, and
do not wait for CI. No release/version bump is part of the rename.

Risk: active lowering/Wasm/site work overlaps moved files. Start only at the
requested quiet point after those edits are integrated, and preserve content.

## What is deleted

- Old folders `cmd/zir-asm`, `cmd/zir-zx`, `cmd/zir-wasm`, `cmd/zir-ir`,
  `cmd/zir-c`, and `cmd/zir-go` after moving live files.
- `zi2asm`, phony `assembly`, and stale installed `zi2asm`/`zi2wasm`.
- `--input zx`, `--machine`, and `zi2zx bundle`.
- The monolithic old driver's multi-output branches.
- VS Code's `zi2asmStep`/discovery and Kryon's obsolete source globs.
- Active docs/current site text that presents an old tool as current.

Raw receipts, ZX artifacts, the embedding Wasm tier, portable engine,
`ziran bundle`, and generated C/Go routes remain.

## Open questions for the owner

None are blocking. Two interpretations are explicit:

1. `--target` is CPU-only; `--platform` distinguishes Linux and Android.
2. One coordinated cutover means one commit in each independent repository;
   one Git commit cannot span all three.


