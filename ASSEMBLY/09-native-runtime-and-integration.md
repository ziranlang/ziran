# Step 09 — Integrate native ZX execution, AOT builds, caching, and hosts

Status: planned. Depends on [steps 04](04-portable-instruction-engine.md)–[08](08-assembler-objects-and-linker.md). Next: [inspection and final cutover](10-inspection-performance-and-cutover.md).

## Objective

Run `.zx` through the shared native backend where supported and through the new instruction interpreter elsewhere. Integrate source builds, packages, libraries, hosts, cache, and embedding APIs without relying on C/TinyCC for new-native code generation.

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
- [ ] Preserve execution budgets, stack checks, source-aware traps, parallel workers, persistent state, migration, snapshots, and module/library initialization across native and interpreted modes.
- [ ] Preserve the documented engine-selection contract. An explicit native request must fail clearly if unsupported; any automatic interpreter selection must be a documented policy visible in diagnostics and profiles.
- [ ] Add direct-native build/run targets to the package driver and target capability reports. Keep existing C/Go/wasm commands and legitimate target-specific bindings usable.
- [ ] Integrate package dependency recipes, native artifact caching, sysroots, host providers, assets, executable entries, libraries, install paths, and incremental rebuild dependencies.
- [ ] Replace the browser's generated-C bundle compilation path with direct wasm output from the same portable IR; retain interpreter execution where required by browser/runtime policy.
- [ ] Build runtime helpers through the new backend and verify self-hosting of the new compiler modules. Retain the established bootstrap mechanism; regeneration must be deterministic and locally checked.
- [ ] Remove TinyCC and generated-C dependencies from the new native ZX path after their replacement tests pass. Remove build/library dependencies only when no other maintained consumer needs them.
- [ ] Reconcile embedding API changes deliberately. Preserve semantic contracts, update callers through their owning projects, and reject retired entry points where repository policy requires a clean cutover.
- [ ] Verify installation from the current working tree at the stable executable path. Do not require a commit first, replace the owner's running application, or produce alternate task-specific binaries.

## Tests

Extend [native run](../tests/native_run.sh), [portable entry](../tests/portable_entry.sh), [bundle providers](../tests/bundle_runtime_provider.sh), [cache tests](../tests/native_build_cache.sh), [object reuse](../tests/native_object_reuse_cases.zi), [build concurrency](../tests/native_build_parallel.sh), [browser lifecycle](../tests/zx_web_lifecycle.sh), and [install tests](../tests/install_user.sh).

Add cold/warm cache correctness, interrupted writes, incompatible CPU cache reuse, stale runtime ABI, corrupted entries, load/unload races, nested host callbacks, parallel failure, and mutable-state separation. Test from stripped bundles with generated C/TinyCC unavailable. Run independent native AOT and native ZX instances against the same fixture expectations.

Use the full native local stack. All builds go through `build-queue --`; tests remove inherited display variables or use private displays. Dedicated performance measurement belongs on Alpha, not the desktop.

## Exit gate

The new-native standalone and ZX paths share code generation and encoding, all required hosts/packages work, cold/warm execution is correct, and current-tree installation is verified. No hidden AST/C/TinyCC fallback remains on a path advertised as the new native engine. Required downstream integration gaps remain incomplete until executed and verified.

## Handoff

Deliver API/CLI behavior, cache/runtime identities, lifecycle tests, package/host integration evidence, installed-path verification, and remaining platform limitations for final inspection and cutover.
