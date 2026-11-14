# Changelog

## [Unreleased]

- Document module initializers in the usage guide and lock their behavior with a test: a module-level variable whose initializer calls a procedure runs once before the entry on the C, Go and portable targets, importing a module is what includes it, and a module can register a procedure with another module that never imports it. A program without the import carries neither the module nor what only it reaches.

- Revision-28 lowering walks an `if` with its `else if` links as one chain that meets in a single join block, so a case list of any length lowers (a 1,500-case list used to exhaust the nesting budget). The portable interpreter gives freed heap storage back by following a link from each block to the one below it instead of scanning every block ever allocated, and a block keeps its record when it grows in place.

- Revision-28 lowering adds unions of integers and floats: a union is one unsigned integer as wide as its widest member, and a member is read and written by reinterpreting its low bytes.

- Revision-28 lowering adds `VecClone`, which fills a fresh destination with a copy whose capacity is the source's count. Seven procedures nothing called are removed from the portable interpreter, the executable reader and the native encoder.

- Revision-28 lowering adds `VecGet` and `VecPop`, which answer an `Option`: `VecGet` copies an element and gives nothing at or past the count, `VecPop` moves the last element out and shortens the vector.

- `ziran check` evaluates a law that reads a field of a record returned through another spelling of its type. A procedure declared `-> Pad.Hold` that forwards a `Hold` made in the imported module, or that returns an untyped `.{...}` literal, was "outside the compile-time evaluator"; the returned record is now matched by its declared type and the literal takes the return type.

- Revision-28 lowering adds `VecResize`: it reserves exactly the requested capacity like `VecReserve`, drops the elements it removes and zeroes the ones it adds. The portable interpreter finds a derived address through a hash index instead of scanning its whole address table, which made a pixel loop spend billions of work units; `image_resize_fallible` now runs on revision 28 with the semantic route's result.

- Revision-28 lowering adds `New(T)` and `free` on the heap instruction, `size_of` with the C layout size, negative folded constants such as a negative enum member, and a bare `null` passed, assigned or returned as a pointer. 60 of the 77 spec programs the semantic route runs now lower.

- Revision-28 host capabilities take and return more than scalars and plain records: a host value is now a bool, integer, float, opaque `*void` handle, or a record, fixed array or view (`string`, `[]T`) of host values, so a record that holds a string, a slice of strings and a handle all cross. `LirExecBindHostTyped` passes every non-scalar value as one host image, the value's bytes followed by the elements of each view, with view words holding an offset and a count; returned views become read-only instance storage and a malformed image fails the call. Lowering also adds `VecReserve` and signed literals in global initializers, gives every module the same `float32`, `float64` and `[N]T` lowered types so values pass between modules, and reads the zero value of a fixed array longer than 256 elements from a frame slot the procedure allocates on entry and never writes, instead of building it from one constant per element. All 89 built Kryon examples now lower to revision-28 ZX; they do not run on it yet.

- Revision-28 lowering adds fields read and written through a pointer, `VecSwap`, and record, string, float and fixed-array global initializers, which private initializer procedures store before module startup code runs. Native output names `assembly.gap.native-pointer-field` instead of trapping at run time on a record field reached through a pointer argument. 48 of the 75 `tests/spec` entries the semantic route runs now lower.

- Revision-28 lowering adds byte builders and owned `Text`: `BuilderAppend` reserves with the C runtime's policy before copying, `BuilderFinish` hands the bytes on, and `TextView` borrows a `Text` or `Vec(u8)` in place. Every module's spelling of `Vec(T)` is one LIR type. 39 of the 72 `tests/spec` entries the semantic route runs now lower.

- Revision-28 lowering handles owned values beyond a single vector local: moves follow the checker's move flag and leave the source empty, records and fixed arrays may hold vectors, vectors may hold vectors, and vectors travel as parameters and results. Drops recurse through fields and elements, and a fresh owned result nobody keeps is dropped at once. `zi2asm --run --heap-report` prints the heap blocks still live after the run; the parity test requires none. 34 of the 71 `tests/spec` entries the semantic route runs now lower.

- Revision-28 ZX gains heap storage and the first owned-vector lowering. LIR has one heap instruction, `resize(pointer, old_count, new_count)`; the portable interpreter keeps typed heap blocks whose addresses fail after a resize or free, and a request beyond the value-byte budget gives null. `Vec(T)` locals lower for elements that own no vectors: indexing, `count`, `capacity`, `VecSlice`, `VecPush` with the C runtime's growth, `VecClear`, `VecFree`, and drops at block close, return, break and continue. Moves, vector-bearing records, the Option-returning operations, builders and `Text` are named compile errors for now. Native and wasm output stop at `assembly.gap.native-heap` and `wasm.gap.instruction`.

- `zi2asm --emit wasm` lowers verified portable ZX to one wasm32 module, the third lowering of the same image beside the interpreter and native ELF. Source input and saved-ZX input give identical bytes (`examples/hello.zi` is 287 bytes), and a form without wasm code stops at a named `wasm.gap.*` diagnostic. Of the 37 programs in `tests/zx_lowered_parity.sh`, 36 run as wasm modules with the interpreter's output. `zi2wasm` and the bundle Wasm tier still use the ZIR-based lowerer.

- Revision-28 lowering covers more of the language: assignment through slice elements (`view[i] = x`, `view[i].field = x`), global procedure slots with typed null, mixed-signedness integer comparisons by value, and checked views of records, fixed arrays and procedure values. `zi2asm --run` binds platform byte output, so interpreted programs print. Of the 69 `tests/spec` entries the semantic bundle route runs, 32 now lower (was 20); `tests/zx_lowered_parity.sh` holds 37 programs to identical output on both routes. Native output names two more refusals instead of a bare encoder error: `assembly.gap.native-limit` and `assembly.gap.native-unsupported`, beside `assembly.gap.native-aggregate-view`.

