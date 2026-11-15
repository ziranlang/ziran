# M3 native cutover: verification and remaining work (2026-11-15)

Read-only verification of the working tree on Omega at HEAD `4ceb7f40`
("Run ZX natively by default and integrate the finished assembly lanes", local
only; the integration lane is verifying it before push). Spec:
`ASSEMBLY/milestones.md` M0-M6 and "Current position". Evidence is taken from
existing `build/check-logs/`, `bench/results/` and `bench/zx_compare.md`; the
one fresh focused run is noted where it appears. The `build/check-logs/` receipts cited below were read at 13:23-14:05 on 2026-11-15 and were wiped minutes later by the optimizer lane's `make clean` (14:14); their quoted results are preserved here, and the next `make check` regenerates them.

## (a) M1 verdict: done

Milestone scope (milestones.md:87) and exit evidence, item by item:

| M1 item | Verdict | Evidence |
| --- | --- | --- |
| Typed host capability calls (`s64->s64` first, then scalar/string/record) | Done | Scalar ABI bind `cmd/zir/zir_host_exec.c:183` (`LirExecBindHost`), typed ABI `:1687` (`LirExecBindHostTyped`) with mutable-view registration `:1690-1692`; value model `include/ziran_host.h:12-42`. Tests: `tests/host_bindings.sh` ("128 host bindings passed in source and saved bundles", `build/check-logs/host_bindings.sh.log`), `tests/host_capability.sh` (`build/check-logs/host_capability.sh.log`, includes the missing-capability negative), `tests/portable_host_records.sh`, `tests/host_slices.sh`, `tests/host_arrays.sh` (logs present from the 2026-11-15 13:2x run). |
| In-bundle `--bind` providers | Done | CLI `cmd/zi2zx/main.c:24,112-114` (`--bind module:function=module:function`, `--bind-host module`); a provider already present in the checked program lowers to a direct portable call (`ASSEMBLY/README.md:121-123`). Test: `tests/capabilities.sh` ("schema, evidence, source/saved targets and CLI rejection", `build/check-logs/capabilities.sh.log`), `tests/bundle_runtime_provider.sh`. |
| Immutable strings (literal, copy, `.count`, checked index) | Done | `string` = `{data: *u8, count: s64}`; literals are deduplicated read-only globals; copies, `.count`/`.data`, checked indexing, slicing and equality lower for source, saved IR and the portable engine (`ASSEMBLY/README.md:126-134`). Test: `tests/string_storage.sh` (passing, `build/check-logs/string_storage.sh.log`), plus parity cases `zx_lowered_text`, `assembly_string_index` in `tests/zx_lowered_parity_cases.zi:137,152`. |
| Runtime global initializers | Done | Dependency-ordered once-only initializers per portable instance; a failed initializer is sticky and entry is not started (`ASSEMBLY/README.md:150-152`, `ASSEMBLY/milestones.md:164-166`). Tests: `tests/runtime_globals.sh` (`build/check-logs/runtime_globals.sh.log`), `tests/portable_globals.sh` (passing, empty log). |
| Capability list recomputed in the revision-28 container | Done | `cmd/compiler_exec_link.zi:452-453` "Recomputes the reachable host capabilities instead of trusting the producer"; counted during reachability `:480`, written into the header `:588` -> `cmd/compiler_bundle.zi:263`, validated on read `cmd/compiler_exec_link.zi:718-724`. Test: `tests/capabilities.sh`. |
| C embedding API (open, list capabilities, bind, run with fuel) | Done | `include/ziran_host.h:63-77` (`BundleOpen`, `BundleCapabilityCount/AbiAt`), `:83-91` (`BundleInstantiate`, `BundleInstantiateLimited` fuel budget), `:119-120` (`BundleInstanceLimitSteps`), `:104-113` (`BundleInstantiateWithOptions` Auto/strict policy). The comparison harness drives it (`bench/zx_compare_host.c`, `bench/zx_compare_component.c`). |
| Corpus-coverage report: every benchmark workload lowering | Done in substance | Both benchmark workloads lower and run oracle-correct on the new engine: `bench/zx_compare.md:250-270` and receipts `bench/results/zx-compare-alpha-physical-2026-11-12.json`, `-2026-11-13.json`, `-pointer-dispatch-2026-11-13.json`; step-03 records the M1 coverage list (`ASSEMBLY/03-language-lowering.md:32-35`). Note: no standalone coverage-report artifact exists; the receipts are the evidence. |
| Oracle agreement (source, saved IR, new ZX) | Done | Python oracle checks every timed batch (`bench/zx_compare.md:19-21`); the 2026-11-13 receipts' 90 samples were independently recomputed against the oracle (`bench/zx_compare.md:311-320`). Parity: C (source), interpreter (lowered ZX), bundle and wasm compared per case (`tests/zx_lowered_parity_cases.zi:38-64,93-104`). |
| Negative fixtures for every remaining Gap | Done for the documented set | Gap policy normative (`ASSEMBLY/gap-policy.md:15-20`); machine-path gaps fixtured (`tests/machine_ir_cases.zi:429-431,435-437`), no-fallback and byte-identical ELF fixtures (`tests/machine_elf.sh:129-142`), backend-policy no-engine-fallback fixtures (`tests/native_backend_policy_cases.zi:79-83`), expected trap `std_string_bad` (`tests/zx_lowered_parity_cases.zi:156`), feature-matrix Gap cells documented with rejecting tests (`ASSEMBLY/feature-matrix.md:47-71`). |
| Interim comparison 1 (new portable engine vs old VM, explicit selection) | Done | `bench/zx_compare.md:250-270`, receipt `bench/results/zx-compare-alpha-physical-2026-11-12.json` (physical Alpha, virt none, 90 oracle-verified samples); refined by the 2026-11-13 cohorts (`bench/zx_compare.md:272-342`, receipts `-2026-11-13.json` and `-pointer-dispatch-2026-11-13.json`). |

