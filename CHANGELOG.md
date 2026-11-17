# Changelog

## [Unreleased]

- Native x86-64 code tests an inclusive `for` range with a computed end once per iteration instead of twice: after leaving on the last value, the copy of the header test that loop rotation leaves in the latch always holds and becomes a jump. The LIR keeps the test, which the vectorizer reads.
- `Finite32` and `Finite64` live in the new `std/finite` module, which `std/math` re-exports, and are `#inline`: generated C gets a static inline copy of each in `finite.h`, so callers no longer make an out-of-line call per component. NaN and infinities still give false.
- An in-process native run of an entry without a result reports zero instead of whatever its last call left in the result register, as the portable engine does.
- In-process native x86-64 code allocates and frees blocks of up to 256 bytes itself: 16-byte classes with free lists and chunks per instance, behind a 16-byte header that still rejects a block with the wrong count or element size. Larger blocks and growth go through the host as before. Binary-trees runs in under half the time of the C reference (46 ms against 98 ms on Alpha), down from 223 ms.
- Native x86-64 division and remainder by any other positive constant use a high multiply instead of `idiv`, signed and unsigned at every integer width. FASTA, whose generator takes `%139968` and `%1000000007` per byte, runs in less than half the time.
- Native x86-64 code starts every function on a 32-byte boundary and keeps each branch, compare-and-branch pair, call and return inside one 32-byte block, with NOPs where needed. Intel cores with the jump-erratum microcode fix keep such loops in their decoded-instruction cache, so a change elsewhere in a program no longer moves a hot loop's speed by up to a third.
- Native x86-64 branches on an ordered float comparison (`<`, `<=`, `>`, `>=`) jump on the compare's flags, NaN included, instead of first building a bool and testing it.
- `bench/bench.zi standard-ab` times the C reference and several in-process native runners on the standard suite in shuffled rounds on one CPU, checking every result against C. `standard-profile` reports SIGPROF samples of a runner by function, with the hottest native code disassembled.
- Native x86-64 checks no longer branch around their traps: a passing check falls through, and its trap call sits after the function's blocks, so loops with bounds, division, shift, stack and step checks keep one straight path. Trap reports still name the check's function, block and source line.
- Native x86-64 additions can write a separate result register with LEA, keeping reused inputs live without tied copies; narrow integer results retain their normalization.
- Register allocation keeps spilled values in memory through blocks that only carry them to the next control-flow edge, avoiding a reload and another spill before the next call. Joins retain their register allocation.
- Native x86-64 integer-to-float conversions clear unused XMM lanes before converting, removing the legacy SSE instruction's dependency on an earlier floating-point value while preserving rounding and condition flags.
- Native x86-64 returns simple integer base cases before allocating a frame or saving registers, while preserving checked and fueled paths. Adjacent 64-bit register copies and constant additions/subtractions use one address instruction, including recursive call arguments.
- WAV decoding supports mono/stereo IMA ADPCM and Microsoft ADPCM in the core audio module and mixer, with fact-based frame counts, bounded block validation and independent PCM fixture tests.
- Float64 `sqrt` and `std/math.Sqrt` lower to hardware square root on x86-64, AArch64 and ARMv7, with the same IEEE rounding on the portable engine, generated C/Go and Wasm. An independent integer reference checks every exponent and one million bit patterns.
- ZX lowering folds signed integer interval checks such as `value > 1000 || value < 0` into one unsigned comparison when both sides read the same stable local, record member or fixed-array element. Calls and other expressions whose second evaluation can matter retain short-circuit evaluation.
- Native verification distinguishes integer and floating-point scratch registers when checking parallel copies, so valid floating-point argument copies are accepted even when register numbers match across banks.
- Core audio builds share SHA256-pinned Xiph Vorbis archives across native targets; Windows uses the correct C-long ABI, verified against every public field offset and actual streaming/seek decoding.