- A saved ZIR file from another toolchain is no longer a bare "unsupported ZIR version". The reader names the file's version, its own path and the version it reads, and the `ziran` driver that started it (`ZIRAN_DRIVER`), and says to rebuild or reinstall the Ziran toolchain so every tool agrees.

- `ziran inspect` shows each statement's source once, as a trailing `//` comment, and each expression by what it means: a literal's value (`%0 string "Hello, World!\n"`) and a call's callee and arguments (`%1 call print(%0) -> void`). Expression text is printed only when no statement holds it.

- Docs, the website and `zi2asm` help call the saved checked program just ZIR, since ZIR is always written after type checking.

- Saved ZIR version 64 no longer repeats source text in every expression. An expression whose text appears in its statement saves the statement's distance, the length and a bias change instead, and reads back byte for byte; the writer keeps whichever form is shorter. `examples/hello.zi` saves its literal once (223 to 184 bytes), and `compiler_lowered_x64.zir` shrinks from 583,008 to 526,092 bytes. Kryon's 75-module widget graph shrinks by 18.8% and its widget bundles by 14–21%. Version 63 files are rejected; regenerate saved `.zir`. The published site artifacts were re-encoded to version 64 with identical listings and unchanged timings.

- The LIR optimizer folds `element.address` at constant index 0, `extract` of a freshly built aggregate and conversions of constants, removes dead values, prunes unreferenced types and interns duplicate source origins before ZX is written, so the portable interpreter and native lowering both receive the smaller program.

- Native regression tests remove their temporary `build/tests` work folders after failing runs as well as passing ones, and the private Chromium profile under `build/t` is removed when the browser closes. Pass an explicit work folder to keep test outputs.

- Unify the experimental direct pipeline around verified portable ZX: check and
  lower source, verify and optimize LIR, encode ZX, then read that same image for
  portable execution or native ELF generation. Remove standalone `.lirb`,
  `--emit exec` and `--input exec`; retain source/ZX LIR inspection and source
  optimization reports. Reduce direct-native hello to short constants and
  frame-free literal writes, use bounds-checked base/index token dispatch,
  omit fallthrough jumps, compact adjacent read-only data without file padding
  or an empty data section, fix function symbol extents, and omit inlined
  service declarations so static ELF files have no undefined symbols.
  Default semantic bundles and semantic-ZIR Wasm remain
  explicit cutover gaps; native debug origins remain absent in stripped ZX.

- Add deterministic register allocation for the experimental machine IR on Linux x86-64. RegAllocAllocate turns fixed and tied constraints into explicit moves, solves liveness, runs linear scan per register bank with splitting at calls, fixed registers and pressure, rewrites spills, reloads and parallel block-parameter copies, and lays out 16-byte-aligned frames. A separate MachineVerifyAllocated checks every value location, constraint, clobber, frame and fuel placement. Fuel checks now name the instance counter and clobber flags; a loop or callee without one fails allocation.

- Implement the experimental direct-native pipeline's first Linux x86-64 console ABI veneer: linked, verified portable ZX instructions lower directly to a static ELF with its own `_start`, Linux `write` syscall 1 through a partial-write/`EINTR` `write_all` loop, and `exit_group` syscall 231, with no libc, CRT or dynamic linker. Ziran-owned literal formatting writes strings and text views by exact byte length and formats booleans, every signed and unsigned integer width, and `%%`; floating-point formatting and unavailable target services fail with stable named gaps. Generated C/Go binary routes and portable host bindings remain separate and are never selected as fallback.

- Add the direct assembly target registry and preflight. `ziran capabilities --json` lists every CPU/OS row under `assembly_targets` as implemented, gap, excluded or unknown. No row is implemented yet: Linux x86-64 stays a gap until its encoder and native runtime cutover pass. `ziran capabilities --assembly-target=ROW[+FEATURE...] [--os-service=NAME]...` rejects an unimplemented row, CPU feature or OS service with `assembly.gap.target-unimplemented`, `assembly.gap.cpu-feature` or `assembly.gap.os-service` before reading source or saved IR.

- Calls through a module alias accept types that module re-exports with `using`. `Button.Draw` declared with `PropsModule.Props` takes a caller's `Button.Props` even when the caller's own `PropsModule` names another module; results map the same way.

- Go output compiles `VecSlice` and slice bounds of mixed integer widths, such as `s16` and `s32` indices.

- Fix full lowercase for ASCII A-Z in `std/unicode`; both allocating and caller-owned paths now preserve the existing Unicode conformance contract.

- Add portable native standard facades for byte text, environment access, timers, secure randomness, mapped files, standard streams, worker pools, directory trees and threads. Platform backends remain private implementation seams behind one import.

- Lower string literals, copies and `.count` in the experimental direct
  backend's portable path. Literal bytes are deduplicated read-only data
  that execution can never write. `bench/zx_compare.zi` now lowers to a
  revision-28 executable container.

- Lower `#foreign host_api` calls with scalar signatures to explicit host
  capabilities in the experimental direct backend. Revision-28 containers
  record the recomputed capability count, and the portable engine runs only
  after an embedder binds every capability and checks each host result.

- Add an opt-in revision-28 ZX executable container to the experimental
  direct backend. `zi2asm --emit zx` stores verified lowered instructions;
  `--input zx` inspects, re-emits or runs stripped files through the
  portable engine without source or `.zir`. Versions 26/27 are rejected
  with a rebuild diagnostic there; default `ziran`/`zi2zx` output is
  unchanged. Address-taken local scalars lower to fresh checked frame
  storage in the portable engine and native x86-64, and a verified
  target-neutral machine IR now precedes register allocation.

- Lower dynamic local fixed-array reads and assignments to checked executable
  instructions, including nested fields and indices evaluated once before
  the right side. The portable engine and direct x86-64 backend share the
  bounds and value-copy semantics.