M1 verdict: **done**. The only cosmetic gap is the missing standalone
corpus-coverage artifact; the milestone's substance (every benchmark workload
lowering, with oracle agreement and interim receipts) is proven.

## (a) M2 verdict: implementation and audit done; one exit-test gap remains

Section 3 item 1 of `plans/2026-11-15-assembly-remaining.md:102-203` is a
completed audit. Re-checked against the tree, its single remaining gap is
confirmed:

- `tests/zx_lowered_parity_cases.zi:44-48`: the expected-trap case
  (`item.traps`) asserts C-vs-interpreter traps and `return`s **before** the
  native build at `:94-104`, so `std_string_bad:main` never executes through
  `zx2native`; only 70/71 parity fixtures run on the machine path.
- `:162-163` then prints "71 programs ... as native ELFs", which overstates
  native coverage.

Exact harness fix: in `tests/zx_lowered_parity_cases.zi`, replace the early
return at `:44-48` so a trapping case additionally builds the ELF through
`zx2native` (sharing the `:94-104` native build), executes it, and requires the
native status to be the trap status 125 (or add an equivalent dedicated native
expected-trap fixture), and make the `:162-163` count count only fixtures that
actually executed through `zx2native`. Roughly 10-20 lines, test-only; verify
with `build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u
DBUS_SESSION_BUS_ADDRESS -u SESSION_MANAGER HARMONY_DESKTOP_RECOVERY=0 make
check CHECK_FILTER=zx_lowered_parity`. After that, M2's implementation and
exit are complete. The documentation cleanup listed in the audit
(`ASSEMBLY/milestones.md:44-47,155-162`, `ASSEMBLY/machine-ir.md`,
`ASSEMBLY/performance-report.md:295`) stays non-blocking; other lanes own those
files right now.

Live-tree note (2026-11-15 14:05): `zx_lowered_parity` is currently red at
case-14 (`zx_lowered_features:main`, "lowered IR rejected: problem=19") from
another lane's uncommitted optimizer work (`build/check-logs/zx_lowered_parity.sh.log`).
That is in-flight lane state, not an M2/M3 regression of the committed tree.

## (b) M3 item status (Native ZX on Linux x86-64, first cutover row)

Scope and exit items (milestones.md:89):

