# Changelog

## [Unreleased]

- ZX lowering turns a range test with literal bounds, such as `code >= 12288 && code <= 12543`, on a stable local, record member or fixed-array element into one unsigned comparison. Kryon's TextIdeographic drops from 55 to 41 instructions, and the Text widget scenario ran faster in Wasm in 8 of 10 alternating Alpha rounds, with the paint result checked on every call.
- Wasm frames hold only the record slots a procedure still uses. A large record read in place, or a zeroed record that is never built, reserves no frame space, so Shovey, whose level replay reads single fields of a 306 KB global, compiles to Wasm instead of exceeding the 8 MB frame limit.
- On x86-64 and aarch64, native console prints read their arguments straight from the saved registers instead of decoding each by type. On Alpha, arrays.zi went from 1.018 to 0.490 µs per call, enums.zi from 0.353 to 0.177, below Wasm, and loops.zi from 1.560 to 0.720.
- Native ZX gathers standard output in the same console buffer as the engine and hands it to stdio once per run, and before standard error, a host capability call or a trap report. On Alpha, arrays.zi went from 1.150 to 1.023 µs per call, enums.zi from 0.392 to 0.360 and loops.zi from 1.780 to 1.600.
- Native host calls resolve each function's capability or platform service once per instance, keep up to eight arguments on the stack instead of allocating them, and format printed integers directly. On Alpha, arrays.zi went from 2.075 to 1.122 µs per call, enums.zi from 0.686 to 0.378 and loops.zi from 3.155 to 1.718.
- `BundleInstanceOptions.unbounded` runs an instance without step metering: native code for it is built without fuel checks, like an unbounded executable, and takes no step limit. The standard and example benchmark hosts use it, as C and Wasm run unmetered. A normal native run now leaves its output to stdio buffering, as the engine does, instead of flushing stdout and stderr on every call. On Alpha, enums.zi went from 1.213 to 0.688 µs per call, arrays.zi from 2.563 to 2.059, score.zi from 0.033 to 0.022, dijkstra by 24% and fannkuch-redux by 14%.
- In-process native ZX no longer makes a system call on every entry: each thread keeps its stack range, and a forked child looks it up again under its own limit. Records & calls (score.zi) went from 0.511 to 0.032 µs per call on Alpha and text_demo.zi from 0.512 to 0.033 µs; arrays, enums and loops took 13–30% less time.
- Fixes: native ARMv7 and AArch64 code reads and changes members of large records and globals in place, as x86-64 code does, instead of copying the whole record into the frame for every access. Shovey's `LevelReplayPump`, whose 12 MB frame stopped it at its frame limit when Play was pressed on an ARMv7 phone, now needs 1.8 MB on every target. A record or array built from zeros is zeroed in one pass instead of one store per member, so Shovey's native code shrinks from 6.4 to 1.4 MB on ARMv7 and from 2.8 to 0.9 MB on x86-64, and its AArch64 build, where one 4.7 MB function put branches out of range, now compiles.
- Native code is verified once: the encoder no longer verifies again a program register allocation just verified, which shortens a phone's on-device compile. The native disk cache now works in Android apps: a library linked without a GNU build ID, as reproducible APKs are, is identified by its code, and an app process caches in its own cache directory, so only the first launch after an install compiles a bundle.
## [0.7.0] - 2026-11-18

