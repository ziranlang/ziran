# Changelog

## [Unreleased]

- Omit unused native C range indices when a nested loop shadows the index,
  while retaining reads of the outer index before and after the nested scope.

- Allow request-local native HTTP TLS context configuration and CA files. Keep custom TLS connections private so borrowed callback state and trust settings cannot leak into pooled requests.

- Preserve imported record identities when borrowing or indexing vectors through
  another module, including native TSX slices of aliased record types.
- Keep editor positional-read offsets 64-bit on native and WebAssembly targets,
  and select the matching `pread64` ABI on Android.

- Release the owned intermediate buffer after capped file appends while preserving whole-file ownership and rollback behavior. Verify repeated successful reads and refused oversized reads under address, undefined-behavior and leak sanitizers.

- Expose borrowed response-header observers and encoded-body control for bounded native HTTP transfers. Preserve decoded transfer defaults, direct proxy/redirect behavior and pooled-handle reuse; include the retained native HTTP fixture in the regular checks.

- Keep capturing callbacks with slice parameters canonical through checked IR
  serialization and rechecking. Preserve generic type visibility when adding a
  type application grows and relocates the module type array.

- Preserve a native text view's checked argument position, name and source
  location when removing an identity conversion inside a call.
- Fingerprint populated Git submodule directories by their actual working
  contents when child Git metadata is absent, and include initialized child
  working edits in native artifact invalidation.

- Add optional 64-bit Linux `std/decimal_linux` arithmetic using mpdecimal 4,
  with arena-owned coefficients, exact binary64 construction, explicit contexts,
  rounding/status/trap results and owned formatting output. Programs without
  this import keep no mpdecimal dependency.

- Add full default Unicode 17 uppercase mappings to `std/unicode.Upper`,
  including multi-scalar expansions, with source/saved-IR C, Go and portable
  conformance coverage generated from the official Unicode tables.

- Validate generated callback environments after signature normalization and in
  their declaration's module context. Preserve checked scoped borrows when
  linking or loading IR, including callbacks that update caller-owned views.

- Include native C module headers by their checked output filename, keeping
  private file-import identities independent of authored source filenames.

- Keep native pointer-range loans on their pointee rather than the record or
  array storing a raw pointer. Preserve checked address aliases and returned
  backing paths without borrowing mutable caches of foreign buffer addresses.
  Retain views written through checked aliases in their actual owner, and keep
  unrelated native handles out of global-view cleanup proofs. Copy record-field
  view origins independently of the alias's address loan.

- Keep project source entry identities separate from declared package modules.
  Preserve authored entry spellings and checked IR filenames, disambiguate actual
  output collisions, and retain relative saved imports independently of identity.
  Preserve internal open-import aliases when saving those locations so checking
  saved modules does not create duplicate aliases.

- Share native assets, clock, process and file services between portable-program
  hosts through `std/guest_host`, with reply storage owned by each instance.
  Expose typed process identity, accounting units, signals and shell execution
  through `std/guest_process`. Reject embedded NUL in shell commands and reclaim
  temporary file-read storage after copying host replies.
  Reuse the native civil-clock provider for wall milliseconds, including its
  checked platform-specific timestamp layout instead of a duplicate fixed-width ABI.
  Bind the exact standard guest module identities emitted by standalone and
  checked project builds through the same native capability dispatch.

- Save entry-selected checked IR without native-only folding or record-field
  pruning. Preserve canonical expression graphs and host providers so direct
  source and saved-IR portable bundles agree.

- Preserve the exact backing field of returned native views. End a global view's
  loan only when every relevant path clears that owner before mutating its
  storage, including named nested fields. Retain independent aliases and local
  views through calls and joins; share read-only intrinsic effects with the
  ordinary mutation checker. Recursive cleanup that cannot write the backing
  preserves safety without establishing a clear.
  Track retaining fields through nested calls and typed owner-clearing helpers,
  including forward declarations, while keeping opaque callbacks, pointer
  rebinding and interior pointers conservative.

- Preserve named string and real constants in compound global initializers
  through generated C/Go, portable and direct native output.