- Extend the experimental direct assembly backend with checked global and
  fixed-array addressing, source-range inspection filters and executable
  identities. Saved lowered instructions now preserve source end positions.
- Add opt-in verified scalar optimization with deterministic transformation
  reports. Full ZX format replacement and the remaining assembly upgrade are
  still in progress; no performance improvement is claimed.
- Preserve assignment destinations and preceding compound-assignment values
  across side-effecting calls, and retain live nested portable-VM storage.

## [0.4.2] - 2026-11-11

- Add the experimental `make assembly` / `zi2asm` scalar direct Linux x86-64
  ELF path, verified lowered IR, inspection dumps and native source-range maps.
  Existing ZX and other backends remain unchanged; the full ASSEMBLY upgrade,
  optimizing allocator, runtime integration and other platforms remain pending.

- Portable ZX prepares short helpers and ordinary scalar record-field reads
  instead of repeatedly entering the expression evaluator for those operations.

- Run a source file directly with `zi2zx hello.zi`. Website examples now use
  the one-command `ziran run hello.zi` flow.

- Keep multiline quantified laws readable after `=>` and indent their continuations.

- Delete 263 redundant check scripts and their unused helpers. Local and CI
  runs execute the complete retained suite, prioritizing compiler semantics,
  memory safety, real backends, package integration and standard-library behavior.
- Test helpers and the check controller are authored in Ziran, including
  package graphs, diagnostics, native builds and browser
  interoperability. Image tests always compare independent reference pixels.
- Timed compiler kernels run explicitly from `bench/`, outside routine checks.
  Benchmark builds and measurements use Alpha and retain their actual host.
- The target page includes six JSON and UTF-8 workloads measured on Alpha,
  with downloadable compiler output, raw samples and unsupported-target diagnostics.
- JSON readers scan long unescaped strings with SIMD while preserving borrowed
  text, escape handling, error positions and secure cleanup. The retained
  desktop benchmark measures a substantially faster 1 MiB document parse.
- SIMD byte-vector loads read immutable strings directly on C, Go and portable
  ZX, with whole-access bounds checks and no intermediate byte allocation.
- Programs using SIMD across several modules keep all required operations when
  generating C, including parallel compilation.
- Standard-module release availability stays consistent in source archives and
  shallow Git checkouts.

## [0.4.1] - 2026-11-11

- Portable ZX prepares print formats, statement boundaries and collection-aware
  procedure plans, and caches native stack bounds safely per thread. Unchanged
  desktop loop bundles run about 14 times faster in repeated verified samples;
  records, text and arrays also improve without changing their output or budgets.
- The target page includes real Kryon Button, Text and Input scenarios with
  generated C, Go and portable ZX output, raw samples and complete checked IR.
  These desktop scenarios measure widget behavior and recorded paint commands.
- Targets remains available throughout the shared website navigation. Both
  matrix selectors share the same style, and checked IR has its own inspector
  instead of a timing column.
- Benchmark medians exclude anomalies using symmetric 1.5×IQR fences across
  targets and platforms, retaining all raw samples and exclusion indices.

- The target matrix compares warm execution on desktop, USB Android and web,
  with a platform selector, raw samples, timing ranges and named runtimes.
- WebAssembly handles omitted slice bounds and empty slices at the end of arrays.

- The website uses a text-free capybara mascot in its header, footer and browser tab.
- Fresh native builds avoid optimizing the temporary bootstrap compiler while
  keeping the final compiler tools fully optimized.
- Generated C includes only the headers it uses and omits empty source files
  for modules that only declare types.
- Inspect portable `.zx` bundles with `ziran inspect`, including their checked
  modules and hexadecimal bytes.
- Build WebAssembly with `ziran build --target=wasm`, including Hello World
  programs that print a string.

## [0.4.0] - 2026-11-10

- The website presents all four outputs in a target matrix: inspect C, Go,
  portable ZX, direct WebAssembly and checked IR, download generated files,
  run WebAssembly in your browser and compare measured target costs.

- `ziran run app.zx` discovers installed package runtime providers for host capabilities; console and graphical bundles use the same command.
- The website uses a simpler reading layout across the homepage, guides and API pages, with responsive navigation and the working browser playground.

- `pkg paths PACKAGE...` returns ordered resolved paths as JSON after one
  graph validation, sharing `pkg path` selectors and cache checks.

- Reused compiler builds refresh embedded runtime headers by content, so
  restored source timestamps cannot leave generated C using stale helpers.

- Linked functions keep their copied statement and expression capacities
  consistent with their storage, preventing heap corruption when later
  lowering appends nodes after dead-code pruning.

- Expression and initializer parsing build shared IR directly from Ziran,
  removing the C callback parser. Compile-time quoted searches share the
  Ziran source scanner.

- `#run` evaluates text of any length with builders, pointers, slices and
  folded loops, and iterates array literals directly.
- `#asm` template, operand and clobber strings may name compile-time constants,
  including templates generated in the same file. Local blocks retain these
  strings through rechecking and saved IR; ARM NEON helpers support callers
  built without NEON and with only 16 D registers.
- Effect-class derivation and `#law ... effect` evaluation run in Ziran
  instead of C, preserving law results and diagnostic details.
- `.Member` resolves against an enum imported from another module, as in
  `F(.Two)` for a `B.Kind` parameter, and an imported constant such as
  `B.LIMIT` sizes an array.
- Import linking, file-private renaming and the `#inline` self-call check
  run in Ziran instead of C, with the same diagnostics and checked names.
- The scalar proof kernel behind `#proof` is written in Ziran instead of C,
  with the same proof results, diagnostics and step budget.
- Theorems and their `#proof` certificates are parsed, prepared and checked
  by Ziran code instead of C, with the same results and diagnostics.
