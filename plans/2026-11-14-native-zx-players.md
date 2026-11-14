# Native ZX players and phone support — implementation plan (2026-11-14)

Status: plan only. This file makes no source change and records no platform as
implemented. It continues [the 2026-11-13 native optimization plan](2026-11-13-native-optimizations.md#next-native-players-phones-and-a-faster-pipeline),
items 2–4, and the M2–M4 gates in
[`ASSEMBLY/milestones.md`](../ASSEMBLY/milestones.md#milestones).

## Scope and invariants

The final execution path is:

```text
revision-28 .zx bytes
  -> bounded container decode + LIR decode + LIR verification
  -> target preflight
  -> machine IR -> allocation -> one target encoder
  -> relocate into separate RX code / R constants / RW instance state
  -> target entry and typed-host trampolines
```

The `.zx` remains portable and contains no trusted machine code. Code pages are
never writable and executable at the same time. Every native instance owns its
mutable globals, fuel, traps, and host-call state; immutable lowered code may be
shared. An explicit native request never falls back. The ordinary player may
use `Auto` policy: it chooses native or the portable engine once for the whole
bundle before initializers run. A native preflight miss under `Auto` selects the
engine for that bundle; a compile, relocation, initializer, trap, or host-call
failure after native was selected is an error and never restarts in the engine.
This is the “engine fallback per bundle” required here without violating
[`ASSEMBLY/milestones.md:67-76`](../ASSEMBLY/milestones.md#L67) or replaying side
effects.

All future build, test, device, and benchmark commands below run through
`build-queue --`. They explicitly remove inherited desktop display variables.
GUI coverage uses a private Xvfb/Xephyr display owned by the test. Performance
is measured only on Alpha's physical host. Android device evidence is from the
USB Moto e6 on Omega, serial `ZE2223BQZT`; do not disable Wireless Debugging.

## Current state

### Verified input and embedding lifecycle

- `cmd/zir/zir_host_exec.c:421-502` (`ExecBundleOpen`) already performs
  `ZxExecRead`, obtains the verified `LirProgram`, builds the portable execution
  plan, copies assets, and parses the interface section. The interface parser at
  `cmd/zir/zir_host_exec.c:243-343` reads `node`, `host`, `abi`, `global`, and
  `entry` records. The kind is fixed as `ExecutableSectionInterface =
  0x80000003` at `cmd/compiler_bundle.zi:72` and documented at
  `ASSEMBLY/zx-executable-format.md:68-104`.
- The C bridge already translates host images and mutable slices through
  `exec_from_image` (`cmd/zir/zir_host_exec.c:678`), `exec_to_image`
  (`cmd/zir/zir_host_exec.c:860`), and `ExecHostsCall`
  (`cmd/zir/zir_host_exec.c:994`). `exec_host_call`
  (`cmd/zir/zir_host_exec.c:1102-1124`) is currently an engine callback; it is
  the semantic reference for native trampolines.
- `ExecInstanceOpen` (`cmd/zir/zir_host_exec.c:1162`) and `exec_run`
  (`cmd/zir/zir_host_exec.c:1129`) are engine-only. The public instance is a
  two-way union, engine or Wasm, at `cmd/zir/zir_host.c:216-222`; selection in
  `instantiate_bundle` at `cmd/zir/zir_host.c:321-330` chooses Wasm when a
  provider is installed and otherwise always opens the engine. Run/limit/close
  dispatch at `cmd/zir/zir_host.c:347-388` has no native arm.
- The public API promises verified-on-open, independent state, concurrency,
  copied binding names, cumulative limits, and terminal failure at
  `include/ziran_host.h:102-159`. `std/bundle_host.zi:70-94,229-261` is the
  Ziran-facing wrapper used by Kryon.

### Two x86-64 encoders and the unfinished machine path

- The maintained destination is machine IR. `MachineNativeBuild` at
  `cmd/compiler_machine_pipeline.zi:105-147` performs `MachineLower`,
  `RegAllocAllocate`, `MachineX64Encode`, and ELF wrapping. It still rejects all
  LIR initializers and constant globals at lines 108-111 and returns only an ELF
  byte slice through `MachineNativeView` at lines 98-100.
- `cmd/compiler_x86_64.zi:8-28` defines the allocated-machine contract and
  PC-relative relocation rule. `MachineX64Image` at lines 51-58 contains code,
  function/block/instruction ranges, relocations, and entry. `Process` at
  lines 781-856 verifies, measures, writes, and transactionally publishes it;
  `MachineX64Encode` is exported at lines 886-890.
- `cmd/compiler_machine_elf.zi:144-239` currently lays out and validates the
  image for a fixed-base ELF; `Write` at lines 260-333 resolves global, fuel,
  and trap-handler relocations. Those relocation calculations are reusable,
  but fixed `MachineElfBase` and process `_start` are not an in-memory ABI.
- A second direct LIR encoder remains in `cmd/compiler_lowered_x64.zi`.
  `X64Encode` (`2642-2707`) performs its own layout and code generation, while
  `LirX64Encode`, `LirX64Elf`, and `LirX64Ranges` are exported at
  `2710-2715,2761`. `cmd/zir-asm/main.c:434-526,860-878` still consumes its ELF
  layout/ranges and emitter. The architecture decision at
  `ASSEMBLY/milestones.md:40-44,85-90` is to delete this direct encoder after the
  M2 machine-path gate.
- `cmd/compiler_target.zi:11-14,81-132` describes x86-64, ARMv7, AArch64,
  Linux and Android ABIs, but `TargetMachineLoweringSupported`
  (`165-168`) accepts only Linux x86-64 SysV. Registry rows explicitly say no
  ARMv7/AArch64 selection or encoding at `327-340`. There are no
  `compiler_armv7.zi` or `compiler_aarch64.zi` encoder files.

### Players and Android

- Kryon's desktop player `tools/zx.zi` opens an embedding `Bundle`, validates
  capabilities, creates an instance in `OpenInstance` (`395-420`), then runs
  frames. Its second non-watch path repeats binding/instantiation at
  `572-610`; neither can select or report a native backend.
- Reusable Kryon player state is in `src/backend/bundle_player.zi:27-43`.
  `PlayerLoad` (`178-191`) verifies and binds, `PlayerStart` (`195-199`) calls
  `InstantiateBundle`, and `PlayerFrameNative`/`PlayerFrame`
  (`220-243`) drive the same embedding instance. Backend identity is not stored.
- `src/backend/hosted_app.zi:40-176` similarly opens, starts, runs, restarts,
  and closes a bundle without choosing or exposing the execution backend.
- Android already ships `app.zx`: `src/backend/android_bundle_run.zi:282-295`
  reads and opens it, and `314-343` hot-swaps instances. It currently receives
  libziran from `scripts/build_android_runtime.py:9-32`, which only forwards
  compiler/archive tools and generic flags; it has no target/ABI/native-loader
  switch. The default project ABI is currently `arm64-v8a` at
  `src/project/options.zi:457-464`; the Moto e6 requires an explicit
  `armeabi-v7a` evidence row.

### Benchmarks and historical controls

- The retained old-VM/TinyCC M0 and current engine measurements are described
  at `ASSEMBLY/milestones.md:85-93,135-144` and
  `plans/2026-11-13-native-optimizations.md:397-398,484-492,532-547`.
  Raw benchmark material belongs under `bench/results/`; it is historical input,
  not something to regenerate from removed TinyCC code.
- Platform policy requires physical Alpha for timings and distinguishes
  emulated correctness from performance at
  `ASSEMBLY/07-architecture-and-platform-coverage.md:91-100`.

## Target design

### One target-neutral machine image

Replace the ELF-shaped `MachineNativeOwner` with a target-neutral
`MachineCodeOwner` that owns `MachineProgram`, the selected encoder image, and a
`MachineImage` descriptor:

- code bytes and alignment;
- immutable constant bytes and alignment;
- per-instance writable-data template and alignment;
- function/entry offsets;
- typed relocations: code-relative function, data-relative global/fuel,
  runtime trap, platform service, and typed host capability;
- target/ABI, required CPU features, runtime ABI revision, and stack/fuel facts.

`MachineCodeBuild` always consumes a `LirProgram` obtained by `ZxExecRead`; ELF
and in-memory loaders are consumers of the same image. The x86-64 encoder is
the existing machine encoder. AArch64 and ARMv7 implement the same image and
relocation protocol, never separate lowering pipelines.

### Runtime layout and W^X

`compiler_native_load.zi` plans bounds and relocations without executing
anything. A small C platform shim owns OS calls that Ziran cannot yet express:
reserve RW anonymous pages, copy code/constants, resolve relocations, switch
code to RX and constants to R, allocate independent RW instance data, flush the
instruction cache where required, and unmap. There is no RWX transition. Failed
layout, relocation, protection, or cache flush destroys all mappings before
entry. x86-64 uses `mmap`/`mprotect`; Android ARM uses the NDK equivalents and
`__builtin___clear_cache`/`cacheflush` as selected by the compiler.

### Trampolines and host images

The interface-section node graph remains the source of host type spellings and
field names. Native code calls a target ABI trampoline with `(instance,
capability-index, host-image-base, result-image-base)`. The common bridge uses
the existing `ExecHostsCall` conversions and mutable-slice copy-back; target
assembly veneers only save registers, align the stack, spill ABI arguments into
the canonical image, and re-enter generated code with a success/trap value.
Platform console functions use the same table rather than special hidden
symbols. Reentrant and nested calls keep instance-local active-call frames.

### Backend policy

Add `BundleBackendPolicy { Auto, Engine, Native }` and
`BundleExecutionBackend { None, Engine, Wasm, NativeX86_64, NativeAArch64,
NativeArmV7 }`. Existing `BundleInstantiate*` uses `Auto`; a new
`BundleInstantiateWithOptions` lets tests, tools, and Kryon request exact
behavior. `Auto` resolves once from the bundle, installed Wasm provider, target,
CPU, and native preflight. `Engine` and `Native` are strict. Query APIs expose
the chosen backend and the native rejection reason, so fallback is observable.
Hot reload selects the replacement independently before migration and never
changes the running old instance on failure.

## Ordered implementation sessions

There are no intermediate commits. Each session ends with its focused local
proof; the last integration session runs the full gates before one coherent
commit per affected repository.

Path coordination: the sibling
[`2026-11-14-tool-names.md`](2026-11-14-tool-names.md) cutover should establish
`cmd/zi2zx`, `cmd/zx2native`, and `cmd/zx2wasm` before player integration.
References below to `cmd/zir-asm` and `cmd/zir-zx` cite the current 2026-11-14
checkout. If that cutover has landed, apply the named function change in its
permanent folder; do not restore an old directory or compatibility executable.
Sessions 1-4 may precede the rename when their files do not overlap the moved
drivers, but Session 5 and later must use the final tool paths.

### Session 1 — Freeze backend selection and native image contracts

Add/change:

- Add `cmd/compiler_machine_image.zi` with `MachineImage`,
  `MachineSection`, `MachineRelocationKind`, `MachineRelocation`,
  `MachineRuntimeABI`, `MachineImageValidate`, and `MachineImageFree`.
- Change `cmd/compiler_machine_ir.zi` relocation/call enums and owners so host,
  platform, trap, function, global, fuel, constant, and entry references survive
  allocation with stable symbol IDs.
- Change `cmd/compiler_machine_verify.zi` and
  `cmd/compiler_machine_verify_allocated.zi` to reject unknown relocation
  kinds, out-of-range symbols/addends, non-host external calls, native-width
  mismatches, and absent stack/fuel facts.
- Add `BundleBackendPolicy`, `BundleExecutionBackend`,
  `BundleInstanceOptions`, `BundleInstanceBackend`, and
  `BundleNativeProblem` declarations to `include/ziran_host.h`. Add matching
  raw declarations/wrappers to `std/bundle_host.zi`; implementations may return
  `None/TargetUnimplemented` until Session 4.
- Add `tests/native_backend_policy_cases.zi` and register it in
  `tests/check_plan.zi`. Cover strict Engine/Native, observable Auto selection,
  preflight-before-initializer, and no retry after native side effects.

Delete: nothing.

Proof:

```sh
build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY make check CHECK_FILTER=native_backend_policy CHECK_JOBS=1
build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY make check CHECK_FILTER=machine_ir CHECK_JOBS=1
```

Risks: `Auto` can become an implicit downgrade. The test must distinguish a
pre-execution whole-bundle choice from post-failure fallback and assert the
backend query on every instance.

### Session 2 — Merge x86-64 onto the machine encoder and delete the duplicate

Prerequisite: finish the queued Resize surface on machine IR, not in the direct
encoder.

Add/change:

- Change `cmd/compiler_machine_lower.zi` functions `LowerInstruction`,
  `LowerCall`, `LowerGlobal`, `LowerAllocate`, and `LowerResize` to cover every
  operation accepted by `X64Supported` in the old direct path, including
  initializers, aggregate copies, strings, Resize, host/platform calls, fuel,
  and constant data.
- Change `cmd/compiler_regalloc.zi` (`RegAllocAllocate` and spill/call split
  helpers) and `cmd/compiler_machine_verify_allocated.zi` for the completed call
  and aggregate surface.
- Change `cmd/compiler_x86_64.zi`: extend `Call`, `GlobalAddress`, `Fuel`,
  `Instruction`, and `Process`; emit `MachineImage` instead of an ELF-specific
  image, using `cmd/compiler_x86_bytes.zi` for the one x86 byte writer.
- Change `cmd/compiler_machine_pipeline.zi`: rename `MachineNativeBuild` to
  `MachineCodeBuild`, remove the initializer rejection at lines 108-111, and
  expose image/diagnostic views. Change `cmd/compiler_machine_elf.zi` to consume
  `MachineImage` for AOT ELF only.
- Change `cmd/zir-asm/main.c` native/map paths at `434-526,795,860-878` to use
  the machine image/ranges. Move any still-required map record to
  `cmd/compiler_machine_image.zi`.
- Move direct-encoder tests from `tests/spec/compiler_lowered_x64_test.zi` and
  `tests/native_services_cases.zi` into `tests/x86_encoder_cases.zi`,
  `tests/machine_elf_cases.zi`, and `tests/zx_lowered_parity_cases.zi`, preserving
  byte-size, symbol, aggregate, pointer, Resize, print, trap, and parity cases.
- Update `assembly.mk`, `Makefile`, `ASSEMBLY/README.md`,
  `ASSEMBLY/machine-ir.md`, and `ASSEMBLY/native-aggregate-abi.md` to name the
  one encoder.

Delete after all parity cases pass:

- `cmd/compiler_lowered_x64.zi`.
- `tests/compiler_lowered_x64.sh` and
  `tests/spec/compiler_lowered_x64_test.zi` after their unique assertions have
  moved.
- All `compiler_lowered_x64` objects/includes and `LirX64*` calls from
  `assembly.mk`, `Makefile`, and `cmd/zir-asm/main.c`.

Proof:

```sh
build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY make check CHECK_FILTER=x86_encoder CHECK_JOBS=1
build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY make check CHECK_FILTER=machine_elf CHECK_JOBS=1
build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY make check CHECK_FILTER=zx_lowered_parity CHECK_JOBS=1
build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY make check-bootstrap
```

Risks: the old path includes packed aggregates, allocation descriptors, float
printing, and ELF inspection features not yet in machine IR. Do not delete it
from file-count parity alone; every old test must have an identified new owner.
Keep AOT ELF and in-memory bytes as two layouts of the same encoded instructions.

### Session 3 — Implement the Linux x86-64 W^X loader and relocation engine

Add/change:

- Add `cmd/compiler_native_load.zi` with `NativeLoadPlan`, `NativeCode`,
  `NativeInstanceMemory`, `NativePlan`, `NativeApplyRelocations`,
  `NativePublish`, `NativeInstanceOpen`, and `NativeInstanceClose`.
- Add `cmd/zir/zir_native_memory.c/.h` as the small platform shim with
  `ZirNativeReserve`, `ZirNativeProtectRX`, `ZirNativeProtectR`,
  `ZirNativeFlushInstructions`, and `ZirNativeRelease`. On POSIX it must use
  page-rounded `mmap`/`mprotect`/`munmap`; no call accepts an RWX protection.
- Change `cmd/compiler_x86_64.zi:163-169,288-299` and
  `cmd/compiler_machine_elf.zi:118-239` so the shared relocation calculation is
  base-relative and overflow-checked rather than tied to `MachineElfBase`.
- Add `cmd/zir/zir_host_native.c/.h` with an opaque `NativeBundleCode` cache
  entry and `NativeInstance` lifecycle. Session 3 binds only entry, trap, fuel,
  globals, and platform console; typed arbitrary hosts remain a named preflight
  gap until Session 4.
- Add `tests/native_load_cases.zi` and `tests/native_wx_test.c`: malformed and
  overflowed relocations, wrong target/CPU/runtime ABI, protection-call failure,
  no RWX interval, independent writable state, shared RX code, fuel/trap, unload
  with live-instance protection, and no external compiler/linker in `PATH`.
- Add sources/objects to `Makefile` and `assembly.mk` without changing the
  default embedding backend yet.

Delete: the fixed-base relocation copy inside `MachineElfEncode` once both ELF
and memory consumers call the shared planner; any temporary x86-only loader
types introduced during the session.

Proof:

```sh
build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY make check CHECK_FILTER=native_load CHECK_JOBS=1
build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY make check CHECK_FILTER=native_wx CHECK_JOBS=1
build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY make check CHECK_FILTER=machine_elf CHECK_JOBS=1
```

Risks: PC-relative x86 relocations are limited to signed 32-bit reach. Reserve
one bounded arena for code/constants/trampolines or emit absolute veneers;
never truncate. Teardown must be reference-counted because bundle code outlives
individual instances.

### Session 4 — Add typed host trampolines and the embedding native backend

Add/change:

- Change `cmd/zir/zir_host_exec.c` by splitting interface parsing and host-image
  translation from engine execution: keep `exec_parse_interface`,
  `exec_from_image`, `exec_to_image`, `ExecHostsOpen`, `ExecHostsCall`, and state
  conversion common; move `ExecInstance*` engine code behind an explicit
  `EngineInstance` name in `cmd/zir/zir_host_engine.c/.h`.
- Add target-neutral bridge entry points to `cmd/zir/zir_host_native.c/.h`:
  `NativeHostInvoke`, `NativeTrap`, `NativeInitialize`, `NativeRun`,
  `NativeLimitSteps`, `NativeStateImage`, and `NativeStateStore`.
- Change `cmd/compiler_x86_64.zi` `Call` and prologue/epilogue emission to the
  native runtime ABI: SysV stack alignment, callee saves, reentrant instance
  pointer, typed host trampoline index, trap return, and fuel decrement at calls
  and loop back-edges.
- Change `cmd/zir/zir_host.c` `Bundle`, `BundleInstance`, `open_bundle`,
  `instantiate_bundle`, `BundleInstanceRun`, limit/state/export/import, and
  close functions to support Native. Implement
  `BundleInstantiateWithOptions`; keep old entry points as `Auto`.
- Change `include/ziran_host.h` and `std/bundle_host.zi` implementations for the
  Session 1 declarations. Make `BundleInstanceBackend` reliable after success;
  make native rejection diagnostic stable and queryable before instance open.
- Add native variants to existing host/component tests:
  `tests/host_capability_test.c`, `tests/host_slice_test.c`,
  `tests/record_host_test.c`, `tests/array_host_test.c`,
  `tests/bundle_component_test.c`, `tests/bundle_step_limit.sh`, and
  `tests/zx_web_lifecycle_host.c`. Add nested/reentrant host, mutable slice,
  record-by-name, persistent state, hot replacement, terminal native trap, and
  “initializer runs exactly once” cases.

Delete:

- Engine-only `ExecInstance` typedef/API names from `zir_host_exec.h` after
  callers use the backend-neutral instance dispatcher.
- Duplicated host-image translators from native/Wasm adapters; interface
  section `0x80000003` has one parser and one semantic conversion layer.

Proof:

```sh
build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY make check CHECK_FILTER=host_capability CHECK_JOBS=1
build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY make check CHECK_FILTER=bundle_component CHECK_JOBS=1
build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY make check CHECK_FILTER=bundle_step_limit CHECK_JOBS=1
build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY make check CHECK_FILTER=native_backend_policy CHECK_JOBS=1
```

Risks: C callbacks can re-enter the same bundle and can retain opaque pointers.
No trampoline may keep borrowed temporary images beyond a call; root/copy-back
ownership must match the engine exactly. Signals are not a trap API.

### Session 5 — Cut over Linux players bundle by bundle

Add/change in Ziran:

- Change `cmd/zir-zx/main.c` `run_command` (`223-262`) to accept
  `--backend auto|engine|native`, pass `BundleInstanceOptions`, and print the
  selected backend under an inspection flag. Default is `Auto`; explicit
  `native` is strict.
- Add native/engine paired cases to `tests/zx_executable.sh`,
  `tests/zx_lowered_parity.sh`, `tests/bundle_runtime_provider.sh`, and
  `tests/portable_entry.sh`. The same `.zx` bytes must produce equal output and
  state. Unsupported native input under `Auto` must select Engine before the
  initializer; under `Native` it must fail with no entry/host output.
- Change target row 0 in `cmd/compiler_target.zi:309-313` from `Gap` to
  `Implemented` only after every M2/M3 fixture, default-player integration, and
  W^X test passes. Update `ASSEMBLY/07-architecture-and-platform-coverage.md`
  and `ASSEMBLY/09-native-runtime-and-integration.md` with actual evidence, not
  planned claims.

Add/change in Kryon:

- Change `tools/zx.zi` `OpenInstance`, `Reload`, and `main` to request `Auto`,
  store/query the chosen backend, retain it in diagnostics, and ensure a reload
  selects the replacement before closing the current instance. Remove the
  duplicated binding/instantiation loop at `572-610` by calling `OpenInstance`.
- Change `src/backend/bundle_player.zi` `Player` to store backend identity;
  change `PlayerLoad`, `PlayerStart`, `PlayerOpen`, `PlayerClose`, and frame
  functions to use the options API without changing host capabilities or
  surface ownership.
- Change `src/backend/hosted_app.zi` `HostedAppOpen`, `HostedAppStart`,
  `HostedAppRun`, `HostedAppRestart`, and `HostedAppClose` so a preloaded,
  restarted, or migrated app follows the same one-choice-per-bundle rule.
- Extend `tests/zx_native_test.py`, `tests/zx_hosted_native_test.py`,
  `tests/zx_reload_test.py`, `tests/zx_headless_test.py`, and
  `tests/ziran_hosted_app_test.sh` with native-selected and engine-fallback
  assertions. GUI paths run only inside the existing private display harness.
- Update Kryon `mk/ziran-project.mk` dependencies if the embedding library gains
  native objects; do not introduce a separate native player binary.

Delete:

- The second `tools/zx.zi` binding/instance construction at lines 572-610.
- Player-specific backend guessing; only libziran preflight decides.

Proof:

```sh
build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY make check CHECK_FILTER=zx_executable CHECK_JOBS=1
build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY make check CHECK_FILTER=zx_lowered_parity CHECK_JOBS=1
build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY make check CHECK_FILTER=target_diagnostics CHECK_JOBS=1
build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY make -C ../../kryonlabs/kryon test-zx-native
build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY make -C ../../kryonlabs/kryon test-zx-hosted-native
```

Risk: Kryon hot reload and component migration expose state compatibility bugs.
Keep the old instance live until replacement compilation, binding,
initialization, and migration all succeed.

### Session 6 — Implement AArch64 on the shared machine pipeline

Add/change:

- Add `cmd/compiler_aarch64.zi` with `MachineA64Owner/Image`, form validation,
  two-pass `MachineA64Measure`/`MachineA64Encode`, AAPCS64 register classes,
  prologue/epilogue, calls, branches, scalar/float operations, loads/stores,
  aggregates, stack checks, fuel, host/platform/trap veneers, and relocation
  records matching `MachineImage`.
- Add `cmd/compiler_arm_bytes.zi` only for shared fixed-width ARM field writing,
  range checks, and immediate decomposition; keep target instruction selection
  in the target encoders.
- Change `cmd/compiler_machine_pipeline.zi` `MachineCodeBuild` to dispatch by
  `target.arch`, and change `cmd/compiler_machine_lower.zi`,
  `cmd/compiler_regalloc.zi`, and allocated verification for AAPCS64 fixed and
  reserved registers.
- Change `cmd/compiler_target.zi` `TargetMachineLoweringSupported` and rows
  `334-340` only when Linux/Android AArch64 proof exists. Keep Windows/macOS
  rows Gap.
- Add `tests/aarch64_encoder_cases.zi`, cross-disassembly fixtures, malformed
  relocation negatives, and qemu-user semantic smoke tests. QEMU proves
  semantics only, not platform completion or performance.

Delete: any x86 condition embedded in target-neutral lowering/allocation;
temporary AArch64-specific copies of machine IR or loader code.

Proof:

```sh
build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY make check CHECK_FILTER=aarch64_encoder CHECK_JOBS=1
build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY make check CHECK_FILTER=machine_ir CHECK_JOBS=1
build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY make check CHECK_FILTER=target_diagnostics CHECK_JOBS=1
```

Risks: AArch64 has fixed-width branch ranges and a required instruction-cache
flush. Veneers/islands must be planned before publishing RX pages; never patch
live code. Linux success does not by itself implement Android.

### Session 7 — Implement ARMv7/AAPCS32 and 32-bit host images

Add/change:

- Add `cmd/compiler_armv7.zi` with `MachineArmV7Owner/Image`, ARM/Thumb choice
  fixed by the target contract, AAPCS32 allocation, even-register 64-bit pairs,
  stack alignment, hard/soft-float policy, calls/returns, aggregates, checked
  memory, fuel/traps, host veneers, and shared relocations.
- Change `cmd/compiler_machine_ir.zi` and `cmd/compiler_machine_lower.zi` so
  native-width integers/pointers are target-width values rather than assumed
  64-bit host addresses. Change verifier and host-image adapters to reject any
  pointer/size that cannot round-trip through 32 bits.
- Change `cmd/compiler_target.zi` ARMv7 descriptions/feature requirements and
  `TargetMachineLoweringSupported`; rows `326-332` remain Gap until their
  separate Linux and Android gates pass.
- Add `tests/armv7_encoder_cases.zi`, relocation/immediate boundary tests,
  qemu-user parity, and explicit hard/soft-float mismatch negatives.

Delete: target-neutral `usize == u64` assumptions and any 64-bit-only trampoline
layout; do not delete 64-bit host-image semantics used by the portable format.

Proof:

```sh
build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY make check CHECK_FILTER=armv7_encoder CHECK_JOBS=1
build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY make check CHECK_FILTER=machine_ir CHECK_JOBS=1
build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY make check CHECK_FILTER=native_backend_policy CHECK_JOBS=1
```

Risks: ARMv7 ABI variants are not interchangeable. Pin the NDK triple/API,
float ABI, Thumb state, and atomic requirements in the target descriptor and
cache key; reject rather than guess.

### Session 8 — Load native ZX on Android and prove the USB Moto row

Add/change in Kryon:

- Change `scripts/build_android_runtime.py` arguments and command construction
  (`9-30`) to require `--abi`, `--api`, and target identity, and to build the
  native-loader/encoder objects for `arm64-v8a` or `armeabi-v7a`. Forward no
  host-machine CPU features.
- Change `scripts/android-build.sh`, Android Gradle/CMake/make inputs, and
  `mk/ziran-project.mk` so each APK ABI packages the matching libziran and the
  same `app.zx`; no platform-specific ZX is generated.
- Change `src/backend/android_bundle_run.zi` open/reload/run paths
  (`282-343,389-394`) to use `Auto`, log the chosen backend and strict native
  preflight problem, and preserve the old live instance on failed reload.
- Change `src/project/options.zi` validation so `arm64-v8a` and
  `armeabi-v7a` map explicitly to AArch64/ARMv7 target rows.
- Extend `tests/zx_android_test.py` and `tests/zx_hosted_android_test.py` with
  engine/native parity, host calls, frames, touch/text, assets, lifecycle,
  state migration, fuel, and native-gap cases.

Add/change in Ziran:

- Complete Android branches in `cmd/zir/zir_native_memory.c`, target preflight,
  AArch64/ARMv7 platform veneers, and `tests/diagnostic_android.sh`.
- Record reproducible device receipts under the established test-results
  location: APK hash, Ziran/Kryon revisions, NDK/API/ABI, serial
  `ZE2223BQZT`, CPU features, selected backend, commands, results, and logs.

Delete: engine-only Android assumptions/comments and any ABI-blind cached
libziran reuse. Preserve the portable engine as the per-bundle Auto fallback.

Proof (run through the managed Omega device route; `adb -s` is mandatory):

```sh
build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY make -C ../../kryonlabs/kryon android-test-zx ABI=armeabi-v7a ADB_SERIAL=ZE2223BQZT
build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY make -C ../../kryonlabs/kryon android-test-zx-hosted ABI=armeabi-v7a ADB_SERIAL=ZE2223BQZT
build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY make check CHECK_FILTER=diagnostic_android CHECK_JOBS=1
```

Risks: a cross-build or emulator cannot close the Moto row. Verify the live USB
device serial, installed APK identity, ABI actually loaded, and backend query.
Do not run GUI tests against the owner's live desktop.

### Session 9 — Cache, concurrency, full gates, and Alpha comparisons

Add/change:

- Add `cmd/compiler_native_cache.zi` with a content-addressed key over ZX digest,
  target/ABI, detected CPU features, machine/runtime ABI revisions, optimizer
  policy, instrumentation/fuel semantics, and host veneer revision. Cache only
  immutable machine images; instance state and host bindings never enter it.
- Add an atomic cache adapter in `cmd/zir/zir_host_native.c` using the established
  user cache path, private temporary file, fsync/rename, digest verification,
  bounded size, live-code reference counting, and corrupt/stale refusal.
- Extend `tests/native_build_cache.sh`, `tests/native_object_reuse_cases.zi`, and
  `tests/native_build_parallel.sh` for cold/warm equality, interrupted writers,
  corrupt entries, masked CPU features, runtime revision changes, simultaneous
  compile/instantiate/unload, and independent state.
- Extend `bench/zx_compare.zi`, `bench/zx_engine.zi`, `bench/README.md`, and the
  existing receipt writer so one synchronized Alpha cohort records:
  portable engine, cold native ZX, warm native ZX, AOT machine encoder, C
  control, and retained M0 old-VM/TinyCC receipts. Store raw samples, hashes,
  tool/ZX identities, CPU governor/features, warmup, sample count, medians and
  dispersion under `bench/results/`. Never reconstruct or relabel old TinyCC
  samples.
- Update `ASSEMBLY/milestones.md`, target/runtime documents, and the linked
  2026-11-13 plan only through their owning tasks; this plan must remain the
  implementation checklist.

Delete: cache entries from obsolete machine/runtime ABI revisions through the
bounded cleanup rule; no source deletion beyond the duplicate encoder already
removed in Session 2.

Proof on the current machine before timing:

```sh
build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY make check CHECK_JOBS=2
build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY make check-bootstrap
build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY make -C ../../kryonlabs/kryon check CHECK_JOBS=2
```

Alpha physical-host measurement (not a desktop/VM run):

```sh
build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY build/bin/ziran run bench/zx_compare.zi -- --engine --native-cold --native-warm --aot --c-control --m0-receipts bench/results
build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY build/bin/ziran run bench/zx_engine.zi -- --engine --native-cold --native-warm --output bench/results
```

The implementation session must first confirm these benchmark flags against the
landed harness; if the harness uses different names, update the harness and this
command together rather than silently omitting a cohort.

Risks: cache speed can hide compile cost and filesystem cache state. Report cold
and warm separately. “Native faster” requires correct output, identical
workload/ZX hash, physical Alpha evidence, and comparison to both the current
engine and the immutable M0 receipts.

## Final deletion ledger

After all gates, the tree no longer contains:

- `cmd/compiler_lowered_x64.zi`, its dedicated shell/spec test, or any
  `LirX64*` consumer;
- fixed-base duplicate relocation logic in the ELF writer;
- engine-specific public instance dispatch in `zir_host.c`;
- duplicated interface/host-image conversion in engine, native, and Wasm
  backends;
- Kryon's duplicate desktop binding/instantiation loop;
- x86-only assumptions in target-neutral lowering, allocation, images, loader,
  cache, and host trampoline code;
- Android runtime artifacts reused across ABIs.

Do **not** delete the portable engine: it is the cross-platform backend and the
documented `Auto` fallback for a whole bundle. Do not delete Wasm selection,
the M0 receipts, generated C/Go tools, or AOT ELF output.

## Cross-cutting risks and stop conditions

- Treat every bundle, cache file, relocation, host image, and returned host
  value as hostile input. Bounds/overflow failures occur before mapping RX or
  running initializers.
- A native target row remains `Gap` until its real OS/CPU evidence passes. Do
  not mark Android from Linux/qemu evidence or AArch64 from ARMv7 evidence.
- Preserve fuel, stack limits, initializers, state snapshots, hot reload,
  reentrant hosts, and terminal-failure behavior. Scalar “hello” parity is not
  enough.
- Native compilation must not call a C compiler, assembler, external linker,
  or removed TinyCC route.
- If `mprotect`/Android executable-memory policy refuses a mapping, strict
  Native fails; Auto may select Engine only if refusal is discovered during
  side-effect-free preflight, not after a native instance begins.

## Open questions for the owner

No blocking product choice is required. This plan interprets “engine fallback
per bundle” as the observable `Auto` policy above, while explicit `Native` and
`Engine` remain strict. If the desired default is instead always Engine or
always strict Native, that is a user-facing policy change; the loader,
trampolines, phone work, and tests remain the same.