- Combine the positive text-mutation fixtures and remove thirteen redundant
  check or execution commands from the text-mutation and portable-global checks.
  Keep source/saved-IR byte comparisons and unique backend and instance-lifecycle
  assertions in the existing checks.

- Check explicitly bounded native pointer slices and preserve their caller's
  backing storage through pointer-returning helpers. Reject expired views and
  writes through string aliases. Preserve zero-copy Go text views and real
  portable/Go literal terminators without changing the visible string length.

- Preserve native C linkage for package-private implementations used by another
  module in the same package. Keep their types, constants and generated default
  helpers available internally while rejecting external source access. Keep
  generic specialization linkage consistent with strict checked-IR metadata.

- Allow checked Scoped callbacks to update borrowed models and read captured
  views for the invocation. Preserve immutable owned captures and reject escaping
  views. Inline array and record copies retain their own backing storage. Track
  mutually exclusive branches without dropping loans at their join.

- Support native pointers to fixed arrays in locals, records, parameters,
  returns, callable slots and collections across C, Go and portable output.

- Fingerprint artifact dependencies from their working bytes without creating
  a Git index or changing the shared index. Preserve content identity when a
  concurrent commit records unchanged bytes; report unreadable input explicitly.

- Add checked native record initialization with `nativeInit<T>` and explicit
  borrowed slice types with `NativeSlice<T>`. Lower TSX `BuilderPrint` through
  the shared native formatting rules, preserving explicit integer widths and
  authored diagnostics through source and saved-IR execution.

- Add `#scope_package` for native implementation boundaries. Preserve the
  package owner and declaration visibility in checked IR format 71, and
  project editor declarations for the requesting package without exposing
  another package's private names. Rebuild saved IR with the current compiler.

- Generate TypeScript editor declarations from the real checked native module
  graph with `ziran api --typescript --out build/.ziran/types --project`.
  Preserve resolved import spellings, generic records, native scalar boundaries,
  enum values and callable signatures; publish the resolution manifest last and
  invalidate it after a failed check. Unsupported native projections report an
  error. Whole-application editor coverage remains in progress.

- Use TypeScript syntax for checked native references (`NativeRef<T>`,
  `nativeRef`, `nativeDeref`), borrowed ranges (`nativeSlice`) and caller-location
  defaults (`nativeCallerLocation()`). Reject the former Ziran spellings in
  TS/TSX sources. Add native integer bitwise operators with standard precedence,
  explicit wide-index/UTF-8 reads (`nativeAt`) and storage writes (`nativeSet`).

- Initialize native model/TSX view import cycles once when the intervening
  modules have no runtime initializers. Share startup dependencies across C,
  Go and portable lowering; retain errors for actual initializer cycles.

- Avoid an extra register copy for x86-64 immediate integer multiplication
  when its input remains live. Keep the existing power-of-two and factor-31
  specializations; matched Alpha trials improve indexed record updates by
  about 2%, with the wider standard workload mean unchanged.

- Declare cyclic native/TSX interface types before C header includes, and keep
  inline bodies behind the same complete-type rules as their signatures.
  Preserve contextual scoped callbacks and captured slice indexing in C and Go.

- Resolve imported native foreign procedures through ordinary typed module
  exports. Preserve host capability identity, checked signatures and bindings
  across TSX aliases and saved IR, including unbound native host linker symbols;
  rebuild checked IR with format 70.

- Share C compiler annotations in `ziran_compiler.h` instead of repeating
  portability macros throughout generated sources and inline headers.

- Resolve imported global record and callable types in their declaration module,
  including pointer, array and generic storage. Keep TSX loop identities and
  continue steps local to each function and nested callback.

- Add checked defaults to named TypeScript functions and preserve authored
  caller columns through native and saved IR, including JSX component calls.

- Check borrowed callbacks and views through deeply nested and recursive record
  declarations without a depth cutoff. Keep bound native provider names when
  saved modules are emitted as separate C programs, including a TSX `main`
  provider alongside the process entry.

