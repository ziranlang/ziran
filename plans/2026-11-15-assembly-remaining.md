# Assembly target: remaining work (2026-11-15)

This plan checks `ASSEMBLY/milestones.md` (M0–M6) and the 2026-11-14 plans
against the working tree on Omega at 09:45. It sits on master at `ad295286`
with about 195 uncommitted files from several lanes.

## 1. Integrate what is already done (M0) — done 2026-11-15

The integration gate committed the finished lanes as one commit on master
(each part depends on the others to build) and pushed it once; CI was not
checked at the time of writing. The commit holds five parts:

1. Allocator: an interval needed at the current instruction is never evicted,
   and the allocated-program verifier checks operand placement.
2. AArch64 machine encoder (`cmd/compiler_aarch64.zi`,
   `cmd/compiler_arm_bytes.zi`, `tests/aarch64_encoder_cases.zi`).
3. Native default with checked pointers: on linux-x86_64, `ziran run app.zx`,
   `zi2zx run`, the embedding API's Auto and the Kryon players run bundles
   natively with stray/freed/double-free/bounds/read-only traps. A native gap
   is a named preflight error with no engine fallback, and zx2native goes
   through the same preflight. The gate also fixed the x86-64 call sequence.
   The hidden result address was loaded into rax before the stack arguments
   and indirect target were placed, which corrupted Kryon's text-field
   clipping and crashed indirect calls returning records.
4. Self-hosting ports `compiler_backend_process`, `compiler_backend_cache`,
   `compiler_native_names` and `compiler_runtime_emit`, with bootstrap C and
   Makefile rules. Gate fixes went in the same commit: zir_c.toolchain
   diagnostics match the old C again; the parallel scheduler no longer
   deadlocks on a held compiler; and a fresh BUILD_DIR builds (pattern
   rules, generated-header prerequisites, per-object engine flags).
5. Legacy deletions (the 36 files: N1–N4, F1–F9, S1), F10 (`--opt` on ZX
   input is rejected), the ASSEMBLY doc edits and status refreshes.

Local results on Omega for the committed tree: `make check-bootstrap` passes
and `make check` passes 264/266. The two failures are outside this work:

- `zstd_go` is environment-only. A clean origin/master clone fails the same
  way, because the fixture needs Go 1.25 and Omega has 1.24.4 with
  `GOTOOLCHAIN=local`.
- `site_targets` fails only with the bench lane's uncommitted site snapshot
  in the tree. `scripts/site_targets.py` gained a native column after
  `site/generated/targets/matrix.json` was measured, so the harness digest
  and `targets.html` are stale. The committed tree keeps the committed
  snapshot, and the check passes there.

Kryon's zx-headless, zx-hosted and zx-services tests pass when built from a
deleted `build/zx-raylib/runtime`.

Left uncommitted for their own lanes: the optimizer (`cmd/compiler_opt.zi`,
its tests, `bench/wasm_tier.py`, the `cse` report lines and the range work in
`cmd/zx2wasm/zx_wasm_lower.c`); S1 (`cmd/compiler_zir_lower.zi`, comment
hunks in `cmd/zi2zx/zir_asm_lower.c`); and the bench lane's site snapshot.

## 2. In flight (14:40)

| Work | Lane | Done when |
| --- | --- | --- |
| Re-measure the site-target snapshot with the native column and commit it | glm-ziran-site-targets-native-column (Co-Z) | `site_targets.sh` passes in the live tree; pushed |
| Compile-time folding of std text operations, CSE and Wasm index-range elision | glm-ziran-lir-optimizer-speed (Co-Z) | text_demo zx near 40 ns; full suite green; pushed |
| S1 port of the ZIR→LIR lowering to Ziran (`cmd/compiler_zir_lower.zi`), deleting `cmd/zi2zx/zir_asm_lower.c` | glm-ziran-selfhost-ports-2 (Co-Z; moves to Claude for testing if slow) | bootstrap green; zx lowering tests pass; pushed |
| M3 steps 2, 3, 4 and 7 (element-type check, full-suite receipt, installed tool, disk cache), plus the AArch64 record-result call fix | claude-ziran-ziran-integration-gate | `plans/2026-11-15-m3-native-cutover.md` steps done; pushed |
| M3 steps 5–6 (comparison-2 harness and Alpha measurement) | Co-Z bench lane | comparison 2 published in `bench/zx_compare.md` |
| Jai lessons (`plans/2026-11-15-jai-lessons.md`) | std, C-unit and language lanes | each step's tests pass; pushed |
| Go 1.25 on Omega for `zstd_go` | environment | `zstd_go.sh` passes locally |

## 3. Not started or open, in milestone order

1. **M1 and M2 are done** (verified 2026-11-15; removed from this plan). The
   evidence, item by item, is in `plans/2026-11-15-m3-native-cutover.md`
   section (a). The last M2 exit gap, the expected-trap parity fixture, now
   runs through `zx2native` too (commit "Run the trapping parity fixture
   through native code, fix token docs").
2. **M3: native ZX on Linux x86-64.** Already in place: the W^X loader,
   relocation, host trampolines, stack/fuel checks, the in-process
   content/CPU/backend cache, and the native default with no fallback. The
   remaining steps are in `plans/2026-11-15-m3-native-cutover.md`, "Ordered M3
   remaining work":
   - the pointer element-type check
   - a full-suite green receipt
   - verification of the installed tool
   - the comparison-2 harness and its Alpha measurement
   - the persistent disk cache
3. **M4, the rest after AArch64.** ARMv7/AAPCS32 with 32-bit host images,
   native ZX loading on Android, and proof of the USB Moto row (player
   sessions 7–8). Direct wasm lowering from verified ZX already exists.
4. **M5: objects, linker and foreign libraries.** Relocatable ELF, COFF and
   Mach-O, archive reading, the external C ABI and foreign libraries,
   Windows/macOS ABIs and signing. Not started.
5. **M6: inspection and final cutover.** Unwind, debug and profiler mapping,
   then the owner's full before/after ZX comparison.
6. **Self-hosting S5–S18.** `cmd/zir` still holds 60 C files (about 64,000
   lines) for the checker, parser, emitters, host API and Go backend. These
   are long sequential sessions, and S18 is bootstrap convergence.
7. **Legacy leftovers.** F9 (check the bootstrap regeneration), F10 (remove
   the ignored `--opt` on ZX input, coordinated with VS Code), R1/R3 (VM-era
   names; no `ZiranVm`/`ziran_vm_` names remain in `include/` or `cmd/zir/`,
   so this probably only needs the docs and site), and N5 (duplicated
   relocation code).
8. **Known language gaps** that stay explicit Gap diagnostics: ownership of
   resizable collections, reentrant callbacks, and general native
   string/arbitrary-host instructions.

## 4. Order

1. M3 to its exit: element-type check, full-suite green receipt, installed
   tool, comparison 2. The disk cache can come right after comparison 2.
2. Self-hosting S1, then S5+ (the checker), which also unlocks the deferred
   Jai items (build-as-program, `#modify`, type_info).
3. M4 rows after M3 (ARMv7, Android, Moto), then M5 and M6.