- The `.zx` container (header, capability, law and waiver tables, assets
  and their limits) is written, read and checked against its program by
  Ziran code instead of C; bundles keep the same bytes.
- Record fields and enums take `#c "SPELLING"` so a generated C header
  matches C code that shares the records: `const char *` or `char name[N]`
  fields, and enum constants with a prefix such as `ZIR_EXPR_`. Saved IR is
  version 63.
- The compiler's IR records (modules, functions, statements, expressions,
  types, laws and proofs) are defined once in Ziran; C includes the header
  generated from them.
- A record whose array size names a constant, such as `[NAME_MAX]u8`, keeps
  its fields' `#go_tag` and `#c` spellings; Go dropped the tags before.
- Ziran code reading or storing a pointer field spelled `#c "const char *"`
  gets the right type in generated C, so it compiles without a const or
  pointer-sign error.
- `#if`, `#ifx`, `#assert` and `#run` share one compile-time evaluator, and
  enum values read integer literals as the language does (`0b10_1`, `1_000`).
- `#asm` routines take slices, clobber ranges such as `xmm0..xmm7` and
  `q0..q1`, and one variant per line; the checker rejects undeclared or
  reserved registers, and `-DZIRAN_ASM_CHECK` runs every variant against
  the portable body. TinyCC assembles SSE2-SSE4.2 variants.
- `ziran run FILE.zi` runs `#asm` variants with TinyCC. Bundles keep them,
  but a bundle file always runs the portable bodies, so a downloaded bundle
  cannot run instructions of its own.
- `-DZIRAN_NO_HEADER_INLINE` keeps generated C calls on the external
  definitions instead of header inline copies, so link-time interposition
  such as `ld --wrap` sees every call again.
- A `#target` procedure's SSSE3, SSE4.1 and AVX2 copies call the same
  tier's copy of each `#inline` procedure of the module, which ran SSE2
  code or a call out of the tier's file before. The AV1 decoder decodes
  1080p frames 2-4% faster.
- A unary operator inside a binary one keeps its grouping: Go read
  `a ^ -b` as `(a ^ 0) - b`, and C read a float `-(-x)` as `--x`.
- A local such as `at := row * stride + column` declared before the guard
  that bounds `row`, `column` and `stride` proves reads such as
  `levels[at + stride]` in generated C, so they drop their bounds checks.
- Generated C skips the zero fill of a local declared without a value when
  every element is provably stored before anything reads it, such as a
  block filled row by row with `Simd.Store` in `for i: 0..7`.
- Generated C headers no longer define record and array constants as
  macros in Ziran spelling; uses carry the value, as in Go.
- Native SIMD vectors such as `s16xN` hold as many 128-bit blocks as the
  code running them handles: two in a `#target` procedure's AVX2 copy and
  in AVX2 builds, one elsewhere. Loops step by `Simd.Lanes(s16xN)`, and
  every width gives the same results.
- Generated C gets a copy of `ziran_simd_ops.h` holding only the SIMD
  helpers the program calls, with the same object code. TinyCC compiles
  the AV1 decoder's modules a third faster, which starts an uncached ZX
  `ziran run` about 230 ms sooner on 32-bit Android.
- Generated C leaves out bounds checks that an `else` branch or a constant
  left shift proves: `view[i - 1]` in the `else` of `if i == 0`, `t[i]`
  under `if i != 8` in `for i: 0..8`, and `bytes[(row << 4) + t]`.
- `ziran run` of a ZX bundle without a native cache starts about 18% sooner
  on 32-bit Android and 10% sooner on x86-64, with the same generated C.
- Generated C keeps the bounds check behind a guard on a sum or product
  that may wrap: `if first + 1 + second > view.count { return }` no longer
  removes the check of `view[first]`, through a local, an `ifx` choice or a
  loop limit either.
- Generated C leaves out bounds checks a guard on a parameter's field
  proves, such as `destination[i]` in `for i: 0..frame.count - 1` after
  `if destination.count != frame.count { return }`, until a call or a store
  through a pointer, slice or global could change the field.
- String constants and string globals longer than 4095 bytes build as strict
  C (GCC and Clang `-pedantic -Werror`, TinyCC). Header string constants
  are escaped like other C strings, so `??` text no longer forms a trigraph.
- std's libc bindings for `getcwd`, `readlink`, `read`, `write`, `poll`,
  `bcopy`, `mempcpy` and `curl_ws_recv` use `usize` and `isize` for size_t
  and ssize_t values, so they link and run on wasm32.
- `VecSlice` and `values[a:b]` can view a global Vec or a Vec field of a
  global record. While the view lives, the Vec cannot grow, be freed or be
  replaced, here or in a called procedure; the view cannot be returned or
  kept outside its block.
- `build --target=c --exe` links with Emscripten's `emcc`, keeping the
  `.wasm` file its launcher loads.
- Each `#load` file of a module can store and index the same `Vec` or
  `Option` of a module-private record.
- A public record whose fields hold module-private types builds in modules
  that import it: its C header now defines those types.
- A fresh Text from a procedure that only reads through a pointer argument,
  as in `AwardsText(*awards)`, is freed with its block, and so is one read
  by a local whose name another block also declares.
- A field of a local record or by-value parameter that holds a Vec or Text
  can be assigned: `shelf.label = TextCopy(name)` frees the old value, and a
  field moved out can be assigned again.
- Fixed a use-after-free in the checker when a record literal field
  initializer was a fresh Text.
- `Simd.Load`, `Simd.Gather` and a returned constant array read the shared
  table instead of copying it at every use, in C and Go, and `TABLE.count`
  no longer builds the table.
- `std/simd` in the portable VM (`zi2zx run --vm`) runs about a quarter
  faster: each call site works out its operation and operands once.
- Uncached `ziran run` of programs that use `std/simd` compiles in half the
  time: C that only names vector types includes their small types header,
  not every operation.