- Add checked stack-borrowed `Scoped<Signature>` native callbacks and defer JSX
  children according to their declared props type. Keep aggregate child values,
  nested container ordering and authored source locations through saved IR.
  TS interfaces require all declared fields; native references use the checked
  `nativeRef` and `nativeDeref` intrinsics.

- Keep owned callable ABIs tied to their checked signature declarations across
  import aliases and linked builds. Preserve nominal generic arguments and
  lexical helper dependencies in saved and entry-linked IR. Native TSX module
  `let` bindings use typed storage and initialize once in source order.
  Portable component state exports include public slots only; private runtime
  storage stays local to its instance.

- Preserve decimal exponents across native floating-point formatting trials,
  and keep full normal and subnormal float output in portable and hosted runs.

- Preserve generic procedures' defining-module helper calls through package
  re-exports. Checked IR supports explicit `--retain module:function` native
  host roots through the same root selection as portable bundles.

- Capture native TSX actions in move-only `Owned<Signature>` callables with
  immutable owned snapshots, checked borrow lifetimes and deterministic cleanup
  through source and saved IR on native and portable targets.

- Select packages by exact lock identity through `pkg path` and `pkg paths`,
  including transitive dependencies with the same manifest name.

- Serialize package-triggered builds of a pinned compiler. Concurrent project
  commands wait for the same complete build instead of racing generated files
  or starting a launcher while its companion tools are still being built.

- Native script runs select dependency libraries from the modules they include,
  matching native builds. Unused optional codecs no longer become link
  requirements, and script caches track the dependency link metadata.

- Preserve architecture-specific assembly in ZX revision 29 through source and
  saved-IR builds. Native ZX selects compatible templates by architecture,
  OS, ABI and usable CPU features, with the portable body as the default.
  Explicit target-only procedures verify their requirements before initialization.

- Accept `.ts` and `.tsx` sources directly in the compiler and package tools
  for the supported native TypeScript subset, replacing the `tsx2zi` prototype.
  Native generic record types share Ziran's checked specializations. Collection
  `for...of` evaluates its source once, borrows named storage and releases a
  fresh owned collection on exit through the ordinary checked IR.
  Generic applications preserve declaring module and private file identity,
  including nominal arguments, while aliases of one declaration agree.

- Generate a checked C process entry with `--emit-main --entry module:function`
  without invoking a C compiler, for native hosts and browser build systems
  consuming ordinary source or saved IR.

- Keep small loaded records in scalar registers through member updates,
  stores and block transfers on native targets, avoiding temporary record
  copies while preserving snapshots across writes and calls. Checked memory
  accesses retain their aggregate validation. Indexed array reads and writes
  into parent aggregates use the same scalar path.

- Install the `zi2c` launcher on PATH so native compiler tooling can find it.

- Expire native script builds after 24 hours without use, cleaning up after
  scripts finish at most once per hour while preserving active builds.

- Bind generated C foreign declarations directly to their linker symbols,
  using readable module names; remove forwarding wrappers and per-binding guards.
  Keep package namespaces readable, including names such as `kryon__system_theme_linux`.

- Prune branches guarded by unregistered callbacks and unwritten boolean globals
  in closed native entry links, then discard empty hooks and constant-result
  calls when their arguments have no effects. Keep separate generated module
  files and preserve live registration and escaped addresses.
  Finalize record layouts and validate the surviving graph once pruning stabilizes,
  instead of repeating whole-program checking after each shrinking pass.

- Name generated callbacks after their owning procedure and specializations
  after the original procedure and concrete type. Qualify colliding record
  names with their module; retain identity suffixes where required.

- Marshal Windows x86-64 host record arguments and return values using the
  Win64 C ABI, including padding, floats, nested arrays, hidden return
  pointers and scalar arguments that spill to the stack. Direct native GUI
  executables can now call full frame-input and text host services.

- Match host record types from dependency aliases and readable package identities, preserving exact module and field checks.

- Name dependency modules and generated symbols after their import aliases instead of repository addresses and revision hashes. Conflicting pins under one name require an explicit dependency alias.

## [0.9.0] - 2026-11-25

