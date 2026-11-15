# Input trust tiers for the compiler and standard library (2026-11-15)

Planning document. No code changed for this plan. Owner words: 'can we use this
to improve the complire', asked against a four-tier input-trust table
(absolute trust / local control / semi-trusted local / fully untrusted).
Runtime assignments follow the standing split: Claude/Codex for compiler and
verifier work, Co-Z for docs and tests.

Everything below is checked against the current tree at HEAD `40212fc9`
('Cache compiled native code on disk across runs') plus live-lane work in
progress. Laws quoted from `docs/LAWS.md`; trust rules from
`ASSEMBLY/milestones.md` and `ASSEMBLY/gap-policy.md`.

## Status (2026-11-15)

Steps 0-2 are implemented. What landed differs from the sketches below in
three places, each for a reason:

- Step 0's counter is `MachineCheckCount` in
  `cmd/compiler_machine_pipeline.zi`, printed per fixture by
  `tests/proven_pointer_cases.zi`, not a `zi2zx` flag: no new CLI surface, and
  the test asserts the numbers.
- The facts live in their own module, `cmd/compiler_lowered_proven.zi`, which
  joins the engine module set so native lowering and `zx2wasm` link one copy.
  `compiler_opt.zi` is unchanged.
- The measured 17/17 does not all go: 4 of the pointer fixture's 17 resolves
  are elided. The other 8 load/store sites are in `Observe`, which reaches the
  record through a parameter (any caller's pointer, possibly stale), and the 5
  platform-call string arguments carry a run-time count. Both keep their
  checks. The fixture prints `resolves=13 direct-frame=4`.
- Following the owner's "make sure we oly have guardarils wher ew enede
  tehm", native code also drops index checks the intervals prove (not only
  Wasm), and Wasm drops proven view checks. On the bench kernels Sieve and
  Hash have no pointer or index checks left; Records' 15 proven
  `extract`/`insert` indices are left to the native Records lane, which owns
  those encoders.

## Verdicts

| Tier | Verdict | Why, tied to the laws |
| --- | --- | --- |
| 0 absolute trust | Adapt (adopt the goal, adapt the mechanism) | Ziran already has a proof system: `#law`/`#proof` certificates with `proved` status and structural evidence (`docs/LAWS.md`). The optimizer lane proves constant-element safety (`SafeConstantElement` in `cmd/compiler_opt.zi`) and the Wasm lane proves index ranges (`range_plan`/`range_mark` setting `fn->proven` in `cmd/zx2wasm/zx_wasm_lower.c`). What is missing is one shared, verifier-checked proven-safe fact from LIR that every backend consumes. Today the native checked-pointer path emits `PointerResolve` unconditionally: the measurement below found 17 of 17 check sequences guarding accesses whose slot, version, extent and writability are compile-time constants. LAWS.md says a proved obligation needs no runtime re-check; `ASSEMBLY/milestones.md` says internal stages rely on the verifier instead of duplicating checks, so the fact must live in machine IR where the verifier can enforce it, never as a silent lowering shortcut. |
| 1 local control | Adapt (validation only, never weaker checks) | argv and env are attacker-controlled in practice (CI, scripts, other apps). LAWS.md keeps checked pointers and no-fallback semantics; nothing in Tier 1 may weaken them. What changes is validation typing: today `EnvironmentText` borrows an unbounded C string and `EnvironmentNumber` silently substitutes a fallback (`std/environment.zi`). Plan `Option`/`Result`-returning typed parsers so a wrong value is a named, handled outcome instead of a silent default, while every pointer stays checked. |
| 2 semi-trusted local | Adopt | LAWS.md already states the rule for saved artifacts: loaders retype and recheck certificates; saved status, type annotations, and evidence cannot authorize a proof. A self-built, content-hash-cached `.zx` and the M3 step-7 native disk cache are exactly this tier: the file can be edited on disk, so structural validation plus a digest is mandatory, and the cache may skip only work the hash proves is redundant (recompilation), never a check. The disk cache is now implemented (`cmd/compiler_native_cache.zi`, wired in `cmd/zir/zir_host_native.c`, HEAD commit) with format-tagged keys, payload digest, bounded size and corrupt/stale refusal; the remaining work is durability tests plus writing the tier rule into LAWS.md. |
| 3 fully untrusted | Adopt and extend | `ASSEMBLY/milestones.md` already mandates hostile-input hardening at trust boundaries (ZX container and instruction decoders, loaders, host results). `cmd/compiler_exec_link.zi` implements it: on load it recomputes program/assets/content digests, compares them (`SameDigest`), decodes with `Serial.LirRead`, and re-verifies with `Verify.LirVerify`. Extend the same rule to std input APIs: `std/binary.zi` currently reads a short or misaligned field as zero, which is a silent fallback, not a named diagnostic. Add length caps and named failure results to file, HTTP and host-byte APIs. |