- The portable VM holds `std/simd` vectors of up to 128 bits inline in its
  values: a 352p AV1 decode under `zi2zx run --vm` takes 4.9 s, from 6.5 s.
- Under TinyCC on x86-64 (`ziran run`), `std/simd` operations run inline
  rather than as calls, and small records copy through registers: a cached
  1080p AV1 decode takes 503 ms, from 697 ms.
- Under TinyCC and unoptimized C, `std/simd` multiplies, rounding shifts,
  widening, narrowing and gathers run inline, and bounds checks and whole
  slices (`a[:]`) cost no call: AV1's vector self-guided filter decodes
  352p in 76 ms under a cached `ziran run`, from 95 ms.
- Go output computes each `std/simd` operation in its own typed code, which
  Go inlines, instead of through one general lane interpreter: a 352p AV1
  decode built as Go takes 44 ms, from 346 ms.
- Under TinyCC on every target, as on x86-64, `std/simd` operations run
  inline, and vector loads and stores copy in place rather than through
  memcpy. On 32-bit ARM, small records copy through registers and constants
  build with movw and movt: a cached 352p AV1 decode under `ziran run` on a
  Cortex-A53 phone takes 1.8 s, from 3.6 s.
- In C, locals, parameters and globals whose names begin with two
  underscores, which C reserves, are renamed as keywords are.
- Under TinyCC and unoptimized C builds, checked division, shifts and index
  reads are right when an operand is named like one of their temporaries,
  such as `zi__a`.
- zi2c writes the C for large programs far faster: it works out how each
  module's header includes its imports once, so Breeze's 921 modules lower in
  under a second instead of over two minutes, with the same output.
- Checking resolves constants through the symbol lookup cache, as it does
  types and procedures: Breeze checks about 0.25 s faster, AV1 about 15%.
  `ZIRAN_VERIFY_SYMBOL_LOOKUPS=1` verifies constant lookups too.
- `tests/compiler_kernel_benchmarks.sh` no longer stops when another
  commit lands during the run; it checks the tools, inputs and headers it
  built and measured.
- `forall` laws check in about half the time, with the same results: each
  law's condition is parsed once, a procedure's parameter list is split
  once, and a call made again with the same arguments in a later case reuses
  its result.
- The slice lifetime check finishes in a fraction of the time on large
  programs: each pass rechecks only procedures whose callees or globals
  changed, and callee summaries and recorded writes are looked up directly.
  `ZIRAN_PROFILE` now reports the slice lifetime and law gate checks as
  their own phases.
- `#reads` follows strings through field paths: a record parameter copied into
  a local record's field is followed to its later uses, a later `=` into the
  field replaces the string, and nested record fields are checked. Fresh Text
  in a `:=` record local that is only read is freed with its block.
- A local override whose own manifest gained, lost or changed dependencies
  since `ziran.lock` builds locally: its graph is resolved from the working
  manifests in memory, and the lock is left for `ziran lock`. Packages without
  an override and `--locked` builds keep the exact check.
- `ziran COMMAND --help` and `-h` print that command's usage and do nothing
  else; `ziran update --help` used to rewrite the lock.
- A local override checked out past its locked commit is validated against the
  graph its root dependency locks, so exact transitive pins of that package
  no longer fail with a dependency-count mismatch.
- `std/simd` vectors such as `u8x16`, `s16x8` and `f32x4`, and 256-bit forms
  such as `s16x16`, with operations that give the same bits in C (SSE2 to
  AVX2, NEON and WebAssembly SIMD128), Go, bundles and the portable VM.
- A `#target` procedure runs the best x86-64 SIMD instructions the CPU has,
  chosen once when the program runs. `ZIRAN_SIMD_TIER` lowers the choice,
  and `#asm` variants needing a vector feature beyond it are passed over.
- `std/simd` works however it is imported or aliased, and short standard
  imports such as `#import "simd"` also resolve through
  `--module-path std=DIR`, as package tests pass it.
- SIMD loads, stores, widening and 256-bit halves compile to single
  instructions instead of copies through memory.
- `std/simd` under `ziran run`, TinyCC and `ZIRAN_SIMD_SCALAR` runs about
  nine times faster: each operation works on its own lane type, and TinyCC
  on x86-64 uses SSE2 for the common integer operations.
- `Simd.Sign` and `Simd.AbsDiff`. 256-bit vectors run as two 128-bit halves
  without AVX2, and reductions, `MulWide`, `Narrow` and `AddPairs` use
  fewer instructions.
- On 32-bit ARM, `f32` vector arithmetic keeps tiny values instead of
  flushing them to zero, and byte shifts of 256-bit vectors are correct.
- `std/resample`'s kernels use `std/simd` instead of assembly. They make the
  same bytes and run faster: at least as fast with AVX2 and about 20% faster
  on large downscales, 3 to 10% faster with NEON on a Cortex-A53, and about
  twice as fast with WebAssembly SIMD128.
- On 32-bit ARM, Clang builds `Simd.LoadLow` as one load instead of copying
  the bytes through the stack.
- `Text` in `std/vec` is owned UTF-8 text that frees its bytes with its
  owner. `TextView(text)` borrows it, a `Text` passed where a `string` is
  expected lends the same view, and `TextCopy` copies a string into one.
- A procedure can return `Text` to callers written for a `string` result:
  they get a string as before, while `result: Text = Render(...)` frees it
  with `result`. `BuilderFinish` gives a `Text` where one is expected.
- Standard text producers such as `TextReplace`, `Float64Repr`, `JsonWrite`
  and `JoinPath` return `Text`. Existing callers still get a `string`;
  `json: Text = JsonWrite(...)` frees the text when `json` goes away.
- A `Text` result only read within its statement, as in
  `Count(JsonWrite(...))`, `Render(a) == Render(b)` or `.count`, is freed
  when its block closes instead of kept for the program.