- `zx2native` on Linux x86-64 now implements the full System V AMD64 C ABI for registered host-library veneers: arbitrary records (including mixed float/integer/padded structs such as `Rectangle`, `Color`, `Vector2`, `BundleInput`, `TextRequest`), stack-spilled arguments past six integer / eight SSE registers, and record return values (both <=16-byte register returns and >16-byte `sret` hidden-pointer returns) marshal between Ziran's packed record layout and C's naturally aligned struct layout. This lets native executables produced from portable `.zx` bundles link full platform UI/SDL/GPU host libraries directly instead of hitting `assembly.gap.machine-host-abi`. `tests/machine_elf.sh` covers `Color`/`Rectangle`/stack-spilled calls, `string` returns, and 14-field padded struct returns.
- Setting `ZIRAN_JIT_DUMP` to an absolute directory makes the in-process native loader publish each instance's loaded code as a JITDUMP file there (`jit-<pid>-<sequence>.dump`) before it runs: one JIT_CODE_LOAD record per program function, with its loaded extent, so perf's JIT interface attributes samples to function names. A run without the variable writes no file. `tests/native_load.sh` checks the header, the records and the named function.
- A host library registered for `zx2native` may be an `ar(1)` archive of ELF64 x86-64 relocatable objects instead of a shared object. The Linux executable links the members its capabilities reach, each once, with their text, read-only data, writable data, zero fill, GOT entries and relocations, and the capability's veneer calls its member directly, so the executable stays static and has no interpreter or dynamic record. An unsupported section, relocation or symbol is the stable `assembly.gap.machine-archive` diagnostic naming the capability, member and symbol, and the old output is kept. `tests/machine_elf.sh` links and runs an archive, rejects an undefined member symbol, and keeps the shared-library case green.
- `zx2native --platform linux|windows` supplies the ZX Runner's file and clock services itself, so a program that uses `std/guest_files` or `std/guest_clock` links into the same static ELF or PE32+ as any other instead of stopping at a host-capability gap. The linker names the nine `host_api` services by function and signature, keeps them apart from library imports so the ELF stays static and the PE imports only what a veneer calls, and emits each veneer after the library ones. Linux veneers make the system calls with the path in a 4 KiB stack slot; Windows veneers widen UTF-8 paths to UTF-16 with `MultiByteToWideChar`, convert listed names back with `WideCharToMultiByte`, and import `GetFileAttributesExW`, `CreateFileW`, `ReadFile`, `WriteFile`, `DeleteFileW`, `MoveFileExW`, `CreateDirectoryW`, `FindFirstFileW`, `FindNextFileW`, `FindClose`, `QueryPerformanceCounter`, `QueryPerformanceFrequency` and `GetSystemTimeAsFileTime` as needed. A read or a listing grows a heap block through the runtime's resize entry and shrinks it to the final length; the block stays allocated. `tests/machine_elf.sh` and `tests/zx_windows_parity_cases.zi` run one program that writes, reads, renames and removes a file (with a non-ASCII name), lists a directory and reads both clocks on Linux and under Wine.
- `zx2native --platform windows` writes each line of standard output to a console once it ends, as the Linux ELF does, while a redirected or piped descriptor keeps whole buffers: the buffered runtime asks `GetConsoleMode` once, so an image with more than one console call imports it beside `GetStdHandle`, `WriteFile` and `ExitProcess`; hello, with one call, still imports the three. `tests/zx_windows_parity_cases.zi` runs a program that prints, works and prints again under a pseudo-terminal on both systems and times its first line.
- `zx2native --platform windows` writes x64 unwind data for every function with a prologue: a `.pdata` `RUNTIME_FUNCTION` and a `.xdata` `UNWIND_INFO` record that walks the pushed frame pointer and the stack allocation. The record starts where the frame does, after an early return that leaves before any frame exists, so a Windows stack walker attributes frames from the first byte.
- `zx2native --target armv7 --platform linux|android` writes a static ARMv7 ELF32 (EM_ARM, EABI5) with its own runtime, matching the AArch64 writer: the entry stub sets the stack limit, calls the initializers and the entry, and ends through the runtime's exit; the console writer is A32 machine code emitted from Ziran source. Both ARM rows run on their devices (Pixel 4a and Moto e6 play) and both report a named gap for a facility they do not carry.
- `zx2native --target aarch64 --platform linux|android` writes a static AArch64 ELF with its own runtime: the entry stub sets the stack limit, calls the initializers and the entry, and ends through the runtime's exit; the console writer is AArch64 machine code emitted from Ziran source. The image resolves every relocation at link time, so it needs no loader. A program that calls a formatter or a host capability is still a named gap.
## [0.8.0] - 2026-11-23