## Measured baseline (local test, one build-queue run)

Command (single build-queue invocation, outputs under the ignored
`build/trust-tiers-measure/`):

```sh
build/bin/zi2zx --root tests/spec --entry zx_lowered_pointers:main --target zx --opt -o build/trust-tiers-measure/pointers.zx tests/spec/zx_lowered_pointers.zi
build/bin/zx2native --target x86_64 --platform linux --emit elf -o build/trust-tiers-measure/pointers.elf build/trust-tiers-measure/pointers.zx
objdump -D -b binary -m i386:x86-64 build/trust-tiers-measure/pointers.elf
build/bin/zi2zx --root tests/spec --entry zx_lowered_pointers:main --target zx --emit lir -o build/trust-tiers-measure/pointers.lir tests/spec/zx_lowered_pointers.zi
```

Results on `tests/spec/zx_lowered_pointers.zi` (a typical pointer fixture:
one frame record, three string globals, a print host call):

- 12.5 KB native ELF, 17 `shr 0x2c` sites = 17 emitted `PointerResolve`
  check sequences (plus 1 `shl 0x2c` token re-encode and one unrelated
  `xorps` byte coincidence).
- LIR: 8 loads + 4 stores + 5 host-call pointer arguments = exactly the 17
  resolve sites; 5 `element.address` all with constant indices into fixed
  arrays, 7 `field.address` with static offsets, 1 `allocate` with a static
  record layout, 0 `bytes.equal`, 10 host calls.
- Classification: 17/17 check sequences guard compile-time-provable
  accesses. Frame-local slots have static extents and a provable live
  range; the string globals are constant tokens (slot, version 0, extent and
  read-only flag all static); every dynamic index is a constant 0 within a
  fixed array. Tier-0 elision would remove every one of these roughly
  10-instruction sequences from this example without changing behavior.

Caveats: one small fixture, x86-64 only, whole-file disassembly; it is a
check-site count, not a benchmark. Step 0 makes this a repeatable in-tree
number.

## Soundness policy (applies to every step)

1. A proven fact may remove a check only when it covers everything the check
   enforced: slot validity, version match, liveness, extent (offset + width),
   and writability for stores. Anything unknown keeps the check.
2. The fact is recorded in machine IR and enforced by
   `cmd/compiler_machine_verify.zi` (today that file has no Load/Store rule at
   all; the resolve-before-use invariant lives only in lowering). A removed
   check is therefore verifier-visible, not silent, matching the LAWS.md rule
   that internal stages rely on the verifier.
3. Every elided class needs a hostile counter-test: the same code with a
   non-constant index, a resized block, or a stale alias must still trap.
4. Optimizer elisions stay bounded and budgeted like the existing passes
   (`Spend`, `LirLimits`, `VerifyPass`), and keep `zx_lowered_parity`,
   `machine_elf`, `x86_encoder`, `aarch64_encoder`, `regalloc` and
   `native_load` green, per the stage rules in
   `plans/2026-11-15-native-checked-pointers.md`.

## Ordered implementation steps

### Step 0 - Make the Tier-0 win a repeatable number

- Behavior: a machine-stats emit that counts `PointerResolve` sites per
  program and classifies each as constant-token (global), static-frame, or
  dynamic. Sketch: `zi2zx --emit machine-stats` prints
  `resolves=17 constant-token=10 static-frame=7 dynamic=0` for the pointer
  fixture.
- Files: `cmd/zi2zx/main.c` (flag), `cmd/compiler_machine_pipeline.zi` (walk
  before encoding), reusing the Step-1 classification facts once they exist
  (until then classify from LIR shape, as this measurement did).
- Backend impact: none; statistics only.
- Ziran test: none needed for the flag itself; the Step-1 test asserts on the
  counter.
- Done: the command prints the three counts for the pointer fixture and
  matches the manual measurement above.
- Size: S. Runtime: Claude/Codex (pipeline plumbing), Co-Z (docs).

### Step 1 - One shared LIR proven-safe fact, checked by the machine verifier

- Behavior: a LIR-level pass computes, per value, which checked-pointer
  obligations are statically decided: constant-token globals (slot, version,
  static extent, writability from the read-only flag), frame-arena slots with
  static extents whose live range is the frame, and indices provably inside a
  fixed array (constant index, or interval ranges as the Wasm lane already
  computes). Sketch:

```zi
Table :: [4]s32 = .{10, 20, 30, 40}
Pick :: () -> s32 { return Table[2] }  // constant index, fixed array
```

  The load behind `Table[2]` carries the proven fact; `Table[i]` with a
  runtime `i` keeps its full resolve-and-trap path.
- Files: new pass in `cmd/compiler_opt.zi` (extends the existing
  `SafeConstantElement` fact and the constant-folding infra; port the
  interval machinery from `cmd/zx2wasm/zx_wasm_lower.c` `range_plan`), fact
  fields in `cmd/compiler_machine_ir.zi` and `cmd/compiler_machine_lower.zi`
  (`LowerResolve` and the `LowerMemory`, `LowerBytesEqual` and
  platform-service sites near lines 615/687/742), verifier rule in
  `cmd/compiler_machine_verify.zi`: every Load/Store/BytesEqual address
  operand must be a resolve result or carry the proven fact.
- Backend impact: native gains check elision; the machine verifier gains a
  real safety invariant; C, Go and engine paths are unchanged in this step
  (their checks are semantic-level and already law-covered).
- Ziran test: new `tests/proven_pointer_cases.zi` registered in
  `tests/check_plan.zi`: the pointer fixture asserts the Step-0 counter
  reaches the elided set with zero wrong elisions, plus hostile variants
  (runtime index, freed or resized block, aliasing store) that must still
  trap natively.
- Done: counter shows the elided sites; hostile variants still trap;
  `zx_lowered_parity`, `machine_elf`, `x86_encoder`, `aarch64_encoder`,
  `regalloc`, `native_load` and the full `make check` pass.
- Size: M/L. Runtime: Claude/Codex.
- Sequencing: `cmd/compiler_machine_lower.zi` is the integration/M3 active
  file. Land the fact + verifier first (machine IR + verify files), and take
  the lowering edit only on top of the M3 checked-pointer stages.

### Step 2 - Wasm consumes the same fact instead of its private copy

- Behavior: `zx_wasm_lower.c` keeps `checked_index` but reads the shared LIR
  fact; its local `fn->proven` from `range_mark` becomes a consumer of the
  shared pass, optionally refined by its own Wasm-specific ranges.
- Files: `cmd/zx2wasm/zx_wasm_lower.c` (`range_plan`, `range_mark`,
  `checked_index` near line 2558).
- Backend impact: Wasm bounds elision becomes consistent with native; elided
  sites shrink in size and skip the compare-and-trap.
- Ziran test: extend the parity and Wasm cases with the same constant/hostile
  pairs as Step 1 (`zx_lowered_parity_cases.zi` fixtures).
- Done: identical elision decisions on the shared fixture for native and
  Wasm; suite green.
- Size: S/M. Runtime: Claude/Codex.
- Sequencing: `zx_wasm_lower.c` is the optimizer lane's file; do it right
  after Step 1 lands the fact definition, in coordination with that lane.

### Step 3 - Tier 1 typed environment and argv validation

- Behavior: validation-typed accessors beside the existing ones. Sketch:

```zi
count, ok := EnvironmentRange("JOBS", 1, 64)  // ok=false names the failure
```

  Parsers return `Option`/`Result`-style values with explicit length caps; no
  silent fallback spelling survives in new APIs; memory safety unchanged (all
  strings stay checked views).
- Files: `std/environment.zi`, `std/environment_linux.zi` (bounded copy of
  the C value before returning), the argv accessor surface wherever arguments
  are read today.
- Backend impact: none; pure std additions on all targets.
- Ziran test: `tests/std_environment_cases.zi` (unset, empty, oversized,
  non-numeric, in-range, out-of-range).
- Done: new cases green in `make check`; old functions still work with
  documented Tier-1 semantics.
- Size: S/M. Runtime: Co-Z.

### Step 4 - Tier 2 trust rules in LAWS.md plus cache durability tests

- Behavior: extend `docs/LAWS.md` Saved-artifacts-and-trust with the tier
  table: downloaded `.zx` = full verification (digest + decode +
  `LirVerify`, as `cmd/compiler_exec_link.zi` already does); self-built
  hash-cached `.zx` and the native disk cache = structural validation +
  digest, skipping only hash-proven-redundant work. Confirm the implemented
  cache (`cmd/compiler_native_cache.zi`, `cmd/zir/zir_host_native.c` near
  lines 558-660) refuses corrupt, truncated, stale and oversized entries and
  survives restarts.
- Files: `docs/LAWS.md`; `tests/native_build_cache_cases.zi` and
  `tests/native_object_reuse_cases.zi` extensions (these are also the M3
  step-7 test files; see sequencing).