- Native register allocation resolves values across block edges without copying a register from an unrelated block laid out earlier. This fixes tied floating-point results in inlined control flow.
- Native x86-64 division and remainder by a known positive power of two use shifts and a signed rounding bias instead of `idiv`, preserving truncation toward zero and remainders at every integer width.
- The website's kernel and standard tables are measured with `bench/wasm_tier.py --host-gate`: each job runs pinned to one logical CPU of the quietest core, with Go at `GOMAXPROCS=1`, and runs again unless that CPU held a steady clock at its calibrated sustained speed with its hyperthread sibling idle. Rejected attempts stay in the receipt, and the generators refuse ungated receipts; the earlier native kernel column had run at about half clock. The kernel table's ZX native column runs each kernel's bundle translated to machine code in process, in the same harness as C and the engine, so it no longer includes process startup; each kernel's `zx2native` executable is still run once as an output check. Every /targets table is measured again at one compiler commit, and the widget downloads name Kryon's license after its repository with no stale copies left from earlier runs.
- Audio mixing, PCM streams, Ogg/Vorbis inspection and streaming/seek, WAV/MP3 decoding and SDL/WebAudio output are core standard modules independent of Kryon. Android streams through AAudio on API 26+ and OpenSL ES on API 21–25, with the same mixer and sink contract as desktop.

- Loop invariant hoisting leaves standalone constants and symbolic addresses near their uses while retaining useful invariant expressions. This reduces spills in vector growth loops; the matched Alpha Vectors kernel improves from 85.8 to 42.5 µs with unchanged results.