- Build native AArch64 and ARMv7 images with indexed relocation lookup, so each image relocation visits only its own instruction's machine relocations instead of scanning the whole program. Both devices pass the 81-program parity.

- ZX Runner: `make install-user` installs the `zx` command, the `.zx` file type (`application/x-ziran-bundle`, recognized by name and by its `ZX\0\0` header) and a desktop entry that file managers use to open `.zx` files. `zx app.zx` runs any bundle with the toolchain's runtime and hands it to no other program. It first explains a file that is not a ZX program or uses an older or newer ZX format, and names a bundle that needs platform capabilities, such as a window, that the runner does not provide. Started without a terminal, it also shows failures in a dialog.

- Removed `std/rss_scan` (feed item and tag scanning: `FeedNextItem`, `FeedTag`, `FeedCopyText`). The standard library has no replacement; a program that needs it copies `std/rss_scan.zi` from commit c3ef9fb2, the last one that has it, into its own module root.

- Removed `std/http_get_linux` (`GetHttps`, `GetHttpsCapped`). Use `std/net_http`: `SendCapped` with an `HttpRequest` whose `method` is `"GET"`. Commit 4c62f8b8 is the last one that has the old module.

- `#caller_location` names the call site's file relative to its module root, or by its package and the path inside that package, so the same sources build the same output from any folder or package cache.

- ZX embedders can request an active guest stop from another thread, including during initialization and library calls.

- Hosted ZX applications can open from a read callback with a smaller instruction store when running on the portable engine.

- Compile hot packed ZX integer instruction runs on Linux i386 with bounded executable storage and the interpreter's work, fuel and failure locations.

- Speed up large record and array loads in the ZX instruction engine.

- Check public API declarations and examples for a selected backend and platform with `ziran api`.

- Pack checked ZX libraries for explicit OS and CPU targets with `ziran pkg zx pack-ir --locked`.

- Reduce portable ZX integer execution overhead by reusing canonical operands and looking up edge metadata only at branches and jumps.

- Allow instruction-engine hosts to borrow packed read-only scalar slices for synchronous callbacks.

- Add optional bounded progress and cancellation callbacks to PNG/JPEG decoding while preserving the ordinary image readers.

- Add compact/reference instruction-budget parity coverage and retained i386 runtime comparisons; the unproven interpreter optimization candidate was withdrawn before installation.

- The portable ZX engine reuses frame storage for large record and array snapshots with disjoint lifetimes.

- Package locks preserve each dependency's own name and exports when multiple refs select the same local override.

- ZX supports indexing typed pointers. The engine and checked native backends use the backing allocation to reject invalid indices.

- The portable ZX engine can resize a heap block using adjacent free space on both sides, avoiding a replacement allocation when the combined space fits.

- The portable ZX engine reuses storage for temporary derived addresses across branches, so loops can traverse large buffers without filling the address table.

- Native ZX can run with a smaller temporary frame arena when the host cannot reserve 256 MiB of contiguous address space.

- Speed up SHA-256 verification on hosts without SHA instructions.

- Package tested native Ziran toolchains in Gitea by exact source revision and Linux host profile.

- Distribute checked packages as versioned `.zx` libraries for source-free imports in ZX and native builds.

- Publish and fetch verified `.zx` images and checked compiler companions through Gitea. Package locks can discover and pin compatible checked dependencies for source-free ZX builds.