- Wasm now runs all three Kryon widget scenarios faster than Go, and two of them faster than C: Button takes 2.46 µs, Text 3.59 µs and Input 3.24 µs per two frames on Alpha, against 4.33, 3.89 and 6.59 µs in Go. Wasm code reads a record's fields where the record lies instead of copying it, builds records where their caller or parent keeps them, writes a single changed field in place, and writes zeroed records with wide stores or `memory.fill`. `zx2wasm --unbounded` makes a module with no step budget for runs that need none.
- The optimizer inlines small procedures, reads record members through the inserts that built them, drops unused calls to procedures without side effects, specializes calls that pass a literal Boolean controlling a loop, and turns recursion such as `Fib(n-1) + Fib(n-2)` into an accumulator loop. It hoists fixed member reads, such as a slice's data and count, out of loops and rotates loops whose test loads from memory, so JSON round trips run 9.7% and radix sort 6.8% faster. It also folds interval checks such as `value > 1000 || value < 0` into one unsigned comparison. A Kryon widget's theme lookups become the constants they read.
- Native x86-64 code is faster. Division and remainder by constants use multiplies and shifts, constant shifts use immediates, and floating-point values stay in XMM registers across calls that leave those registers alone. Small blocks are allocated in process, so binary-trees runs in under half the time of the C reference. Functions and loops are aligned, so an unrelated edit no longer moves a loop's speed, and `--checks` builds no longer branch around their traps. Float64 `sqrt` and `std/math.Sqrt` use the hardware square root on x86-64, AArch64 and ARMv7.
- New standard library features: `std/base64` encodes and decodes RFC 4648 base64; `std/jpeg` reads baseline and progressive JPEGs and writes JFIF on every target; `std/png_linux` inflates PNG data portably; `std/finite` has inline `Finite32` and `Finite64`; and `std/parallel` adds `ParallelProcessors` and `ParallelInitLanes`.
- Audio is part of the core standard library, independent of Kryon: mixing, PCM streams, WAV with IMA and Microsoft ADPCM, MP3, and Ogg/Vorbis with streaming and seeking, played through SDL, WebAudio, or AAudio and OpenSL ES on Android.
- Fixes: integers format correctly across the full signed 64-bit range, including the minimum value. The native runtime builds on ARMv7 again. An in-process native run of an entry without a result reports zero. Register allocation no longer copies a register from an unrelated block across a block edge.
- The Targets page on the website compares C, Go, native ZX and Wasm in microseconds, all measured on one physical host at one compiler revision. It covers a 29-workload standard suite with independent C and Go references, Kryon widgets and JSON/UTF-8 workloads, with raw samples and generated code to download.
- Known issue: on ARMv7, native ZX can stop Kryon's `LevelReplayPump` at its frame limit. The fix is not in this release.

## [0.6.0] - 2026-11-16

- Ziran is manually safe like C by default: native x86-64, AArch64 and ARMv7 code, Wasm, generated C and the checks Ziran writes into Go carry no defensive runtime checks, so pointers are addresses and indexes are raw. Build with `--checks` to get every bounds check, pointer token and trap as before, or write `#checks` at the top of a module, before a block or statement, or before one index or slice access to check exactly there. The ZX header records how a bundle was built. The portable engine stays checked.
- Native code is much faster: tail calls, functions without frames, small records held in registers one field per register, copy-free reads of large-record fields, `[base + index*scale]` addressing, hot blocks laid out to fall into each other, and register allocation that gives a loop-carried value and its copy one register. Kryon's widget scenarios run natively in 8.6 µs (Button), 9.6 µs (Text) and 13.1 µs (Input) per two frames on Alpha, 121–185× faster than 0.5.0 and within about 3× of C.
- A native ELF carries only what the program uses: fuel, traps, stack checks, the pointer runtime, the heap and console buffering appear only when referenced, and `examples/hello.zi` is a 254-byte executable whose `main` is a few instructions.
- `zx2native --target x86_64 --platform windows` writes a Windows x86-64 PE32+ console `.exe` from verified ZX, with no C runtime and only the kernel32 imports the program uses.
- On linux-armv7, Android included, `ziran run app.zx`, `zi2zx run` and the embedding API run bundles as native code by default; ARMv7 code keeps 64-bit values in register pairs, and all 72 parity programs print the engine's output on an armeabi-v7a phone.
- `zx2native` writes Linux x86-64 executables for programs that call host capabilities, linking each capability by name from a registered host library (`NAME.library` beside the `.host` players in `ZIRAN_HOST_PATH` or `~/.local/share/ziran/hosts`).
- Native compilation is faster on very large programs: the 28.7 MB Unicode conformance bundle compiles in 7.4 s instead of 81 s.
- The plain `ReadU16LE`, `ReadU32LE`, `ReadS32LE` and `ReadF32LE` read at any alignment, as C does.
- The website's target and widget tables show fresh native measurements from Alpha, with the generated C, Go, ZX, checked IR and raw samples to download.

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