- `bench/standard/` holds the benchmarks people use to compare languages, in Ziran, C and Go with every input read at run time: recursive Fibonacci, n-body, spectral-norm, mandelbrot, fannkuch-redux, binary-trees, fasta, nqueens, integer matrix multiplication, quicksort and FNV-1a string hashing. `bench/wasm_tier.py --standard` runs them as C (`cc -O2`), Go, native ZX and Wasm on Alpha's physical host and checks every result against C, at diagnostic sizes or the Benchmarks Game sizes (`--standard-full`); `bench/bench.zi -- standard-compile` times whole-suite builds for each target. The small-kernel table gains independent Go references. The website shows the results, and `bench/results/standard/gaps.md` ranks where native ZX trails C: on spectral-norm, fasta, binary-trees and matmul the same program runs much closer to C as Wasm, so the native backend is the gap there.
- The website's benchmark tables share one number format: three significant digits with thousands separators, never scientific notation, and one unit per table named once in its header (ns for the example programs, µs for kernels, workloads and the standard suite, ms for compile time). The Python generators use `format_number` in `scripts/site_targets.py`, the Ziran renderer uses `scripts/site_numbers.zi`, and `tests/site_targets.sh` checks both against one set of cases and the page for scientific notation. The example and JSON/UTF-8 tables were measured again with the updated harness on the same host and compiler commit.
- Native x86-64 code builds the record a function returns where its caller keeps the result instead of in a frame object it then copies: when every return gives the same record, that record and the members built in it lie in the caller's result memory, which the function keeps the address of in `r14` for its whole body (saved and never allocated), so the return has nothing to copy. A record returned on two different paths, a parameter, or a parameter with a member changed keeps its copy. The machine IR gains a `result_register` flag on functions and placement into a function's `ResultAddress` object, and both verifiers check them.
- Native x86-64 code builds a record member where its parent keeps it instead of in a temporary it then copies from: members of a nested literal, a member inserted into a built record, one replaced through a constant index and a call result used as a member take frame space inside the parent's, and a member that is also read, shared, updated afterwards or whose parent is read while it is built keeps its own. An aggregate build whose members are all replaced before it is read emits nothing, and the optimizer folds f64-to-f32 and f32-to-f64 constant conversions and float negations to the hardware's patterns (NaNs stay). A nested shadow-token builder shaped like Kryon's `DefaultShadows` shrinks from 1759 to 661 bytes of code. The machine IR gains placed stack objects (`placed_in`, `placed_at`), and the allocated verifier checks each lies inside its parent.
- Wasm code reads a large record's fields where the record lies instead of copying it whole, gives record parameters and results the caller's storage, builds a member, call result or returned record where its parent or caller keeps it, and leaves out constructor stores that are overwritten before use. On an Intel Core i7-8700, Kryon's Button, Text and Input widget scenarios take 5.6, 7.3 and 8.4 µs per two frames in Wasm instead of 1056, 715 and 1234 µs, faster than native ZX.
- Verifying and saving LIR is faster: the verifier checks that each definition dominates its uses against one dominator tree per function instead of searching the control flow graph for every use, and the LIR codec reads and writes a whole integer field with one bounds check. Loop rotation rotates every loop in memory and writes the program once instead of once per loop. Bundling the optimizer's own test program takes 11 s instead of 88 s.
- The optimizer inlines direct calls to small procedures (at most 48 instructions that keep no stack storage, do not call their host and do not call themselves): it expands callees before their callers and copies each callee as expanded, so a helper brings its own inlined helpers and one that grew past the limit stays a call, growing a program by at most half and never past its work budget. Native and Wasm code then pay no call, fuel or frame cost for helpers such as Kryon's session lookups. A single-block callee such as `std/math.Sqrt` is copied into the caller's block in place, without new blocks or jumps, so n-body's inner loop keeps its shape. spectral-norm's `Spectral` no longer calls `A` inside its loops; it calls `Multiply`, which has `A` inlined.
- The optimizer reads a record member through the inserts that built it: reading a member an insert replaced gives the inserted value, and reading another member looks past the insert to the record underneath. A call whose result is unused goes when its procedure cannot be observed: no stores, host or indirect calls, possible traps or loops, and calls only to such procedures. A member read of a record joined from several branches moves into each branch, and a call to such a procedure whose result is then read only member by member is inlined for those reads, from callees up to four times the call limit, for up to three rounds after the program is folded. A Kryon widget's theme lookup becomes the constant colour or size it reads: the Button scenario calls `DefaultTheme` once instead of 17 times. Control-flow simplification that makes a jump's target the new entry block drops the old entry instead of leaving it jumping into the new one, and leaves an entry block with parameters in place. Jump threading sends an edge into a block that only jumps on to that jump's target, and an edge into a block that only branches, with a constant condition there, to the side it selects; a jump into such a branch becomes the branch. Each block whose only predecessor jumps to it then joins that predecessor. No edge moves onto a loop header, so loops keep the one entry and latch that hoisting and rotation need. The Kryon Button bundle drops from 7,255 blocks and 4,276 jumps to 4,169 and 1,175, and from 1.58 to 1.35 MB. Common-subexpression elimination follows the dominator tree (cmd/compiler_opt_dom.zi): arithmetic, comparisons and record reads repeated in a block that an earlier copy dominates reuse that copy; constants, conversions, addresses and byte comparisons stay reused within their own block, where the backend folds them into their uses. Copy propagation and jump threading run before inlining as well, so callees are measured as they will be after cleanup, and member-read inlining charges its growth to the program's size after each round's cleanup, for up to four rounds. Hoisting, induction-variable rewriting and loop rotation look for a loop only at a block some edge leads back to in a depth-first walk, instead of walking the whole function once for every block: a 3,000-block function takes about half a million units of optimizer work instead of 250 million, and Shovey's hosted app bundle no longer runs out of budget. `zx2wasm --unbounded` makes the Wasm module for a run with no step budget, as a native executable is: its `fuel` instructions charge nothing and it imports no `zx.budget`, which makes the Kryon widget scenarios about a quarter faster; the site's widget table measures Wasm that way, like the C and Go builds. A Wasm procedure reserves edge scratch only for a block an edge enters with two or more aggregates, so one taking a string or record argument no longer sets up an unused stack frame. A fixed-size Wasm copy of up to 320 bytes, such as a paint command record, is unrolled into loads and stores instead of calling memory.copy, which engines run out of line.
- A host can inspect a slice of records containing strings or nested slices under Wasm without requiring a copy back when its values are unchanged. Mutable byte slices still copy their changes back.

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