| M3 item | Verdict | Evidence |
| --- | --- | --- |
| In-memory W^X loading | Done | `cmd/zir/zir_native_memory.c:3-4` (contract: publish code as RX, constants R, no RWX), `:58-60` (mmap RW), `:80` (RX/R publish); each instance copies the image into RW pages then flips protections (`cmd/zir/zir_host_native.c:5`). |
| Relocation of the same encoder output | Done | The loader compiles through the machine pipeline and takes the image view (`cmd/zir/zir_host_native.c:418-421`); applies every relocation kind to its own copy: GlobalAddress, ConstantAddress, FuelCounter, StackLimit, PointerRuntime, RuntimeTrap, RuntimeResize, HostCapability, PlatformService (`cmd/zir/zir_host_native.c:1228-1271`). |
| Host trampolines | Done | One veneer per host-capability relocation, ABI-shuffling stub calling `NativeHostInvoke(instance, function, &frame)` with typed conversion and copy-back (`cmd/zir/zir_host_native.c:287-326`); platform-service relocations validated at preflight `:474-480`. |
| Stack and fuel checks | Done | Encoder emits the stack-limit check with a `StackLimit` relocation (`cmd/compiler_x86_64.zi:2031`) and the image records `stack_limit_offset`/`pointer_offset` (`:2200-2235`); loader patches fuel and stack words (`cmd/zir/zir_host_native.c:1253-1261`). Execution evidence: `build/check-logs/native_load.sh.log` (fuel exhaustion "stopped after 1000/500 steps"; stack "nested calls filled the stack (problem=frame-limit)"). |
| Content/CPU/backend-keyed cache | Done (in process) | Key = 32-byte bundle content digest + `cpuid`-derived CPU key; the backend is this build's, part of every key (`cmd/zir/zir_host_native.c:130-132`, `:389-397`, lookup `:505-511`, insert `:533-538`, idle eviction `:499-518`). The Session-9 persistent disk cache (`cmd/compiler_native_cache.zi` + atomic adapter) does **not** exist yet; see step 7. |
| `ziran run app.zx` native by default; unsupported fails preflight, no engine fallback | Done | Auto resolves to `BUNDLE_EXECUTION_NATIVE_X86_64` on linux-x86_64 (`cmd/zir/zir_host.c:243-244`, applied `:353`); a native gap is a named preflight diagnostic (`assembly.gap.machine-*`, `cmd/zir/zir_host_native.c:434-441`) with no engine retry (`include/ziran_host.h:104-108`). Tests: `tests/native_load.sh` (strict/engine/auto, shared code, fuel, stack, globals, host strings, checked pointers - `build/check-logs/native_load.sh.log`), `tests/native_backend_policy_cases.zi:79-83` (Auto must not switch to Engine after a native preflight failure; `build/check-logs/native_backend_policy.log`). |
| Full suite on the new default incl. no-fallback negatives | Partially evidenced | Committed tree on Omega: `make check` 264/266 with the two named non-native failures (`zstd_go` needs Go 1.25; `site_targets` waits on the bench lane's stale site snapshot) (`ASSEMBLY/milestones.md:138-146`). Native rows green in `build/check-logs/`: `native_load.sh.log`, `native_backend_policy.log`, `machine_elf.sh.log`, `assembly.sh.log`, `x86_encoder.log`, `machine_ir.sh.log`, `regalloc.sh.log`. Not yet done: one fresh fully-green full-suite run on the new default (blocked today by the optimizer lane's red parity row and the stale site snapshot); see step 3. |
| Installed tool verified | Missing | `ASSEMBLY/10-inspection-performance-and-cutover.md:61` is unchecked; no install-verification receipt exists in `build/check-logs/`. See step 4. |
| Interim comparison 2 (new native ZX vs M0 TinyCC receipts + AOT and C controls, Alpha physical host, step-10 rules) | Missing | No comparison-2 section in `bench/zx_compare.md` (only comparison 1 at `:250` and the portable cohorts at `:272-342`); no new-native receipt under `bench/results/` (the `zx-compare-*` receipts are portable-engine cohorts); the harness lacks the native cohorts - no `--native-cold`/`--native-warm`/`--aot`/`--m0-receipts` flags in `bench/zx_compare.zi` or `bench/zx_compare.py`. See steps 5-6. |
| Known leftover: native pointer access does not check element type | Open | The engine also checks a pointer's element type on access; native checks liveness, version, writability and bounds only (`plans/2026-11-15-native-checked-pointers.md:141-143`, "Recorded for a later stage"). See step 2. |

## Ordered M3 remaining work

Each step: behavior, files, function, test, done. Size S = under half a day,
M = 1-2 days, L = multi-session. Owner marks follow the standing rule:
Claude/Codex for correctness-critical runtime/compiler work, Co-Z for
benchmarks, docs and harness work.

### Step 1 - Truthful native expected-trap parity (gate for every suite claim)

- Owner: Co-Z (test harness; no compiler changes). Size: S.
- Behavior: the expected-trap parity case is also built by `zx2native`, the ELF
  runs, and the native status must be the trap status; the printed count must
  name only fixtures that really executed through `zx2native`.
- Files: `tests/zx_lowered_parity_cases.zi` (`ParityRun` `:44-48`, native build
  `:94-104`, count `:162-163`). Function: `ParityRun`, `main`.
