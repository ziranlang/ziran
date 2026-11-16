# Changelog

## [Unreleased]

- Builds take `--checks`: `zi2zx`, `zi2c`, `zi2go` and the `ziran` commands that forward to them accept it, every module of the build is marked checked, and the ZX header records it, so a saved bundle, `zx2native`, `zx2wasm` and the native loader see how it was built (`zi2zx --emit lir` prints `checks`).
- Native x86-64 code tail-calls: a direct call that its function returns at once leaves the frame and jumps, so the callee returns straight to the caller. A function that keeps nothing on the stack, cannot trap and calls nothing but such a tail call has no frame at all. A function returning a constant is `mov eax, 42; ret`.
- A native ELF carries only what the program uses. Fuel counter, trap stub, stack limit, pointer runtime, heap, console buffer and each runtime routine appear only when referenced. An executable with an unbounded budget (the default) has no fuel checks, and a function that only calls runtime services checks no stack limit. A program whose one console write runs once writes directly, with no buffer or terminal probe. Text follows the headers and data follows the text with no page padding, and an image with nothing writable has one program header. `examples/hello.zi` shrinks from 8,352 to 576 bytes.
- Native compilation checks fuel facts, call references and image relocations in one pass each instead of rescanning every relocation per instruction, call or encoded reference: on Omega the 28.7 MB Unicode conformance bundle compiles natively in 7.4 s instead of 81 s, with the same output.
- Native x86-64 code keeps a looping function's step budget in r15 (stored at calls, returns and traps) and checks it at a loop header with one fall-through compare. A small record of scalar fields that is only built, changed and read by member and passed between blocks is held as one register value per field, with no frame slot. On Omega, Sieve drops from 59.6 to 47.1 µs natively.
- On linux-armv7, Android included, `ziran run app.zx`, `zi2zx run` and the embedding API run bundles as native code by default. The Moto e6 play (armeabi-v7a, Android 9) prints the engine's output for all 72 parity programs natively and from the native cache (`make android-parity ANDROID_ARCH=armv7 ANDROID_API=28 ANDROID_SERIAL=…`, `bench/results/zx-android-parity-moto-2026-11-16.json`), and Kryon's ZX player runs its bundle natively there. On both ARM targets host calls return records, and copies and zeroing of 64 bytes or more run a loop, so code size no longer grows with record size.
- Native code encodes for ARMv7 (A32) on Linux and Android: general values live in register pairs (r0:r1 … r12:lr), 64-bit divides and int64/float conversions call loader helpers through a `RuntimeHelper` relocation, addresses are MOVW/MOVT halves, and all 72 parity programs encode; `armv7_encoder` cases pin the A32 words against the NDK assembler.

## [0.5.0] - 2026-11-16