- A local such as `line := Render(...)` that is only read, compared,
  counted or passed to a procedure that only reads it is freed when its
  block closes too.
- Such a result passed through an `ifx` arm, a local, a procedure that
  returns it, or a record literal field such as `Props.{.text = Render()}`
  whose callee only reads that field is freed the same way.
- Such a result handed to std/format's `Append` or `BuilderPrint`, whose
  first argument is the builder's address, is freed too.
- A callee that copies its string parameters into a local `[N]string` and
  only reads the elements, or that points a `string` local at other views
  of them with `=`, as in `v = s[1:3]`, only reads them.
- A procedure type marked `#reads` after its result, as in
  `Painter :: #type (label: string) -> s32 #reads;`, only holds procedures
  that read their strings, so Text passed through it is freed too. Saved IR
  is format 62.
- Text read in an `else if` condition, after `&&` or `||`, or in an `ifx`
  that gives a number or `bool` is freed when its block closes.
- A global holding a `Vec` or `Text`, alone or in a record, can be assigned,
  which frees its old value, and moved into a local or returned, which
  leaves it empty.
- A file that names `Text` or `Vec(T)` imports `std/vec` automatically, so
  its C header, bundle and Go code see the same types as the code that made
  them. A parameter or return type that names no visible type is an error.
- `#reads` before `#foreign` marks a foreign procedure that only reads its
  string and byte arguments, so Text passed to it is freed too.
- A `-1` or a global argument before such a result no longer keeps it,
  unless the producing call may change a global.
- A local named like a C library function, such as `free`, no longer hides
  it in generated C, and one named like a Go builtin no longer hides it in
  Go.
- `make install-user` keeps a `~/.local/bin/ziran` that launches another
  tool, such as a build queue wrapper, and says so instead of failing.
- `for h.items`, `for * h.parts`, `for it.tags` and `h.items[1:]` work on a
  Vec record field, and a record field holding Vecs moves out, as in
  `last := popped.value`. Pushing to or freeing a Vec while a loop over it
  runs is rejected, also through a field or a called procedure.
- Go output of `for v` over a local Vec compiles again.
- A constant table read in several loops of one function, or through
  `.count`, no longer fails with "cannot bind aggregate constant".
- Integer literals such as `1_000`, `0xDEAD_BEEF`, `0b1010_0101` and `u64`
  values above the `s64` maximum work in globals, `::` constants, constant
  tables, array sizes and range bounds, and pass the portable VM's checks.
- `x := x + 5` in an inner block, and `i := ...` in a `for` body over `i`,
  read the outer binding on every target; generated C no longer reads the
  new local, and the VM no longer steps the shadow instead of the loop.
- Generated C leaves out bounds checks it proves always pass, such as
  `t[i]` in `for i: 0..7` over a `[8]T` array, `view[i]` in
  `for i: 0..view.count-1`, `table[x & 255]`, and `bytes[at]` after
  `line := row * 16` and `at := line + (offset & 15)`.
- `#inline` after a procedure's result always inlines it in generated C,
  and `#cold` keeps a slow path out of line, including other modules' calls.
- A procedure returning an array copies it to the caller once in generated
  C, and `return Make(n)` writes the caller's array directly.
- A local array passed by value to a Ziran procedure is copied once in
  generated C, not by both the caller and the callee.
- Generated Go shifts by a variable count without the general integer
  helper, and copies a constant table straight from its shared `var`.
- A file-scope variable can start from a constant table of any length,
  such as `table: [4096]u8 = BIG`, instead of failing past 4 KiB.
- Constant tables can be nested, as in
  `GRID :: [3]s32.[s32.[1, 2, 3], s32.[4, 5, 6]]` or a `#run` procedure
  that fills `g[a][b]`, and `GRID[i][j]` reads the table without a copy.
- Generated C also leaves out bounds checks an earlier guard proves, such as
  `input[row * stride + column]` in loops over `row` and `column` after an
  `if` that returns when `input.count` is too small.
- Those checks stay out when the loop body has an `if` that continues or
  returns, such as `if flag > 3 { continue }`, or an enclosing `if i < 32`,
  and for rows `stride` apart after a guard that `stride >= width`.
- They also stay out for a row or column a flip picks with `ifx`, and after
  a guard written as `cast(s64)(height - 1) * stride + width`.
- A long `||` guard with comparisons no index uses still proves the reads,
  and so does a filter window that starts `radius` rows and columns back.
- `ziran run` spends its step budget more cheaply: each procedure finds its
  budget once instead of at every loop iteration, so an AV1 decode runs
  about 7% faster.
- Code built by TinyCC (`ziran run`) or by GCC and Clang without
  optimization checks indexes, shifts and divisions in place instead of
  calling a helper, so an AV1 decode runs about 17% faster under `ziran run`
  and 24% faster at `-O0`. Optimized builds are unchanged.
- `ziran run` charges a loop over literal bounds, such as `for i: 0..7`, its
  whole step count once as it starts rather than at every iteration.
- Generated C emits a call of a small procedure of the same module as that
  procedure's body, so `ziran run` (TinyCC) and unoptimized builds skip the
  call: an AV1 decode runs about 7% faster under `ziran run` and 4% faster at
  `-O0`. A private procedure every caller inlined is no C warning.
- Generated C builds without warnings under GCC and Clang `-Wall -Wextra`:
  an `ifx` comparison gets one pair of parentheses, a local nothing reads is
  cast to void, and a procedure named like `ReadSignedBits` no longer pulls
  in an unused helper.
- Generated C builds the SIMD tier copies of a large `#target` procedure in
  their own files, such as `inverse_transform.avx2.c`, so a parallel C build
  compiles them at the same time. An AV1 decoder builds at `-O2` about 40%
  faster with 8 jobs.