- Test: `build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u
  DBUS_SESSION_BUS_ADDRESS -u SESSION_MANAGER HARMONY_DESKTOP_RECOVERY=0 make
  check CHECK_FILTER=zx_lowered_parity` (rebase over the optimizer lane's
  in-flight red row first, or coordinate with it).
- Done: the focused check passes with the trap case native; the printed line is
  truthful. This also closes M2's last exit item.

### Step 2 - Pointer element-type check in native (engine-parity leftover) — closed 2026-11-15 by the verifier

Decision: no runtime check. Native code compiles only programs that the
reader verified, and the verifier makes a load, store or derived address
through a pointer of the wrong element type unrepresentable. `Convert` and
`Bitcast` take only numeric (and bool/integer) types, so LIR has no pointer
casts. `FieldAddress`, `ElementAddress`, views, `Load`, `Store` and platform
pointers are typed against the pointee. There are no unions, a stale token
fails its slot version, and host-returned views get slots typed by their
declared result. A source `cast(*T)` is rejected before any ZX is written.
`tests/verified_loading_cases.zi` checks this. A source-built bundle whose
load is retyped to another integer width, then encoded and framed without
verification as any producer could, is refused by the reader, by
`zi2zx run` and by `zx2native`, with a verifier diagnostic and before any
native code exists. The same test shows that a source pointer cast never
reaches ZX. The engine's own runtime element-type check stays for now; it is
a candidate for trust-tier elision (`plans/2026-11-15-trust-tiers.md`,
step 1). The original plan follows for the record.


- Owner: Claude/Codex. Size: L.
- Behavior: native loads/stores/BytesEqual through a pointer token verify the
  slot entry's element type, like the engine, so a wrong-type access traps with
  the named pointer diagnostic instead of reinterpreting bytes.
- Files: `cmd/compiler_machine_ir.zi` (resolve operand carries the checked
  type), `cmd/compiler_machine_lower.zi` (lowering passes the static element
  type), `cmd/compiler_machine_verify_allocated.zi` (verifier requires it),
  `cmd/compiler_x86_64.zi` (encoded check in the resolve sequence),
  `cmd/compiler_machine_image.zi` (slot entry layout gains the type field),
  `cmd/zir/zir_host_native.c` (loader/runtime slot table and ELF image agree).
- Function: the `PointerResolve` lowering/encoding path and the slot table
  layout in the loader.
- Test: a wrong-element-type access negative fixture in
  `tests/machine_ir_cases.zi` plus `tests/native_load.sh` coverage; then
  `tests/regalloc.sh`, `tests/machine_elf.sh`, `tests/native_load.sh`,
  `tests/zx_lowered_parity.sh` all green.
- Done: wrong-type native access traps natively with the engine-comparable
  diagnostic; all four suites pass with the new fixture.

### Step 3 - Full-suite green receipt on the new default

