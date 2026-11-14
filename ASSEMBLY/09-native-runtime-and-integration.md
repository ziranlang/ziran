# Step 09 — Integrate native ZX execution, AOT builds, caching, and hosts

Status: in progress at an experimental Linux x86-64 AOT slice; native ZX
loading, complete runtime/host integration, and the remaining targets are still
planned. The current startup/syscall work is not a completion claim. Depends on
[steps 04](04-portable-instruction-engine.md)–[08](08-assembler-objects-and-linker.md).
Next: [inspection and final cutover](10-inspection-performance-and-cutover.md).

## Objective

Run `.zx` through the explicitly selected shared native backend or explicitly
selected new instruction interpreter. Integrate source builds, packages,
libraries, hosts, cache, and embedding APIs without relying on C/TinyCC for
new-native code generation and without switching engines after a gap is found.

The experimental pipeline is source → `.zir` → linked LIR → verify →
LIR optimization → portable ZX bytes → verified read, followed by either the portable interpreter or direct target
lowering to a platform binary. Generated C and Go routes can also produce
binaries through their own toolchains, but they are not native here and cannot
substitute for the direct route.

Host capabilities remain portable typed interfaces. Each direct target owns
separate ABI veneers for entry, calls, operating-system services, and exit. The
initial Linux x86-64 console contract uses a static executable with its own
`_start`, no libc/CRT/dynamic linker, `write` syscall 1 through a `write_all`
loop that handles partial progress and `EINTR`, and `exit_group` syscall 231
rather than syscall 60. That opt-in AOT slice is implemented and executed;
native ZX loading and the wider integration work below remain open.

`zi2asm` source-native output serializes and reads the same ZX image as
`--input zx --emit elf`; neither route lowers source-side LIR directly. The
current stripped payload has no source origins, so source debug maps remain a
gap. Bundles and the bundle Wasm tier use this shared instruction route.

For literal-only hello, direct instruction selection emits no stack frame or
spill slots, uses short constants, and writes the literal from read-only data.
The ELF file places `.rodata` immediately after `.text`, with congruent file
offsets and virtual addresses on distinct RX/R pages, and omits an empty
`.data` section. Function symbols retain positive bounded text extents;
inlined platform service declarations are omitted from the ELF symbol table,
so the static executable has no undefined symbols or misleading trap aliases.

## Existing integration points

- [Native bundle API](../cmd/zir/zir_zx_native.h), [native engine](../cmd/zir/zir_zx_native.c), and [instance execution](../cmd/zir/zir_zx_native_run.c).
- [Host-value bridge](../cmd/zir/zir_zx_host.c), [host lifecycle](../cmd/zir/zir_host.c), and [browser integration](../cmd/zir/zir_zx_wasm_web.c).
- [Package entry](../cmd/package_entry.zi), [options](../cmd/package_options.zi), [bundle run](../cmd/package_bundle_run.zi), and [capability reporting](../cmd/package_capabilities.zi).
- [Current native engine build rules](../native-zx.mk).

Proposed modules: `cmd/compiler_native_load.zi`, `cmd/compiler_native_cache.zi`, `cmd/compiler_exec_instance.zi`, and a native backend command entry integrated into the existing package driver.

## Implementation work

- [ ] Expose one compile API taking verified executable IR, target description, optimization policy, and runtime ABI. AOT objects and in-memory images differ in output handling, not instruction semantics.
- [ ] Implement in-memory layout, symbol resolution, relocations, executable-memory allocation, write/execute transitions, instruction-cache synchronization, unwind registration, and teardown for supported platforms.
- [ ] Do not trust persisted executable bytes solely because a neighboring metadata file claims they were verified. Define cache integrity, ownership, atomic installation, invalidation, and refusal behavior explicitly.
- [ ] Key cache entries by executable content, linked libraries, target/ABI, actual selected CPU features, backend/runtime identities, optimization settings, and instrumentation semantics. Keep mutable instance state outside cached code.
- [ ] Preserve concurrent compilation, concurrent instantiation, nested calls, independent globals, reentrant host capabilities, cancellation, failures, and lifecycle retirement. Cache cleanup must not unload code still referenced by a live instance.
- [ ] Implement typed host trampolines using the shared ABI layer. Preserve aggregate/string/slice/function signatures, checked host returns, error propagation, and callback roots.
- [x] Implement and verify the first target-owned console ABI veneer for the
  opt-in Linux x86-64 AOT path. Its Ziran-owned formatter accepts literal
  format strings, strings/text views, booleans, every fixed signed and unsigned
  integer width, and `%%`; `write_all` retries partial progress and `EINTR`.
  Floating-point formatting and every unavailable target service remain stable
  named gaps. Other target rows remain unchecked.
- [ ] Preserve execution budgets, stack checks, source-aware traps, parallel workers, persistent state, migration, snapshots, and module/library initialization across native and interpreted modes.
- [ ] Make the engine-selection contract explicit. A native request fails
  clearly if unsupported and never starts the interpreter. A portable request
  never executes native code. A command may have one documented default, but
  preflight failure never changes that selection.
- [ ] Add direct-native build/run targets to the package driver and target capability reports. Keep existing C/Go/wasm commands and legitimate target-specific bindings usable.
- [ ] Integrate package dependency recipes, native artifact caching, sysroots, host providers, assets, executable entries, libraries, install paths, and incremental rebuild dependencies.
- [ ] Replace the browser's generated-C bundle compilation path with direct wasm output from the same portable IR. Interpreter execution remains an explicit separate mode where browser/runtime policy permits it, never a direct-Wasm fallback.
- [ ] Build runtime helpers through the new backend and verify self-hosting of the new compiler modules. Retain the established bootstrap mechanism; regeneration must be deterministic and locally checked.
- [ ] Remove TinyCC and generated-C dependencies from the new native ZX path after their replacement tests pass. Remove build/library dependencies only when no other maintained consumer needs them.
- [ ] Reconcile embedding API changes deliberately. Preserve semantic contracts, update callers through their owning projects, and reject retired entry points where repository policy requires a clean cutover.
- [ ] Verify installation from the current working tree at the stable executable path. Do not require a commit first, replace the owner's running application, or produce alternate task-specific binaries.

## Tests

Extend [native run](../tests/native_run.sh), [portable entry](../tests/portable_entry.sh), [bundle providers](../tests/bundle_runtime_provider.sh), [cache tests](../tests/native_build_cache.sh), [object reuse](../tests/native_object_reuse_cases.zi), [build concurrency](../tests/native_build_parallel.sh), [browser lifecycle](../tests/zx_web_lifecycle.sh), and [install tests](../tests/install_user.sh).

Add cold/warm cache correctness, interrupted writes, incompatible CPU cache reuse, stale runtime ABI, corrupted entries, load/unload races, nested host callbacks, parallel failure, and mutable-state separation. Test from stripped bundles with generated C/TinyCC unavailable. Run independent native AOT and native ZX instances against the same fixture expectations.

Use the full native local stack. All builds go through `build-queue --`; tests remove inherited display variables or use private displays. Dedicated performance measurement belongs on Alpha, not the desktop.

## Exit gate

The new-native standalone and ZX paths share code generation and encoding, all required hosts/packages work, cold/warm execution is correct, and current-tree installation is verified. No AST/C/TinyCC/interpreter fallback remains on a path advertised as the new native engine. Every required downstream gap has a stable error and remains incomplete until implemented and executed.

## Handoff

Deliver API/CLI behavior, cache/runtime identities, lifecycle tests, package/host integration evidence, installed-path verification, and remaining platform limitations for final inspection and cutover.
