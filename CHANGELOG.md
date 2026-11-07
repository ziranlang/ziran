# Changelog

## [Unreleased]

- The scalar proof kernel behind `#proof` is written in Ziran instead of C,
  with the same proof results, diagnostics and step budget.
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