- ZX runs as native code by default on Linux x86-64 and AArch64, Android included: `ziran run app.zx`, `zi2zx run`, the embedding API and Kryon's players run a bundle's verified program as native code in W^X memory. Stray and freed pointers, double free, out-of-range indices, read-only stores and runaway recursion trap; a native gap is a named error and nothing falls back to the engine. `--backend engine` picks the portable instruction engine.
- Native code is fast: on Alpha's Intel Core i7-8700, warm native ZX runs a scalar and a structured workload 11× and 17× faster than the portable engine, Kryon's widget scenarios take 1.2–2.0 ms per two frames instead of the removed VM's 43–73 ms, and the Sieve, Records, Hash, Vectors and Printing kernels run within 1.3–4× of C. Shared frame slots, in-place member access, dropped checks where indices and pointers are proven, direct compare branches, buffered output and recycled vector blocks get it there.
- Native code compiled for a bundle is cached on disk (`$XDG_CACHE_HOME/ziran/native`), keyed by content, CPU features and compiler build; `ZIRAN_NATIVE_CACHE=DIR` moves the cache and `ZIRAN_NATIVE_CACHE=off` turns it off.
- Native bundles keep module state across hot reload and component replacement, return records, arrays and strings from host calls, start fresh locals as zeros and, built with `zi2zx --debug`, name the source line of a trap; `zx2native --emit map` shows the same origins, and `zx2native` executables take `--fuel N`.
- `.zx` is one format: `zi2zx` and `ziran run` write and open revision-28 executable ZX only. The semantic bundle format, its VM and the TinyCC tier are removed; rebuild older `.zx` files.
- WebAssembly has one route: `zx2wasm` lowers verified ZX with a typed host ABI and a heap. Wasm modules run faster (real loops, shared frame slots, buffered printing) and run in Asyncify runtimes such as Kryon's web player.
- The portable instruction engine runs verified ZX about 4–5 times faster.
- The embedding API runs revision-28 bundles with typed host calls: records, fixed arrays, strings, slices and handles cross the boundary, `[]T` changes copy back, globals read and write by name, and components carry state.
- ZX covers every `Simd` operation, `#parallel` loops, the bit builtins, unions, `New(T)`, `free`, pointer fields, builders, owned `Text`, the remaining `Vec` operations, and long `if`/`else if` chains and case lists.
- A closed C program carries only the code it uses: module headers declare only what other files name, runtime headers keep only the helpers it calls, and the entry is C's own `main`. Kryon's example headers shrink to a third of their size or less.
- Modules take compile-time parameters with `#module_parameters(...)` and `#import "module"(NAME = value)`, and `#insert NAME;` splices a compile-time string into the module as checked source. Saved `.zir` is version 65; regenerate saved `.zir` files.
- `std/temp` adds scoped temporary storage with mark/reset, `std/context` an explicit logger, scope tag and temporary allocator, and portable facades give one import for byte text, environment, timers, randomness, mapped files, streams, worker pools, directories and threads.
- Environment, argument, binary, file, HTTP and host-byte readers return typed outcomes that name why input was refused.
- `ziran learn on|off|status` keeps a local, size-capped corpus of native builds for a learning compiler. It is on by default and silent; `ZIRAN_LEARN=off` turns it off.
- The experimental direct pipeline lowers verified ZX to static Linux x86-64 ELF files with register allocation and no libc, and `ziran capabilities --json` lists assembly targets.
- `ziran inspect` shows each statement's source and what each expression means, and a saved ZIR file from another toolchain names both versions and how to rebuild.
- The toolchain builds and runs on macOS again, on Apple Silicon and Intel.
- The website's playground works again, and its Kryon widget table compares C, Go and native ZX with fresh measurements.
- Fixes: generated C indexes slices of pointers; Go compiles `VecSlice` with mixed-width bounds and module initializers that register host-calling procedures; calls through a module alias accept types it re-exports; `std/unicode` lowercases ASCII A–Z correctly; a string constant holding `#import` is no longer read as an import; `ziran check` matches a law's returned record by its declared type; `zi2zx` rejects `--opt` on ZX input.

## [0.4.2] - 2026-11-11

- `make zx-tools` builds the experimental scalar Linux x86-64 ELF path with verified IR, inspection dumps and source-range maps; other backends unchanged.
- Run a source file directly with `zi2zx run hello.zi`; website examples use the one-command `ziran run hello.zi` flow.
- The website's target page adds six JSON and UTF-8 workloads measured on Alpha, with downloadable output and raw samples.
- JSON readers scan long unescaped strings with SIMD, preserving borrowed text, escapes and error positions.
- SIMD byte-vector loads read immutable strings with whole-access bounds checks, and multi-module SIMD programs keep their operations in generated C.

## [0.4.1] - 2026-11-11

- Portable ZX runs unchanged desktop loop bundles about 14 times faster, with records, text and arrays also improving.
- The target page adds real Kryon widget scenarios with complete checked IR; Targets stays in shared navigation with its own IR inspector.
- The target matrix compares warm execution on desktop, Android and web with raw samples; medians exclude anomalies with 1.5×IQR fences.
- WebAssembly handles omitted slice bounds and empty slices at the end of arrays.
- Fresh native builds avoid optimizing the temporary bootstrap compiler.
- Generated C includes only the headers it uses and omits empty source files.
- `ziran inspect` inspects portable `.zx` bundles, including checked modules and bytes.
- Build WebAssembly with `ziran build --target=wasm`, including Hello World programs.

## [0.4.0] - 2026-11-10

