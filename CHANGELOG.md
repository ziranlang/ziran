# Changelog

## [Unreleased]

- A closed C program, linked with `--entry` and built with `--exe` or its own `main`, carries only the code it uses: each module header declares just what another generated file names, the module's `.c` keeps what only it reads, header inline copies stay only for procedures other modules call, and a module whose header would declare nothing has none. For Kryon's examples, headers shrink from 18 to 8.5 KB (window), 288 to 91 KB (hello), 319 to 102 KB (box) and 692 to 420 KB (image).
- Native x86-64 code reaches a global or a frame allocation directly when one shared LIR pass proves the access stays inside a live root, instead of resolving its checked pointer; the machine verifier now checks every load, store, zero and byte comparison and re-derives each direct access, and Wasm takes its index proofs from the same pass. Parameters, loaded pointers and heap blocks keep their resolves, so stale aliases, resized blocks and out-of-range runtime indices still trap. A LIR `zero` is now resolved like a store instead of writing through the raw token.
- `#insert NAME;` at file scope splices a compile-time string constant, such as a `#string` body or `#run` output, into the module as checked source; every target sees ordinary declarations, the same from source and saved IR.
- Modules take compile-time parameters: `#module_parameters(NAME := value, NAME: Type = value)` as a module's first declaration, and `#import "module"(NAME = value)` chooses the values, so `#if` keeps unchosen features out of C, Go, ZX, Wasm and native code. One program binds a module's parameters once; an unknown name, a value that does not fit and imports that disagree fail as `module.parameter_unknown`, `module.parameter_type` and `module.parameter_conflict`. Saved `.zir` is version 65 and keeps the bindings.
- A constant whose string holds `#import`, such as module source in `#string`, is no longer read as an import.
- Native code compiled for a bundle is cached on disk (`$XDG_CACHE_HOME/ziran/native`, or `~/.cache/ziran/native`), keyed by content, CPU features and compiler build and checked by digest before use, so later runs skip compiling; `ZIRAN_NATIVE_CACHE=DIR` moves it and `ZIRAN_NATIVE_CACHE=off` turns it off.
- `std/temp` adds portable scoped temporary byte storage with mark/reset; reset retires checked-pointer generations so engine/native stale reads trap instead of aliasing reused scratch.
- `std/context` adds a portable explicit logger, scope tag, and owned temporary allocator without an implicit parameter or global context.
- The shared LIR optimizer adds common-subexpression elimination, constant-text folding, precise scratch-slot accounting, safe dead-code and CFG cleanup, and Wasm range-based bounds-check elimination.
- Go accepts a module initializer that stores a procedure which later calls a host, as a feature module does when it registers itself; only a host call made while an initializer runs is refused.
- `zi2zx` rejects `--opt` on ZX input instead of ignoring it; saved ZX is never reoptimized.
- On Linux x86-64, `ziran run app.zx`, `zi2zx run` and the embedding API run bundles as native code with the engine's pointer checks: dangling frame pointers, use after free, double free and read-only stores trap, a native gap is a named error, and nothing falls back to the engine. `--backend engine` picks the engine.
- Native bundles export and import module state, so hot reload and component replacement keep their state.
- Native code checks the stack in calling functions and traps on runaway recursion, and native traps name the function and, with `zi2zx --debug`, the source line.
- The AArch64 native encoder emits the checked-pointer forms, the frame pointer chain, slot-aware resize and the calling-function stack check, as on x86-64, so its images carry a stack limit and pointer runtime. Inline traps branch past their sequence and keep their full 32-bit code. The loader does not run AArch64 images yet.
- `zi2zx --debug` writes source origins to the optional ZX debug attachment, and `zx2native --emit map` names source lines from it.
- `zx2native` executables start with unbounded fuel like a bundle run; `--fuel N` sets a budget.
- Native host calls return records, arrays and strings, and a returned string stays valid across later host calls.
- Native code starts every fresh local allocation as zeros, as the engine does.
- The instruction engine reuses the address a derivation last made, hands a run's console output to stdio in one write and returns from the entry without building a result; on Omega, zx loops 1994 → 857 ns, arrays 1767 → 891 ns and enums 547 → 229 ns per run.
- The instruction engine runs verified ZX about 4–5 times faster; on Omega, json_scan 665 → about 175 ms, image_resize 278 → about 50 ms and float text 43.7 → 11.9 s.
- The bundle Wasm tier runs in Asyncify Emscripten runtimes like Kryon's web player, suspending and resuming at host calls; JSPI still uses the engine, and `ZIRAN_WASM_TIER` opts out.
- Wasm modules run faster: loops become Wasm loops, records share frame slots, copies are unrolled, and printing is buffered before host calls and traps.
- Revision-28 lowering compares an untyped float literal with a float32 operand as float32 and reaches an assigned destination again after a call in its value.
- The engine and interpreter reuse address entries and heap storage, so large programs stop hitting address limits, and unreachable host-returned strings are reclaimed.
- Generated C indexes a slice of pointers, such as `[]*s32`, with pointer elements and compiles.
- `.zx` is one format: `zi2zx` and `ziran run` write and open revision-28 ZX only; the semantic bundle format, its VM and its libraries are removed.
- WebAssembly has one route: `zx2wasm` lowers verified revision-28 ZX with a typed host ABI and a heap; module state crosses as host images.
- The native x86-64 encoder runs record fields inside frame allocations and large records, matching the interpreter.
- The embedding API runs revision-28 bundles: typed host calls, `[]T` changes copy back, globals read and write by name, and components carry state.
- Revision-28 lowering covers every `Simd` operation, `#parallel` loops, the bit builtins, unions, `New(T)`, `free`, pointer fields, builders and owned `Text`.
- Revision-28 lowering adds `VecClone`, `VecGet`, `VecPop`, `VecResize` and `VecSwap`, and vectors move through records, parameters and results.
- Long `if`/`else if` chains and integer case lists lower without nesting limits, eight or more cases becoming a binary search.
- Host capabilities take and return records, fixed arrays, strings, slices and handles; returned views become read-only storage.
- `ziran check` matches a law's returned record by its declared type, including through another spelling of the type.
- A saved ZIR file from another toolchain names both versions and how to rebuild instead of a bare "unsupported ZIR version".
- Saved ZIR version 64 stores each expression's text once and the LIR optimizer folds constants and dead code, shrinking programs; regenerate saved `.zir`.
- `ziran inspect` shows each statement's source and each expression by what it means.
- The experimental direct pipeline lowers one verified ZX image to static Linux x86-64 ELF files with register allocation and no libc.
- The direct backend lowers strings, arrays, globals, host calls and dynamic accesses with checked bounds; unsupported constructs fail with named gaps.
- `ziran capabilities --json` lists assembly targets; `--assembly-target` rejects an unimplemented CPU, feature or OS service before reading source.
- Calls through a module alias accept types that module re-exports with `using`.
- Go output compiles `VecSlice` and slice bounds of mixed integer widths.
- `std/unicode` fixes full lowercase for ASCII A–Z in both allocation paths.
- Portable standard facades give one import for byte text, environment, timers, randomness, mapped files, streams, worker pools, directories and threads.

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