- The engine-only ZX reader can decode directly into compact instruction storage and expand one function at a time for verification and planning.

- Compact ZX execution uses half the storage for value locations when the configured limits fit a signed 32-bit encoding. Larger plans retain 64-bit locations.

- C embedders can use `BundleOpenEngineReadAt` to reduce the engine’s verified instruction storage. A host allocator can release the unused storage through an optional shrink callback.

- C embedders can open seekable ZX app images through `BundleOpenReadAt` without keeping a full input buffer. The callback is needed only during verification, and images with assets, module linkage, or additional attachments use the byte-based API.

- Native x86-64 code copies and clears fixed-size values through 512 bytes with inline vector instructions, takes register hints from actual control-flow predecessors, creates a stackless leaf's frame only when a fuel check traps, and copies only the used members of record snapshots read through constant fields.

- `zx` hosts bundles in-process and gives them files, the clock, arguments and environment, and their own embedded assets (std/guest_files, guest_clock, guest_process, guest_assets); a capability it lacks is named in the error.

- `ziran api --json` exposes optional type-checked, runnable usage examples for public symbols.

- Native instances release temporary frame reservations while idle, so multiple bundles fit in a 32-bit address space without reducing the active frame limit or checked pointer protection.

- Embedders can route instruction-engine ZX output through a per-instance console callback, including output from global initializers.

- In-process native ZX resizes large heap blocks through the allocator instead of always allocating and copying a replacement. Growth remains zero-filled and checked aliases are invalidated even when the address stays the same. The Vectors kernel on Alpha took 38.19 µs instead of 40.04 µs in 11 matched rounds, against 34.07 µs for C.

- Wasm stores can copy a loaded record directly from its source when no intervening effect requires a snapshot. Copies through pointers preserve partially overlapping memory in either direction.

- The shared optimizer resolves calls through known function addresses, gives calls inside loops a larger inlining budget, and restores borrowed input on resource-limited specialization. Standalone ZX tools now rebuild when the specialization module changes.

- Source and saved module graphs can grow beyond 1,024 files, with bounds checked against available addressable storage.

- Platform conditions in modules with file-scope enum imports no longer force an otherwise ordinary import cycle to resolve at compile time.

- Executable ZX identities hash whole byte ranges through the runtime’s shared SHA-256 implementation, including CPU acceleration where supported, instead of feeding a separate hasher one byte at a time. Identity validation and resource limits are unchanged.

- The native disk cache reloads ARMv7 and AArch64 machine images, including the ARMv7 runtime helper relocations. Android apps reuse compiled code across launches.

- ZX lowering turns a range test with literal bounds, such as `code >= 12288 && code <= 12543`, on a stable local, record member or fixed-array element into one unsigned comparison. Kryon's TextIdeographic drops from 55 to 41 instructions, and the Text widget scenario ran faster in Wasm in 8 of 10 alternating Alpha rounds, with the paint result checked on every call.

- Wasm frames hold only the record slots a procedure still uses. A large record read in place, or a zeroed record that is never built, reserves no frame space, so Shovey, whose level replay reads single fields of a 306 KB global, compiles to Wasm instead of exceeding the 8 MB frame limit.

- On x86-64 and aarch64, native console prints read their arguments straight from the saved registers instead of decoding each by type. On Alpha, arrays.zi went from 1.018 to 0.490 µs per call, enums.zi from 0.353 to 0.177, below Wasm, and loops.zi from 1.560 to 0.720.

- Native ZX gathers standard output in the same console buffer as the engine and hands it to stdio once per run, and before standard error, a host capability call or a trap report. On Alpha, arrays.zi went from 1.150 to 1.023 µs per call, enums.zi from 0.392 to 0.360 and loops.zi from 1.780 to 1.600.

- Native host calls resolve each function's capability or platform service once per instance, keep up to eight arguments on the stack instead of allocating them, and format printed integers directly. On Alpha, arrays.zi went from 2.075 to 1.122 µs per call, enums.zi from 0.686 to 0.378 and loops.zi from 3.155 to 1.718.