- A range over a computed bound, such as `for i: 0..count`, tests the bound
  once as it starts and then leaves only at its last value. Generated C has
  one loop exit, which GCC can vectorize, and an AV1 decode runs about 2%
  faster at `-O2`.
- Generated C writes an array call result straight into the local or array
  lvalue it initializes or is assigned to, without a temporary and a copy.
- Small public procedures are inlinable across modules in generated C
  without LTO: their header carries a `static inline` copy for other modules.
- Generated C divides and shifts by a nonconstant operand through a checked
  helper of its width, and shifts signed values right by a constant with one
  arithmetic shift, keeping the zero-divisor and shift-count aborts.
- Constant number and boolean arrays of any length (`TABLE :: s32.[...]`, also
  through `#run`) are read-only tables: `TABLE[i]` reads one shared copy on C,
  Go and the VM instead of copying the array at every read.
- `clz`, `ctz`, `popcount` and `bswap` builtins for `u8`, `u16`, `u32` and
  `u64`, with the width as the count for zero, on C, Go and the VM.
- A Vec can hold Vecs and records that own Vecs: `VecPush` moves the element
  in, `groups[i].parts[j]` reads and writes in place, and dropping frees the
  inner Vecs. A discarded owned record result, such as `VecPop`'s, is freed.
- A local override that no longer matches `ziran.lock` now names the package, its source and the mismatched pin instead of "cannot fetch locked packages"; `GIT_TRACE` no longer makes a clean package cache look modified.
- `std/thread_linux` links and polls on macOS, which has no `pthread_tryjoin_np`.
- Array literals have no size cap and check in linear time; saved IR is
  bounded only by its 256 MiB read budget. Returning a view of a pointer
  parameter's own array storage is rejected; pass that storage as a slice.
- Native C keeps an array literal of constant numbers or booleans as one
  static table instead of a store per element, so large tables compile fast.
- `ziran edit` previews and applies checked source edits; `ziran compact` and
  `ziran expand` give `.zi` files an optional lossless compact transport.
- JSON diagnostics keep each message within 2048 bytes and report cuts in a
  `truncation` field.
- `std/unicode` adds Unicode 17 `NFCInto`, `LowerInto` and exact letter,
  number and white-space properties.
- `std/json_document` owns its bytes per node, adds `JsonReset`, caller-owned
  `Into` accessors and erasers that wipe private buffers.
- `std/file_tree_linux` adds `FileSnapshot` and `SameFileSnapshot` to tell
  whether a file was changed or replaced.
- `std/process_channel_linux` accepts up to 32 arguments and can discard
  stderr; `std/socket_linux` adds `SocketCloseWrite`.
- Curl HTTP accepts named header values up to 8191 bytes.
- `std/image` adds fallible caller-owned resampling and bicubic resize;
  `std/float_text` adds `Float64ReprInto`.
- `std/resample` gives programs Pillow's resize as allocation-free row
  kernels, with SSE2, AVX2, NEON (AArch64 and ARMv7) and WebAssembly SIMD128
  versions that make the same bytes.
- `#asm` adds `arm+neon` for ARMv7, selected at run time, and
  `wasm32+simd128` for WebAssembly builds made with `-msimd128`.

- Native C generation accepts checked pointer casts to imported C callback types;
  Go and portable targets continue to reject C callbacks.

- Package overrides selecting one locked revision now share its dependency graph
  across older transitive revisions, including newly added dependencies.

- Without a project, `std/NAME` always loads the standard library's module, even
  when a search path or the importing package has its own module named NAME.

- Portable programs use prepared function and global indices for calls and reads, reducing repeated-call execution time.
### Added
- Owned vectors support fallible reserve, resize and capacity-bounded resize with
  matching C, Go and portable execution, initialized defaults and safe owned-element cleanup.
- `std/web.WebBlobBytes` snapshots file or media bytes into a browser Blob
  without first making a separate typed-array copy. The snapshot remains valid
  after source changes or Wasm memory growth.
- Project commands such as `ziran bundle --project` accept `--module-path DIR`
  for modules a build step generates. They join the project's own modules;
  dependencies still resolve only from the lock and never see them.
- `--exe --release` builds with link-time optimization across modules.
- `#parallel_gpu for` runs on the fastest GPU in native Linux and Android builds,
  falling back to CPU threads.
- Python build tools can consume verified dependency artifacts and ordered
  native link metadata through the toolchain's shared artifact helper.
- Dependency-owned native artifact recipes build and reuse declared output sets
  through the locked package graph, with explicit platform/ABI identities,
  resolved options, input validation and atomic publication.
- `std/utf8` gained bounded `Copy`, which preserves valid bytes, repairs
  malformed input with U+FFFD, never splits an output sequence, and reports
  the written count and completion separately.
- `std/text_tools` gained `TextHexDecodeInto`, which appends bounded
  uppercase or lowercase hexadecimal bytes and leaves its output unchanged on
  every failure.