- The website gains a target matrix across C, Go, portable ZX, Wasm and checked IR, with downloads, in-browser Wasm and measured costs.
- `ziran run app.zx` discovers installed package runtime providers for host capabilities; console and graphical bundles share the command.
- `pkg paths PACKAGE...` returns ordered resolved paths as JSON after one graph validation.
- Reused compiler builds refresh embedded runtime headers by content, so restored timestamps cannot leave stale C helpers.
- Linked functions keep their copied capacities consistent with storage, preventing heap corruption after dead-code pruning.
- The parser, effect derivation, import linking, proof kernel, theorem checking and the `.zx` container run as Ziran code, with the same results.
- `#run` evaluates text of any length with builders, pointers, slices and folded loops, and shares one evaluator with `#if`, `#ifx` and `#assert`.
- `.Member` resolves against an enum imported from another module, and an imported constant can size an array.
- Record fields and enums take `#c "SPELLING"` so generated headers match shared C code; `const char *` fields compile and headers stop macro-defining constants.
- `#asm` takes slices, clobber ranges and one variant per line; `ziran run` runs them with TinyCC, bundles always run portable bodies.
- `-DZIRAN_NO_HEADER_INLINE` keeps generated C calls external, so link-time interposition such as `ld --wrap` sees every call.
- A `#target` procedure's SIMD-tier copies call that tier's `#inline` procedures; the AV1 decoder decodes 1080p frames 2–4% faster.
- A unary operator inside a binary one keeps its grouping in generated C and Go.
- Generated C proves away more bounds checks: pre-guard locals, fixed ranges, parameter fields, `else` branches, `ifx` picks; may-wrap checks stay and stored-before-read locals skip zeroing.
- Native SIMD vectors such as `s16xN` hold as many 128-bit blocks as the code handles, every width gives the same results, and `#target` picks the best instructions once at startup.
- Generated C includes only the SIMD helpers a program calls and compiles each tier in its own file; TinyCC compiles the AV1 decoder a third faster.
- The portable VM keeps `std/simd` vectors inline, TinyCC and unoptimized C run operations inline, and Go emits typed code per operation (352p AV1 decode 346 → 44 ms).
- `std/simd` adds `Simd.Sign` and `Simd.AbsDiff`, runs loads, stores, widening and 256-bit halves as single instructions, and its scalar fallback runs about nine times faster.
- Standard SIMD vectors such as `u8x16` and 256-bit forms give the same bits on every target.
- On 32-bit ARM, `f32` vector arithmetic keeps tiny values.
- `std/resample` kernels use `std/simd` instead of assembly, up to twice as fast with Wasm SIMD128.
- String constants over 4095 bytes build as strict C with escaped trigraphs, and generated C builds without warnings under `-Wall -Wextra`.
- Standard libc bindings use `usize`/`isize` sizes so they link and run on wasm32.
- `VecSlice` can view a global Vec or a Vec field, which cannot grow, be freed or be replaced while the view lives.
- `build --target=c --exe` links with Emscripten, keeping the `.wasm` file.
- Each `#load` file can store and index the same `Vec` or `Option`, and a public record with module-private field types builds in importing modules.
- Fresh `Text` in checker edge cases is freed with its block, and assigning a Vec or Text field frees the old value.
- Checking resolves constants through the lookup cache, `forall` laws check in half the time, and Breeze lowers 921 modules in under a second.
- `#reads` follows strings through field paths and marks read-only procedures and foreign calls, so `Text` passed through is freed too.
- A `Text` result only read in its statement or local is freed when its block closes; globals holding a `Vec` or `Text` can be assigned or moved.
- `std/vec` gains owned UTF-8 `Text`; `TextView` borrows, `TextCopy` copies, producers like `JsonWrite` return it, and files naming it import `std/vec` automatically.
- A local named like a C library function or Go builtin no longer hides it, and without a project `std/NAME` always loads the standard module.
- `make install-user` keeps a `~/.local/bin/ziran` wrapper and says so instead of failing.
- `for` loops, slicing and move-out work on Vec record fields; pushing or freeing while looping is rejected, and Go compiles `for` over a local Vec.
- Integer literals with separators, hex, binary and `u64` values above `s64` work in globals, constants, tables, array sizes and ranges.
- Shadowed locals such as `x := x + 5` in an inner block read the outer binding on every target.
- Generated C inlines small same-module and cross-module calls, copies arrays once, keeps one vectorizable loop exit, and honors `#inline` and `#cold`.
- Checked division, shifts and indexes run inline under TinyCC and unoptimized C, about 17% faster under `ziran run` and 24% faster at `-O0`.
- Constant tables bind across several loops, file-scope initializers and nesting, and constant arrays are read from one shared read-only copy.
- `ziran run` charges step budgets once per procedure and literal range, and portable programs use prepared function and global indices for calls and reads.
- Generated C accepts checked pointer casts to imported C callback types.

