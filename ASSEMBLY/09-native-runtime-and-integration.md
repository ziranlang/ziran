# Step 09 — Integrate native ZX execution, AOT builds, caching, and hosts

Status: in progress. Linux x86-64 has an AOT slice and native ZX loading,
and Linux AArch64, Android included, has native ZX loading; `ziran run
app.zx` and the embedding API use it by default on those rows,
with the engine's pointer checks; a persistent code cache, the element-type
check on pointer access and the remaining targets are still planned. The current startup/syscall work is not a completion claim. Depends on
[steps 04](04-portable-instruction-engine.md)–[08](08-assembler-objects-and-linker.md).
Next: [inspection and final cutover](10-inspection-performance-and-cutover.md).

## Objective

Run `.zx` through the explicitly selected shared native backend or explicitly
selected new instruction interpreter. Integrate source builds, packages,
libraries, hosts, cache, and embedding APIs without relying on generated C for
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
rather than syscall 60. That opt-in AOT slice is implemented and executed.
Standard output is buffered, 8 KiB at a time and line by line on a terminal as
in C, and written before any write to another descriptor, before a trap's exit
and at exit, so its bytes and their order match direct writes. Its resize runtime, emitted with the x86 encoder
([`compiler_x86_runtime.zi`](../cmd/compiler_x86_runtime.zi)), recycles freed
blocks through power-of-two size classes instead of mapping and unmapping each
growth; a block over 1 MiB is its own mapping and grows by `mremap`.

Native ZX loading on linux-x86_64 and linux-aarch64
([`zir_host_native.c`](../cmd/zir/zir_host_native.c), with each architecture's
veneers and relocation fields in `zir_host_native_<arch>.c`)
compiles the bundle's verified program with the same machine pipeline, copies
the image into RW pages, resolves its relocations against the instance's own
data, host veneers, trap stub and resize runtime, and only then makes the code
RX; no mapping is ever writable and executable. Compiled code is shared by
open bundles of the same content digest on the same CPU and kept briefly
after the last closes. Each instance owns its globals, fuel counter, stack
limit and resizable storage. A trap returns through `longjmp`, prints a
`zx.runtime` diagnostic naming the function, block and (with a debug
attachment) source line, and ends that instance; nothing restarts on the
engine.

With checks on, native data pointers are checked as the engine checks them
([plan](../plans/2026-11-15-manual-safety.md)). A pointer is a
token naming a slot of the instance's pointer table, version and offset; every
load, store, byte comparison and platform-service argument resolves it
against a live slot of the same version, within its size and, for writes,
writable. Each run of `allocate` takes fresh zeroed bytes from the frame arena
under a slot on the frame's chain, and `Return` ends the chain and gives the
arena back. `resize` ends the old block's slot once the runtime has moved or
freed it; a request that does not fit gives null and keeps the old block.
Globals are slots 1..G, read-only ones without the writable flag. The host
bridge resolves the views it copies and gives returned views read-only slots.
The engine also checks a pointer's element type on each access; native code
does not yet.

`ziran build FILE.zi` composes `zi2zx` and `zx2native`, so source-native output
serializes and reads the same ZX image as direct `zx2native`; neither route
lowers source-side LIR directly. The required payload is stripped of source
origins; `zi2zx --debug` writes them to the optional debug attachment, bound
to the program section's identity, and `zx2native --emit map` names source
lines from it without changing a byte of the executable. Bundles and the
bundle Wasm tier use this shared instruction route.

For literal-only hello, direct instruction selection emits no stack frame or
spill slots, uses short constants, and writes the literal from read-only data.
The ELF file places `.rodata` immediately after `.text`, with congruent file
offsets and virtual addresses on distinct RX/R pages, and omits an empty
`.data` section. Function symbols retain positive bounded text extents;
inlined platform service declarations are omitted from the ELF symbol table,
so the static executable has no undefined symbols or misleading trap aliases.

## Existing integration points

- [Embedding API](../include/ziran_host.h), [bundle lifecycle](../cmd/zir/zir_host.c) and the [engine bridge](../cmd/zir/zir_host_exec.c), which translates host values for the [portable instruction engine](../cmd/compiler_exec.zi) and for the [native ZX loader](../cmd/zir/zir_host_native.c).
- [Wasm lowering](../cmd/zx2wasm/zx_wasm_lower.h) and [browser integration](../cmd/zir/zir_zx_wasm_web.c).
- [Package entry](../cmd/package_entry.zi), [options](../cmd/package_options.zi), [bundle run](../cmd/package_bundle_run.zi), and [capability reporting](../cmd/package_capabilities.zi).

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
- [x] Replace the browser's generated-C bundle compilation path with direct wasm output from the same portable IR. Interpreter execution remains an explicit separate mode where browser/runtime policy permits it, never a direct-Wasm fallback.
- [ ] Build runtime helpers through the new backend and verify self-hosting of the new compiler modules. Retain the established bootstrap mechanism; regeneration must be deterministic and locally checked.
- [x] Remove TinyCC and generated-C dependencies from the new native ZX path after their replacement tests pass. Remove build/library dependencies only when no other maintained consumer needs them. TinyCC, the generated-C ZX route and `libziran-vm.a` are gone.
- [x] Reconcile embedding API changes deliberately. Preserve semantic contracts, update callers through their owning projects, and reject retired entry points where repository policy requires a clean cutover. `BundleUsePortableVm` and `BundleInstantiatePortable` are deleted; Kryon calls neither.
- [ ] Verify installation from the current working tree at the stable executable path. Do not require a commit first, replace the owner's running application, or produce alternate task-specific binaries.

## Tests

Extend [native run](../tests/native_run.sh), [portable entry](../tests/portable_entry.sh), [bundle providers](../tests/bundle_runtime_provider.sh), [cache tests](../tests/native_cache.sh), [object reuse](../tests/native_object_reuse_cases.zi), [build concurrency](../tests/native_cache_parallel.sh), [browser lifecycle](../tests/zx_web_lifecycle.sh), and [install tests](../tests/install_user.sh).

Add cold/warm cache correctness, interrupted writes, incompatible CPU cache reuse, stale runtime ABI, corrupted entries, load/unload races, nested host callbacks, parallel failure, and mutable-state separation. Test from stripped bundles with no C compiler available. Run independent native AOT and native ZX instances against the same fixture expectations.

Use the full native local stack. All builds go through `build-queue --`; tests remove inherited display variables or use private displays. Dedicated performance measurement belongs on Alpha, not the desktop.

## Exit gate

The new-native standalone and ZX paths share code generation and encoding, all required hosts/packages work, cold/warm execution is correct, and current-tree installation is verified. No AST, generated-C or interpreter fallback remains on a path advertised as the new native engine. Every required downstream gap has a stable error and remains incomplete until implemented and executed.

## Handoff

Deliver API/CLI behavior, cache/runtime identities, lifecycle tests, package/host integration evidence, installed-path verification, and remaining platform limitations for final inspection and cutover.