- Backend impact: none; documentation plus tests.
- Ziran test: the two cache case files gain cold/warm/corrupt/stale fixture
  cases (no new shell or Python).
- Done: LAWS.md states the tiers; the durability cases pass.
- Size: S. Runtime: Co-Z (docs + tests), Claude/Codex if any cache code fix
  falls out.
- Sequencing: M3 step 7 owns `compiler_native_cache.zi` and those test
  files; coordinate with the integration lane. HEAD already carries the
  cache commit, so the docs half can start anytime.

### Step 5 - Tier 3 hardening for std input APIs

- Behavior: length caps and named diagnostics at every byte-input boundary.
  Sketch:

```zi
text, result := ReadEntireFileCapped("notes.txt", 1048576)
// result names too-large, not-found, io-error; no silent truncation
```

  `std/binary.zi` stops reading short fields as zero: readers get explicit
  `Option`/`Result` outcomes (the Evolution section of
  `docs/LANGUAGE_DIRECTION.md` permits the break; version the behavior in the
  diagnostic, not a compat shim). `net_http` response bodies and
  `http_get_linux.zi` curl capture gain maximum-size parameters and named
  failures; the URL is validated before it becomes a curl argv entry.
- Files: `std/binary.zi`, `std/file.zi`, `std/file_linux.zi`,
  `std/net_http.zi`, `std/http_get_linux.zi`, in-tree callers updated.
- Backend impact: none; std behavior on all targets.
- Ziran test: `tests/std_input_cases.zi` with fixture files and host captures
  truncated at every boundary byte, invalid UTF-8, over-cap sizes.
- Done: no std input API silently substitutes data; every failure is a named
  result; cases green.
- Size: M. Runtime: Co-Z, with Claude/Codex review of the `binary.zi`
  semantic change (decoder callers must be proven updated).

### Step 6 - Named-diagnostic inventory at artifact and host boundaries

- Behavior: one test inventory proving each untrusted boundary names its
  failure: ZX container (bad digest, truncated, wrong revision), loader,
  native disk cache, host results, and the Step-5 std inputs. No new
  enforcement code expected; this pins the existing ASSEMBLY rule so a future
  change cannot silently weaken it.
- Files: `tests/artifact_consumer_cases.zi` extension plus a new
  `tests/trust_boundary_cases.zi` registered in `tests/check_plan.zi`.
- Backend impact: none.
- Ziran test: the inventory itself.
- Done: inventory green; `ziran explain` lists the codes it covers.
- Size: M. Runtime: Co-Z (tests + docs), Claude/Codex for any named gap the
  inventory exposes.

## Parallel safety with live lanes

| Step | Collides with | Safe plan |
| --- | --- | --- |
| 0 | optimizer lane owns `cmd/zx2wasm/zx_wasm_lower.c`; M3 owns machine files | zi2zx main.c + machine pipeline walk only; safe immediately |
| 1 | optimizer (`compiler_opt.zi`), M3 (`compiler_machine_lower.zi`, native cache), S1 port (`compiler_zir_lower.zi` untouched; keep it that way) | land fact + verifier (`compiler_machine_ir.zi`, `compiler_machine_verify.zi`) first; take `compiler_opt.zi` and `compiler_machine_lower.zi` edits only in coordination with those lanes |
| 2 | optimizer lane | after Step 1's fact exists; coordinate on `zx_wasm_lower.c` |
| 3 | Jai lanes (`std/temp`, `std/context`, `cmd/zi2c`, parser/checker module parameters) | different files (`std/environment*`); fully parallel |
| 4 | M3 step 7 (cache + its test files); Kryon pay-per-import untouched | docs anytime; cache tests after the M3 stage lands |
| 5 | Jai std lanes | `std/binary`, `std/file`, `std/net_http` are in no live lane; parallel |
| 6 | none | parallel, after Step 5 for its std section |

## Open questions

- Engine-side check elision (portable interpreter in `cmd/compiler_exec.zi`)
  is unmeasured and out of scope here; it can reuse the same LIR fact later.
  The first candidate is the engine's per-access pointer element-type check:
  M3 step 2 closed it for native code as discharged by the load-time
  verifier (`tests/verified_loading_cases.zi`), so the engine re-checks a
  fact verified programs already guarantee. Keep it until this step decides.
- AArch64 gets the same resolve elision only after the native lane's stage 5
  lands its encoding (per the checked-pointers plan).
- Whether read-only global bytes should also skip per-access writability
  re-checks in the engine, matching the native read-only flag, is a semantic
  decision tied to the M3 element-type leftover.