- `BundleInstanceOptions.unbounded` runs an instance without step metering: native code for it is built without fuel checks, like an unbounded executable, and takes no step limit. The standard and example benchmark hosts use it, as C and Wasm run unmetered. A normal native run now leaves its output to stdio buffering, as the engine does, instead of flushing stdout and stderr on every call. On Alpha, enums.zi went from 1.213 to 0.688 µs per call, arrays.zi from 2.563 to 2.059, score.zi from 0.033 to 0.022, dijkstra by 24% and fannkuch-redux by 14%.

- In-process native ZX no longer makes a system call on every entry: each thread keeps its stack range, and a forked child looks it up again under its own limit. Records & calls (score.zi) went from 0.511 to 0.032 µs per call on Alpha and text_demo.zi from 0.512 to 0.033 µs; arrays, enums and loops took 13–30% less time.

- Fixes: native ARMv7 and AArch64 code reads and changes members of large records and globals in place, as x86-64 code does, instead of copying the whole record into the frame for every access. Shovey's `LevelReplayPump`, whose 12 MB frame stopped it at its frame limit when Play was pressed on an ARMv7 phone, now needs 1.8 MB on every target. A record or array built from zeros is zeroed in one pass instead of one store per member, so Shovey's native code shrinks from 6.4 to 1.4 MB on ARMv7 and from 2.8 to 0.9 MB on x86-64, and its AArch64 build, where one 4.7 MB function put branches out of range, now compiles.

- Native code is verified once: the encoder no longer verifies again a program register allocation just verified, which shortens a phone's on-device compile. The native disk cache now works in Android apps: a library linked without a GNU build ID, as reproducible APKs are, is identified by its code, and an app process caches in its own cache directory, so only the first launch after an install compiles a bundle.

- Fixes: generated C declares a generic procedure's copy for its caller when the copy's signature names a type through the template's own import alias and the caller has a module-scoped import of that name; GCC 14 rejected the call as an implicit declaration (Kryon's text Draw called from Terminal).

- ZX applications can load separately compiled libraries with verified typed imports, independent instances, export inspection and state-preserving replacement. The loader checks dependency graphs and accepts explicit module roots.

- Most of the compiler builds from Ziran source, bootstrapped from generated C seeds: source parsing, module discovery, declaration parsing, compile-time evaluation and condition selection, IR cleanup, the frontend IR, compiler contexts, source documents, diagnostics, paths, semantic IDs, SHA-256 and law checks, the checker (expressions, statements, values, functions, whole programs, generic linking, SIMD rules, borrow analysis), the C and Go backends with their SIMD and x86-64 assembly emitters, the checked IR codec, module loading, locked package resolution, executable ZX asset collection, entry linking for the C, Go and native backends, inline assembly checking and its C emission, and the ZX embedding API (`include/ziran_host.h`). The saved IR format and the C interfaces are unchanged.

- `ziran run app.zx` hands a bundle only to a runtime provider built by the same toolchain: a `.host` registration records `ziran version --identity` on its second line, and a provider of another build is refused with both identities.

- `ziran lock` and `ziran update` refresh checked-library entries saved under another toolchain instead of refusing; builds name the package and `ziran lock`.

- Generated C names a `#program_export` that two linked package revisions define, gives one type application one C type in every module that spells it, and keeps constants, globals and types off the runtime's and libc's names.

- Nested array literals work at file scope and in procedures: `table: [2][2]s64 = .[.[1, 2], .[3, 4]]`.

- Inside a project, `ziran api PACKAGE/MODULE` prints that package module; options may follow the input files, a module name that selects nothing is an error, and text output lists record fields and enum members.

- A syntax or import error says under it that checking stopped there, and a rejected `--bind` says which side is missing or which module already binds the capability.

- Go's `uintptr` is declared with `#type #foreign builtin "uintptr"`.

- New standard modules for Limbo ports: `std/list`, `std/pool`, `std/tuple`, `std/rune_text`, `std/zx_conc` and `std/bundle_guest`. `std/grapheme` answers ASCII text without its tables.

- Two dependencies that name one repository by different URLs share one module namespace under a local override.

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