### Added
- Owned vectors support fallible reserve and resize with matching C, Go and portable behavior.
- `std/web.WebBlobBytes` snapshots file or media bytes into a browser Blob without a typed-array copy.
- Project commands accept `--module-path DIR` for generated modules; dependencies still resolve only from the lock.
- `--exe --release` builds with link-time optimization.
- `#parallel_gpu for` runs on the fastest GPU in native Linux and Android builds, falling back to CPU threads.
- Python build tools consume verified dependency artifacts, and dependency-owned artifact recipes publish declared outputs atomically.
- `std/utf8` gains bounded, repairing `Copy`; `std/text_tools` gains `TextHexDecodeInto`.
- Linux and Android hosts on x86-64, ARM and ARM64 run portable bundles as native code, cached between launches, with `#parallel for` on one worker thread per processor.
- `zx2wasm` lowers a checked program's scalars, strings, slices, globals, records and fixed arrays to one wasm32 module; other programs fail with named diagnostics.
- `BundleInstantiate` gained a WebAssembly execution tier behind a provider seam; other bundles fall back to the portable VM.
- `std/vorbis` reads a stream's channels, rate and length; `std/vorbis_linux` decodes whole streams to 16-bit PCM, linking the library only on import.

### Removed
- The Python target: `zi2py`, `--target=py`, `py:` imports and the `std/*_py` modules. Run native scripts with `ziran run --target=c`.
- The separate `zi2zx-vm` runner; `zi2zx run --vm FILE` runs a bundle on the portable VM instead.

### Changed
- Native `--exe` builds default to `-O3`, and lowering a ZX bundle to C no longer copies each read-only field.
- Compiler commands run at low CPU and disk priority and use at most half the CPUs, bounded by free memory (`ZIRAN_NICE`, `ZIRAN_JOBS`).
- Native builds run `#parallel for` regions on one thread per processor instead of four; `ZIRAN_PAR_THREADS` still chooses.
- Saved IR writes each string once and stores numbers compactly, so `.zir` files and `.zx` bundles carry about a third of their former size; rebuild format 58 files.

### Fixed
- Later lines of a multiline string keep their text instead of old type spellings.
- Portable record globals initialize fields from open-import constants.
- Caching native script runs survives compiler binaries larger than 16 MiB.
- `ziran lock` and `ziran update` resolve a commit also pinned as a source checkout, and lock one package named by branch and by commit.
- Android applications report portable execution errors in the system log.
- Artifact recipes accept up to 256 inputs.
- Add-ons can use records, functions and globals a shared library did not retain; only those bundles need format 27.
- Native C builds keep globals and constants separate from foreign library symbols.
- Bundles accept arrays of fixed arrays as parameters.
- `#parallel for` regions reject mutating nested calls and shared-slice writes; native workers see every binding their body reads.
- Saved IR keeps the result type of a call whose argument is another call with widened arguments.
- Natively run bundles reach a provider exported under another symbol.
- Go output converts a conditional's arms to its type.
- `StartProcess` and `RunProcess` open redirected files relative to the caller's directory.

## [0.3.1] - 2026-10-23
### Fixed
- Source generation preserves output errors on hosts using file-backed text streams.
- Website release links follow the installed compiler version.

## [0.3.0] - 2026-10-23
### Changed
- Portable executables and libraries use `.zx`, the `zi2zx` tool and the `zx` target; source stays `.zi`, checked modules `.zir`. Rebuild previous `.zib` programs.

### Fixed
- Go output no longer needs temporary filesystem space and reports output write failures.

## [0.2.0] - 2026-09-28
### Added
- `ziran version` (and `--version`) prints the compiler version.
- Release tags follow the compiler, so `ref = "v0.2.0"` in `ziran.toml` pins a release.

### Changed
- A clean build of the compiler is much faster and uses at most eight cores.

### Fixed
- Building `ziran` from scratch on older `make` versions no longer fails to link.

Earlier versions were tagged `v0.1.x` without a changelog.