- Owner: Claude/Codex (correctness gate; runs after steps 1-2 land).
  Size: M (mostly waiting on the other lanes' files to land).
- Behavior: one fresh `make check` and `make check-bootstrap` run green on the
  committed tree with native as the linux-x86_64 default, including the
  no-fallback negative fixtures (`tests/native_backend_policy_cases.zi:79-83`,
  `tests/machine_elf.sh:129-142`) and step 1's native trap case.
- Files: none to add unless a failure needs a fix in `cmd/`; coordinate the
  `site_targets` snapshot with the bench lane and `zstd_go` with the
  Go-1.25 environment item.
- Test: `build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u
  DBUS_SESSION_BUS_ADDRESS -u SESSION_MANAGER HARMONY_DESKTOP_RECOVERY=0 make
  check CHECK_JOBS=2` and `... make check-bootstrap`.
- Done: fresh full-suite receipt in `build/check-logs/` with zero failures (or
  only the two named environment/snapshot items, re-measured), recorded in
  `ASSEMBLY/milestones.md` "Current position" by its owning lane.

### Step 4 - Installed tool verified

- Owner: Co-Z. Size: S/M. Independent of steps 1-3 once the default is in.
- Behavior: install from the current working tree at the stable path
  (`make install-user`), then verify the installed `ziran`: compile, inspect
  and run representative new-format programs, and prove a `.zx` run executes
  natively (backend query/trap evidence), per the step-10 item
  (`ASSEMBLY/10-inspection-performance-and-cutover.md:61`). Never terminate the
  owner's running applications; `install-user` already refuses foreign
  launches (`Makefile:288`).
- Files: no source change; receipt under `build/check-logs/`.
- Test: the installed binary runs the representative programs; a native-only
  trap program exits 125 with the native diagnostic; `BundleInstanceBackend`
  reports native via a small embedding probe or the run trace.
- Done: a dated receipt names the installed binary identity and the native
  execution evidence; the ASSEMBLY/10 box can be ticked by its owner.

### Step 5 - Interim comparison 2 harness (cohorts and receipts)

- Owner: Co-Z (benchmark harness + docs). Size: M/L.
- Behavior: extend the comparison harness so one synchronized cohort records,
  with explicit selection: portable engine, cold native ZX (first compile in
  the process), warm native ZX (cache hit), `zx2native` AOT ELF control, C
  control, and the retained M0 old-VM/TinyCC receipts consumed read-only
  (never reconstructed or relabeled). Store raw samples, engine/tool/ZX
  identity hashes, CPU governor/features, warmup, sample counts, medians and
  dispersion under `bench/results/`.
- Files: `bench/zx_compare.zi`, `bench/zx_compare_host.c`,
  `bench/zx_compare.py` (oracle for the new cohorts), `bench/README.md`,
  `bench/zx_compare.md`. Optionally `bench/zx_engine.zi`.
- Function: new native cold/warm and AOT cohorts behind explicit flags (the
  Session-9 flag names in `plans/2026-11-14-native-zx-players.md:570-577` are
  the starting point; confirm or rename against the landed harness in the same
  change).
- Test: harness dry run on Omega proving all cohorts produce oracle-correct
  results and a complete receipt schema (cold/warm reported separately).
- Done: a complete receipt can be produced end to end on this machine with
  every cohort populated and oracle-verified.

### Step 6 - Interim comparison 2 measurement on Alpha

- Owner: Co-Z. Size: M. Depends on steps 3 and 5 (a green default and a working
  harness); step 4 should land first so the receipt references a verified tool.
- Behavior: run the comparison on Alpha's physical host per the step-10
  measurement rules (`ASSEMBLY/milestones.md:22-24`): physical host, managed
  namespace, virtualization rejected, raw samples, symmetric 1.5xIQR, machine
  and source identities hashed; compare explicitly selected new native ZX
  against the M0 TinyCC native ZX receipts plus AOT and C controls.
- Files: new receipt `bench/results/zx-compare-alpha-physical-native-<date>.json`
  and the "Interim comparison 2" section in `bench/zx_compare.md`.
- Test: all samples oracle-verified; old TinyCC samples byte-identical to the
  M0 receipts' recorded values.
- Done: the receipt and the `bench/zx_compare.md` section exist, and
  `ASSEMBLY/milestones.md` "Current position" can cite them. That closes M3's
  last exit item together with steps 3-4.

### Step 7 - Persistent native-code cache (Session-9 follow-up, not an M3 gate)

- Owner: Claude/Codex. Size: M/L. The M3 scope item is already satisfied by the
  in-process content/CPU/backend-keyed cache (`cmd/zir/zir_host_native.c:130-132`);
  this step is the durable form planned by Session 9
  (`plans/2026-11-14-native-zx-players.md:556-567`), worth doing before or right
  after comparison 2 so warm cohorts survive process boundaries.
- Behavior: `cmd/compiler_native_cache.zi` with a content-addressed key over ZX
  digest, target/ABI, detected CPU features, machine/runtime ABI revisions,
  optimizer policy, fuel semantics and veneer revision; an atomic adapter in
  `cmd/zir/zir_host_native.c` (user cache path, temp file, fsync/rename, digest
  verification, bounded size, corrupt/stale refusal, reference counting).
- Test: cold/warm equality, interrupted writers, corrupt entries, masked CPU
  features, revision changes, concurrent compile/instantiate/unload
  (extending `tests/native_build_cache_cases.zi` and
  `tests/native_object_reuse_cases.zi` for the loader cache; the existing
  `tests/native_cache.sh` covers the build cache, not this one).
- Done: the loader cache survives process restarts with digest verification and
  the new focused tests pass.

## Order and ownership summary

1 (Co-Z, S) -> 2 (Claude/Codex, L) -> 3 (Claude/Codex, M); 4 (Co-Z, S/M) and
5 (Co-Z, M/L) in parallel with 1-2 where files do not overlap; 6 (Co-Z, M)
after 3+5; 7 (Claude/Codex, M/L) optional relative to the M3 gate, before or
right after 6. Step 1 also closes M2. The element-type leftover (step 2) is the
only known engine/native semantic divergence; everything else M3-scoped is
implemented and awaiting its exit evidence.