- Linux and Android hosts on x86-64, ARM and ARM64 run portable bundles as native code, compiled in memory and cached between launches.
- Natively run bundles execute `#parallel for` regions on worker threads, one per processor.
- `#parallel for` bodies can hand out results by writing their own slot of an array or slice the function owns, such as `out[it] = value`, or their own window of a slice, such as the row `pixels[it * width : it * width + width]`.
- The new `zi2wasm` tool lowers a checked program's scalar subset to one wasm32 module: the portable control flow, calls, casts and wrapped integer widths, with a step-budget import matching the native tier's counting. Programs outside the subset fail with a `zx.wasm` diagnostic, and `tests/zx_wasm.sh` compares the module's result with the native tier and the portable VM under node (and wasmtime when installed).
- The wasm execution tier and `zi2wasm` lower strings, slices and module globals: string values are i64 handles over interned data-segment literals with byte indexing, `.count` and equality through a generated compare helper; slices borrow their base's storage as (length, address) handles with bounds-checked element views; globals take fixed data-segment slots whose literal bytes reload fresh on every instantiation. Pointers cross as opaque handles. Probing Inner Breeze's real bundles now passes strings, slices and pointer signatures, leaving enums and expression-initialized globals as the remaining rejections.
- The wasm execution tier and `zi2wasm` lower records and fixed arrays: fields laid out exactly as the checker sizes them, values as arena slots with lexical region lifetimes, literals, member and index places with bounds checks, value copies through calls and returns, and parameter slots the callee owns.
- `BundleInstantiate` gained a WebAssembly execution tier behind a provider seam (`ZxWasmSetPlatform`): with a provider installed, bundles in the scalar subset run as WebAssembly lowered from their verified IR at open time, and everything else falls back to the portable VM; without a provider, behavior is unchanged. Native TinyCC hosts are untouched. `tests/zx_wasm_engine_test.c` checks the seam, handoff and fallback natively, and `tests/zx_wasm_web.sh` runs a bundle end to end through the wasm-built library and a JavaScript provider (skipped without emcc and node).
- `std/vorbis` reads an in-memory Ogg Vorbis stream's channels, rate and length from its pages, without decoding or a library, so programs can bound a stream before decoding it.
- `std/vorbis_linux` decodes whole in-memory Ogg Vorbis streams to 16-bit PCM over the platform libvorbisfile, with a frame limit and strict refusal of truncated, chained or damaged streams. Only programs that import it link the library.

### Removed
- The Python target: `zi2py`, `--target=py`, `py:` imports and the `std/*_py`
  modules. Run native scripts with `ziran run --target=c`.
- The separate `zi2zx-vm` runner. `zi2zx run --vm FILE` runs a bundle on the
  portable VM instead.

### Changed
- Native `--exe` builds default to `-O3`, and lowering a ZX bundle to C is
  faster because read-only field walks no longer copy each field.
- Compiler commands run at low CPU and disk priority (`ZIRAN_NICE`) and use at most half of the CPUs, bounded by free memory (`ZIRAN_JOBS` lowers it). `zi2c` writes C on worker processes, and the lifetime check no longer rescans every global and function, which halves Breeze's front-end time. `--prune-stale` also cleans subdirectories.
- Native builds run `#parallel for` regions on one thread per processor instead of four; `ZIRAN_PAR_THREADS` still chooses.
- Saved IR (format 59) writes each string once and refers back to it, stores numbers in as few bytes as they need, and stores source positions relative to the previous one, so `.zir` files and `.zx` bundles carry about a third of their former code size at the same open cost. Rebuild format 58 IR and bundles from source.

### Fixed
- The later lines of a string that spans several lines keep their text: `int`
  and `float` there became `s64` and `float32`, and assembly such as
  `vmov.i32` was rejected as an old type spelling.
- The standard vector reference includes capacity operations, and their portable
  regression test links correctly with an instrumented compiler library.
- Portable record globals correctly initialize fields from open-import constants,
  including imports used only by initializers.
- Keep caching native script runs when the compiler binary is larger than 16 MiB.
- `ziran lock` and `ziran update` resolve a project that depends on a package and also pins the same repository commit as a source checkout, such as a game and its artwork. The source checkout gets its own lock entry instead of failing with "cannot update project lock"; existing locks still verify.
- Android applications report portable execution errors in the system log, so failures can be diagnosed when no terminal is attached.
- Artifact recipes accept up to 256 inputs, so a provider with a long source
  list no longer fails with "unknown or invalid artifact recipe".
- Add-ons can use records, functions and globals that a shared library did not
  retain, even when both use the same module. Only such add-ons need portable
  bundle format 27; every other bundle is written as format 26, so hosts built
  before 27 still open it.
- Native C builds keep globals and constants separate from foreign library symbols, so a variable such as `open` cannot replace the system file-opening function and crash the program.
- `ziran lock` and `ziran update` lock one package when manifests name the same commit by a branch and by that commit; they failed with "cannot update project lock". A full-commit ref now matches a package locked at that commit.
- Bundles accept arrays of fixed arrays as function and host capability parameters.
- `#parallel for` regions reject mutating, foreign and procedure-value calls nested inside larger expressions.
- Native parallel workers see every binding their body reads, including parameters and fixed arrays.
- `#parallel for` bodies can write into arrays they declare themselves.
- Saved IR keeps the result type of a call whose argument is another call with widened arguments, such as `!Open(board, Index(x, y))`, so builds from it no longer fail with "saved IR does not match the checked program".
- `#parallel for` regions reject handing a shared slice to a callee that writes through it, directly or by passing it on.
- Natively run bundles reach a bound Ziran provider exported under another symbol.
- Go output converts a conditional's arms to its type, such as a float32 arm of a float64 conditional.
- `StartProcess` and `RunProcess` open input, output and error files relative to the caller's working directory, before the child changes into `directory`.

## [0.3.1] - 2026-10-23
### Fixed
- Source generation now preserves output errors on hosts that use file-backed text streams.
- Website release links now follow the installed compiler version.

## [0.3.0] - 2026-10-23
### Changed
- Portable executables and libraries now use `.zx`, the `zi2zx` tool, and the `zx` target. Source stays `.zi` and checked intermediate modules stay `.zir`. Rebuild previous `.zib` programs; renaming a file does not convert it.

### Fixed
- Go output no longer needs temporary filesystem space and reports output write failures instead of producing incomplete source files.

## [0.2.0] - 2026-09-28
### Added
- `ziran version` (and `--version`) prints the compiler version.
- Release tags now follow the compiler, so `ref = "v0.2.0"` in `ziran.toml` pins a release.

### Changed
- A clean build of the compiler is much faster and uses at most eight cores.

### Fixed
- Building `ziran` from scratch on older `make` versions no longer fails to link.

Earlier versions were tagged `v0.1.x` without a changelog.
