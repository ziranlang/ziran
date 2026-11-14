# Native hello-world optimizations and `.lirb` vs `.zx`

Owner, 2026-11-13: "yes do all optmizations run a subataoks for it".
Source of the findings: the native ELF built from `examples/hello.zi` (objdump/nm) and its
lowered text. Baseline: 1,377 bytes of `.text`, 224 instructions,
a 4,840-byte file, and `main` is 1,342 bytes.

## Status

Items 1–3 and 5–7 and the one-pipeline unification were committed together on
2026-11-13 after `make check` (276/278 local tests; `zstd_go.sh` needs Go 1.25+ and
Go 1.24.4 is installed; `native_build_parallel.sh` timed out under full-suite load and
passes alone) and `make check-bootstrap`.

| # | Item | Owner of the work | State |
|---|------|-------------------|-------|
| 1 | Pointer lowering: replace per-element compare chains with base + index and one bounds check | native worker (`compiler_lowered_x64.zi`) | implemented; local regression tests pass |
| 2 | Symbol table: omit inlined services, retain distinct concrete functions with positive bounded extents | native worker | implemented; local no-undefined-symbol regression passes |
| 3 | LIR folds: element.address with constant 0, extract of a fresh aggregate, convert of a constant, dead values | LIR optimizer worker (`compiler_opt.zi`) | implemented; local optimizer tests pass |
| 4 | Native inspection on the M2 pipeline (machine IR + regalloc + encoder) instead of stack-slot `compiler_lowered_x64` | after hello-world lands | planned |
| 5 | Instruction selection: short `mov`/`xor` for small constants, no jump to the next block, no empty 0x50 frame | native worker | implemented; local byte/layout tests pass |
| 6 | ELF layout: `.rodata` right after `.text` with congruent offsets (no 4 KB padding); drop the empty `.data`; keep symbols | native worker | implemented; local ELF/layout tests pass |
| 7 | Smaller LIR: prune unreferenced types, intern duplicate origins | LIR optimizer worker | implemented; local optimizer tests pass |
| 8 | Smaller ZIR: save expression text as a slice of its statement's text | Claude task, after the integration commit | implemented (ZIR 64), uncommitted: the published site snapshots must be regenerated on Alpha first, see below |

Item 8, owner 2026-11-13: "why in zir is print hello world repeatetd twice that dhosen seme to make senes".
`cmd/compiler_serial.zi` writes a `text` string for every statement (`Statement`, `item.text`) and
every expression (`Expression`, `Source(codec, item.text)`). It also writes each node's `span`.
Identical strings are interned, but each node's spelling is different, so hello's 223-byte ZIR
holds three copies of the literal:
- `print("Hello, World!\n");`: the statement
- `print("Hello, World!\n")`: the call expression
- `"Hello, World!\n"`: the literal argument
- `print`: the callee name
Nested expressions repeat their parents' text, so the size grows with nesting depth. Fix:
- Find every consumer of `ZirStmt.text` and `ZirExpr.text`.
- Keep only the text that carries meaning: literal values, names, and `#asm` variants.
- Rebuild the rest from the span and the source file when a diagnostic needs it.
- Bump the ZIR version and keep the C reader (`cmd/zir/zir_serial.c`) in step.

Item 8 result (ZIR version 64, owner 2026-11-13: "canyo umake it more efficent"):
- Consumers: statement text is parsed again when saved ZIR is loaded and checked
  (`zir_cleanup.c` for/while headers and loop targets, `zir_check_statement.c`
  `#through`/`#complete` and case headers, `zir_check_link.c` type applications,
  `zir_expr.c` `#asm` blocks, `zir_emit_statement.c` parallel direction). Removing it
  is unsafe, so `ZirStmt.text` stays, saved once per statement.
- Expression text is read by the checker, emitters, VM and inspector (`#compile_time`,
  literal values, names, diagnostics), so it must still read back exactly. It is no
  longer saved as its own string when its statement holds it: the writer saves the
  statement's distance from the last one referenced, the length, and a change in the
  bias between the span and the text's offset. The writer keeps whichever form is
  shorter, and the reader rebuilds the identical text.
- Spans alone cannot locate the text: expression columns are offset by the
  statement's header (`if `, `x := `), and expression end positions often run to the
  statement end. The saved bias and length cover both. Fixing those spans would
  change diagnostics and is not part of this item.
- Sizes (`zi2zir`, same sources; `ziran inspect` output is identical before and after):

| ZIR file | v63 bytes | v64 bytes | change |
|----------|----------:|----------:|-------:|
| `examples/hello.zi` → `hello.zir` | 223 | 184 | −17.5% |
| `cmd/compiler_lowered_x64.zi` → `compiler_lowered_x64.zir` | 583,008 | 526,092 | −9.8% |
| `compiler_lowered_verify.zir` (same graph) | 236,630 | 190,988 | −19.3% |
| whole `compiler_lowered_x64` graph, 5 modules | 859,812 | 751,127 | −12.6% |

  Hello now saves `Hello, World!` once. The rest of a large module is mostly the
  per-field integers and spans of each record, not text.
- The writer looks for an expression's text only within 256 bytes of the offset its
  span names, and each statement's length and line cursor are cached, so a huge
  literal statement stays linear (`large_array_literals.sh`: 60.4 s before, 8.8 s after).
- Blocker for committing: all 22 published `.zir`/`.zx` artifacts under
  `site/generated/` (targets, json-text, library-targets) embed ZIR 63, which the new
  reader rejects (`site_targets.sh` fails on `score.zx`). Their generators
  (`scripts/site_targets.py`, `bench/json_text.py`, `scripts/site_library_targets.py`
  with Kryon) re-run the timing benchmarks, which run on Alpha's physical host
  (`bench/README.md`). This task does not connect to Alpha, so the snapshots wait for
  an Alpha run before item 8 is committed.
- Published site artifacts were re-encoded from ZIR 63 to 64 by a one-off converter
  that links the old and new codecs. Every file's `ziran inspect` listing is identical
  before and after, and the recorded timing samples are unchanged because the programs
  are the same. Rebuilding them from source was not possible: Kryon is 1,058 files past
  the library fixture revision, and re-timing belongs on Alpha. Sizes: the Kryon widget
  bundles went 694,419 → 596,310, 1,274,131 → 1,005,489 and 806,383 → 600,674 bytes; the
  75-module Kryon ZIR graph in `checked-ir.zip` 2,586,310 → 2,100,156 bytes (−18.8%); the
  json-text bundles shrank 12–21%. The target-matrix `.zir`/`.zx` were rebuilt with their
  recorded commands (for example `score.zir` 708 → 611 bytes).
- `cmd/compiler_serial.zi` mirrors the C codec field for field. It is not imported
  by any build target, so it is checked with `ziran check`, not run.

The small-hello target is achieved without claiming item 4: `main` contains the
inlined `write_all(1, &string.0, 14)` loop plus return, `.text` is 114 bytes,
and the file is under 1.5 KB. It runs with exact stdout and exit 0, uses
`write` syscall 1 and `exit_group` syscall 231, and has no interpreter,
dynamic segment, libc or CRT. Inlined services are omitted from ELF symbols;
`_start`, `_zir_trap` and `main` remain distinct bounded functions.

## Approved: one native pipeline with ZX as a stage

Owner approved, 2026-11-13: "so do that plesae". Implemented as item (d) by the native worker,
after items 1, 2, 5 and 6. `.lirb` is removed: no `--emit exec` or `--input exec`, and
`--emit lir` stays as a text dump. The VS Code Native view stage update follows once the
CLI commands land.

The CLI unification is implemented and locally tested. Source ELF creation
encodes and reads the same verified optimized ZX payload as saved-ZX input;
both routes produce byte-identical ELF files and maps. The portable interpreter
uses that same executable payload. Source LIR inspection can still show the
unoptimized form, while source `--opt --emit opt-report` reports transformations.
The current executable ZX payload strips source origins, so native maps retain
byte coordinates/instruction IDs but do not yet recover source origins.

The default `ziran`/`zi2zx` semantic bundle routes (revisions 26/27) and
semantic-ZIR Wasm lowering have not been cut over to revision-28 executable ZX;
these remain explicit gaps, not completed parts of the unification.

Cutover blocker, checked 2026-11-13 (coordinator step 5, "one ZX format"): all 14
`examples/*.zi` programs lower to revision 28, but LIR lowering (ASSEMBLY step 03) does
not yet cover the language that semantic bundles run. `zi2asm --emit zx` rejects
`tests/spec/vm_storage_reads_test.zi:30` and `tests/spec/std_utf8.zi:43` with "this type
has no portable lowered representation yet" (`Vec` and other collection types), and
`tests/spec/vm_nested_collection_test.zi:103` with "call is not a direct concrete Ziran
procedure". Kryon applications, packages and the playground depend on those features.
Moving `ziran bundle` to revision 28 and removing the 26/27 writer now would turn those
programs into compile errors, so the second format stays until step 03 lowers them. The
Wasm route also needs an instruction-to-Wasm lowering, which does not exist yet. The VS Code
Native stage migration is a subsequent change in `ziran-vscode`, not item 4.

### Lowering coverage toward one ZX format (in progress)

Owner, 2026-11-13: "isi t being delt with to make ti work?" and then "do notu sue subtasks
leran what they are donig and then fulyl take over". The main session does this work directly.

Inventory, 2026-11-13 evening (`zi2asm --emit zx` against the semantic bundle route, script kept
in `backup/takeover-2026-11-13/lowering/inventory.sh`):

| Set | Programs the semantic route runs | Lower to revision 28 |
|-----|----------------------------------|----------------------|
| `examples/*.zi` | 16 | 14 |
| `tests/spec` entries | 69 | 32 (was 20) |
| Kryon examples and taskmgr (`portable` profile) | 100 | 0 |

First rejection of the 37 spec programs that do not lower yet:

| Count | Rejection | Feature needed |
|-------|-----------|----------------|
| 17 | this type has no portable lowered representation yet | `Vec` and other heap collections, nested collections |
| 4 | only zero defaults and integer/bool constant global initializers are lowered yet | computed and aggregate global initializers |
| 4 | expression is outside the initial direct scalar backend | remaining expression forms |
| 3 | invalid checked integer literal | wide and typed literal forms |
| 3 | call is not a direct concrete Ziran procedure | indirect calls through values and fields |
| 2 | direct lowering requires a concrete checked function without non-host foreign services | foreign services and native variants |
| 4 | lowered IR rejected by the verifier | lowering bugs in aggregate entry signatures |

Every Kryon example stops first at `tree_input.zi:457:15` with the collection-type rejection, so
heap collections come first.

Landed in the first milestone: slice element stores (`view[i] = x`, `view[i].field = x`), global
procedure slots with typed null, value-based comparison of mixed-signedness integers, checked
views of records, fixed arrays and procedure values, slice view types interned by element type,
and the checked platform byte-output binding for the interpreter (`zi2asm --run` prints).
`tests/zx_lowered_parity.sh` pins 37 programs: identical output on the semantic route and the
revision-28 interpreter, and native ELF either matches or stops at a named `assembly.gap`.

Order from here: heap collections; global initializers; indirect calls; the remaining literal and
expression forms; then the `ziran bundle`/`zi2zx` cutover and removal of revisions 26/27.

#### Heap collections design (steps 1, 2 and a first slice of 3 and 4 landed)

How the existing routes treat `Vec(T)` (`std/vec.zi`, `cmd/zir/zir_emit_value.c`):
- ZIR has no drop statements. `Vec(T)` is a specialized record `{data: *T; count: s64; capacity: s64}`
  flagged `is_owned_vec`; operations are calls by name (`VecPush`, `VecPop`, `VecGet`, `VecSlice`,
  `VecSwap`, `VecClear`, `VecClone`, `VecFree`, `VecReserve`, `VecResize`, `VecResizeBounded`,
  `BuilderAppend`, `BuilderPrint`, `BuilderFinish`), plus `v[i]`, `v[:]`, `v.count`.
- The C emitter drops every local whose type contains a Vec at scope exit, return, break and
  continue (`drop_locals`): free the data, zero the three fields, recursing into elements that
  contain Vecs. A move leaves the source zeroed, so the unconditional drop is safe.

Plan, in steps that each keep the suite green:
1. LIR: one heap instruction, `resize(pointer: *T, old_count, new_count) -> *T`, appended after
   `Unreachable` so existing numbers stay. It keeps `min(old, new)` elements, zero-fills new ones,
   frees and returns null for `new_count == 0`, and returns null with the block unchanged when the
   budget is exhausted. Verifier rule, text name `resize`, effectful and value-producing.
2. Interpreter: typed heap blocks beside globals, frames and owned bytes. An address entry names its
   block and generation; a resize or free ends the old generation, so stale pointers fail with
   `.Pointer` as stale frame pointers do. A `heap` byte limit joins the instance limits.
3. Lowering: the Vec record type, `count`/`capacity` reads, `v[i]` and `v[:]` through the checked
   view path with `(data, count)`, then `VecPush`, `VecClear`, `VecFree`, `VecReserve`,
   `VecResize`, `VecPop`/`VecGet` (Option), `VecSwap`, `VecClone`, `VecSlice`, and the builders.
4. Lowering: drops at every scope exit edge, and zeroing the source of a move, mirroring
   `drop_locals` and the emitter's move rule.
5. Native x86-64 and wasm: `assembly.gap.native-heap` and `wasm.gap.heap` first; real allocators
   (mmap-backed veneer, linear-memory allocator) after the interpreter is at parity.
6. `Text`, maps and records that hold Vecs follow from 3 and 4; then re-run the inventory.

Landed 2026-11-13: the `resize` instruction with its verifier rule and interpreter heap
(`tests/lowered_exec_cases.zi` covers allocate, grow, zero fill, stale pointer, wrong count, free
and a refused request); the `Vec(T)` record type for elements that own no vectors; `v[i]`,
`v.count`, `v.capacity`, `VecSlice`/`v[:]`, `VecPush` with the C runtime's growth (0 to 8, then
doubling), `VecClear`, `VecFree`; drops of vector locals at block close, return, break and
continue. `tests/spec/zx_lowered_vec.zi` matches the semantic route byte for byte, capacities
included. Native and wasm stop at `assembly.gap.native-heap` and `wasm.gap.instruction`.

Second slice, same day: the checker's move flag (`is_move` on an identifier or member path) empties
the moved source as the C emitter's `clear_owned_value` does; records and fixed arrays that hold
vectors, vectors of vectors, vector parameters and results; recursive drops, including a loop over
the elements of a vector whose elements own storage; a fresh owned result used as a statement is
dropped at once. `zi2asm --run --heap-report` prints the heap blocks still live, and the parity
test requires none for every program. `tests/spec/zx_lowered_owned.zi` covers these.
Inventory: 34 of 71 spec entries lower.

Third slice, same day: `BuilderAppend` (an empty text allocates nothing; otherwise room is reserved
first with the C runtime's doubling, so a refused reserve appends nothing), `BuilderFinish`,
`TextView` of a `Text` or `Vec(u8)`, and vector types interned by element type so two modules'
spellings of `Vec(u8)` are one LIR type. The checker turns `line := BuilderFinish(builder)` into an
owned `Text` built by `vec.TextFromBuilder`, so its bytes are dropped with `line`.
`tests/spec/zx_lowered_text.zi` covers these. Inventory: 39 of 72 spec entries lower.

Fourth batch, same day: fields read and written through a pointer (`pointer.field`, ZIR's
`pointer_member`); `VecSwap`; record, nested record, string, float and fixed-array global
initializers, each parsed as the checked expression it is and stored by a private
`<name>.initializer` procedure that runs before module startup code. Native output names
`assembly.gap.native-pointer-field` for a record field reached through a pointer argument, which
the stack-slot encoder would otherwise turn into a run-time trap.
Inventory: 48 of 75 spec entries lower. All 89 built Kryon examples now get past types, pointer
fields, `VecSwap` and global initializers and stop at `paint_queue.zi:210`, `VecReserve` on a
dereferenced `*Vec(T)`.

Fifth batch, 2026-11-14: `VecReserve` (exactly the requested capacity when it grows, false for a
negative or refused request) and signed literals in global initializers. Host capability
signatures: a host value is a bool, integer, float, `*void` handle, or a record, fixed array or
view of host values (`LirHostShapes` in `cmd/compiler_lowered_verify.zi`); the interpreter passes
each non-scalar value as one host image and turns returned views into read-only instance storage
(`ExecHostEncode`, `ExecHostDecode`). Types two modules both spell: `float32`, `float64` and
`[N]T` are one LIR type for the whole program; before this each module had its own `float32`, so a
float passed between modules failed verification. The zero value of a fixed array longer than 256
elements is a load from a frame slot the procedure allocates on entry and never writes (a global
would not do: the native encoder lists every element of a global and refuses 9,000). Before, lowering Kryon's button example
gave 147,027 instructions, 65,799 of them in `TextRequestSend` for one zeroed `[65536]u8`, and the
ZX writer refused it at its work limit. Its saved program is now 28,828 instructions in 394
procedures, the largest 1,149. The file is 2,187,468 bytes; the semantic bundle of the same
example is 990,229.
`tests/spec/zx_lowered_shared.zi` and `TypedHostShapes` in `tests/lowered_exec_cases.zi` cover these.

Sixth batch, 2026-11-14: `New(T)` is `resize(null, 0, 1)` and `free(pointer)` is `resize(pointer,
1, 0)` behind a null check (checked ZIR names them `zi_new` and `zi_free`); `size_of(T)` is a
constant with the C layout size; a folded constant keeps its sign (`ToneDark :: -3`); a bare
`null`, which checked ZIR leaves untyped, takes the pointer type it is passed, assigned or
returned as.

Seventh batch, 2026-11-14: `VecResize` reuses the `VecReserve` lowering, then drops the elements
it removes and stores zero over the ones it adds (`tests/spec/zx_lowered_resize.zi`).
`image_resize_fallible` then lowered but spent its whole 4,000,000,000 work budget: every derived
address was found by scanning the interpreter's address table. The table now has a hash index
(`ExecInternAddress` in `cmd/compiler_exec.zi`) and the program runs in well under a second.

Eighth batch, 2026-11-14: `VecGet` and `VecPop` build their `Option` (`has_value`, `value`);
a negative `VecGet` index fails the run as it does on the portable VM
(`tests/spec/zx_lowered_take.zi`). `tests/zx_lowered_parity.sh` pins 54 programs.

Ninth batch, 2026-11-14: `VecClone` for elements that own no storage (the checker only allows a
fresh destination), sharing the element loop with `VecResize`. Dead code audit of the assembly
sources: the C files have no unused functions; seven Ziran procedures nothing called are removed
(`ExecCopyGlobalToStorage`, `ExecCopyStorageToGlobal`, `ExecCopyAddressToStorage`,
`ExecCopyStorageToAddress`, `TakeS64`, `ValueClass`, `X86Move32`).

Tenth batch, 2026-11-14: unions of integers and floats lower to one unsigned integer as wide as
the widest member; `overlay.member` reads the low bytes reinterpreted and `overlay.member = x`
replaces them, keeping the bytes above. `tests/zx_lowered_parity.sh` pins 57 programs.

Eleventh batch, 2026-11-14: an `if` with its `else if` links is lowered as one chain with one join
block, so `unicode_normalization_cases.zi` (a 1,500-case list) lowers. Running it showed two more
interpreter costs. Freeing a heap block scanned every block ever allocated; blocks now link to the
block below them and freed storage is given back down that chain (`ExecHeapTrim`), and a block
keeps its record when it grows in place. The program then stopped at the `addresses` limit of
1,048,576 after about 30 million fuel.

Twelfth batch, 2026-11-14: address entries are given back (`ExecNewAddress`,
`ExecReleaseAddresses` in `cmd/compiler_exec.zi`). Each entry made for a frame's storage or a
heap block is chained to it; the chain is released when the frame returns or the block is freed
or resized, and the slots are reused. A pointer is the instance identity's low 16 bits, a 28-bit
slot version and the slot, so a pointer kept from an earlier use of the slot never matches.
`tests/spec/zx_lowered_addresses.zi` makes 1.2 million calls that each take a local's address.

Thirteenth batch, 2026-11-14: an integer case list of eight or more values (the `case_value`
local the checker makes, compared with literals) lowers to a binary search
(`assembly_dispatch`, `tests/spec/zx_lowered_cases.zi`). `unicode_normalization_test` then runs
to the end on revision 28 with the semantic route's result, in 27 s against 25 s on the portable
VM. It needs more than the 4,000,000,000 work units `tests/zx_lowered_parity.sh` allows, and a
minute per run, so it is not pinned. `tests/zx_lowered_parity.sh` pins 59 programs.

Fourteenth batch, 2026-11-14: vectors. A vector type (`u8x16`) is an array of its lanes and each
`zi_simd_*` call is lowered lane by lane (`assembly_simd`): `Splat`, `Zero`, `Make`, `Load`,
`LoadLow`, `Lane`, `WithLane`, `Add`, `Sub`, `Mul`, `And`, `Or`, `Xor`, `Not`, `Equal`, `Less`,
`Greater`, `AllZero`, for integer lanes. All eight JSON programs lower and give the semantic
route's result. `json_array_reuse_test` ends with 230 live heap blocks: its strings come from
`BuilderFinish`, which nothing frees without a collector, so it is not pinned. The address index
failed when a rebuild made its table smaller (`VecResizeBounded` refuses a bound below the
current capacity); it now only grows. `tests/zx_lowered_parity.sh` pins 66 programs.

Fifteenth batch, 2026-11-14: the last three spec programs. `unicode_conformance` is 646,000
instructions, most of them the cleanup of the owned strings each check makes. It ran the lowering
out of its 256 MB and then the optimizer out of its work: the instructions of one expression now
share one origin, and `LirDefaultLimits().work` is 1,000,000,000 instead of 16,000,000. Its
revision-28 file is 36.8 MB against 4.2 MB for the semantic bundle, so it is not pinned; dropping
an owned value through a shared procedure instead of inline code is the fix for that size.
A procedure or block with `#asm` variants lowers from its ordinary body
(`native_assembly_test`), and a record of the same shape under another name converts field by
field (`generic_assembly_test`: the record a generic procedure returns two results in).
`tests/zx_lowered_parity.sh` pins 68 programs.

Inventory, 2026-11-14:

| Set | Programs the semantic route runs | Lower to revision 28 |
|-----|----------------------------------|----------------------|
| `examples/*.zi` | 14 | 14 |
| `tests/spec` entries | 82 | 82 |
| Kryon examples (`portable` profile, saved IR of the 89 built ones) | 89 | 89 |

Every program the semantic route runs now lowers, and no program is rejected by the verifier.

### Cutover to one ZX format (in progress, 2026-11-14)

Who reads semantic bundles today: 191 of the 394 test files, 22 C embedding tests
(`tests/*_test.c` through `include/ziran_host.h`), and Kryon's whole backend (`bundle_app.zi`,
`app_player.zi`, `hosted_app.zi`, the Android hosts, through `std/bundle_host`). The writer can
only switch once each of these reads revision 28, so the cutover goes in stages, each green:

1. Done: `ziran run`/`zi2zx run FILE.zx` run a revision-28 file on the instruction engine with
   the semantic runner's output. `tests/zx_lowered_parity.sh` checks it for all 68 programs.
2. The embedding API (`BundleOpen`, `BundleCapability*`, `BundleRun`, `BundleInstantiate`,
   `BundleInstanceRun`) opens revision 28, translating `VmHostValue` arguments and results to and
   from host images, with mutable slice copy-back. Steps:
   a. Done: any pointer crosses as an opaque handle; platform services require `ziran.platform`.
   b. Done: the optional host interface section names each capability's types and record fields
      (`AssemblyHostInterface`; format in `ASSEMBLY/zx-executable-format.md`).
   c. Done: `libziran` links the instruction engine (`cmd/compiler_runtime.zi`), generated
      by the bootstrap compiler like the other modules, one archive member per module so an app
      that compiles the same standard modules links none twice; its headers are found with
      `-iquote` because the set has its own `math.h`. Bootstrap builds open no bundle and
      compile a stub (`ZIRAN_NO_ENGINE`), so no generated copy is checked in.
   d. Done: `cmd/zir/zir_host.c` opens revision 28 as one more instance kind through
      `cmd/zir/zir_host_exec.c`, which translates host values with the interface section by
      the semantic VM's rules. `tests/portable_host_records.sh` runs the unchanged
      `record_host_test.c` against the revision-28 file. Not yet: mutable slice copy-back,
      state export/import, components, libraries, assets, `BundleCapabilityAbiAt`.

   With `ZIRAN_BUNDLE_EXEC=1` (temporary, removed at the switch), `zi2zx bundle` and
   `ziran run FILE.zi` write and run revision 28 through `ZxExecBuild`. The suite then passes
   218 of 280. The other 62, grouped:
   - VM internals: about 10 `tests/vm_*_test.c` drive the semantic VM through its program
     (`program != NULL`); they go with the VM.
   - Not lowered yet: parallel statements (6), more `Simd` operations (3), compound assignment
     to a union member, three verifier rejections, one optimizer work limit, one value limit.
   - The compiler's own Ziran test programs bundled as programs (`compiler_opt`, `lowered_exec`,
     `lowered_serial`, `compiler_lowered_verify`).
   - Embedding: step limits during global initialization, an unsigned field read, slice
     copy-back, header byte comparisons, components, libraries, assets, state.
3. `zi2zx bundle` and `ziran run FILE.zi` write and run revision 28. Each semantic-only feature
   then gets its revision-28 form or goes: assets, laws, state export, components, libraries,
   linked native code, the browser player.
4. Delete the semantic writer and VM (`cmd/zir/zir_bundle*.c`, `zir_vm*.c`) and the ZIR-based
   Wasm lowering (`cmd/zir-wasm/zir_wasm_lower.c`, `zi2wasm`).

Work split, 2026-11-14 (owner: "keep going until it is 100% complet you may use otehr agents
usb tatks if it helps"). Three Claude specialists, disjoint files, none commits; the
coordinator integrates, verifies with `ZIRAN_BUNDLE_EXEC=1 make check` and the default suite,
and commits:
- `claude-ziran-zx-lowering-coverage`: `cmd/zir-asm/zir_asm_lower.c` (except the host
  interface generator): `using` promotion, remaining `Simd` operations and float lanes,
  `simd_target` fallbacks, the verifier rejections, array literals in read-only data, the
  compiler's own test programs.
- `claude-ziran-zx-embedding-api`: `cmd/zir/zir_host_exec.c`, `zir_host.c`, the host interface
  generator, host slice copy-back in `cmd/compiler_exec.zi`: step limits during initialization,
  state export/import, components, capability signatures, assets, remaining host-value details.
- `claude-ziran-zx-wasm-host-abi`: `cmd/zir-wasm/zx_wasm_lower.c`, `cmd/zir/zir_zx_wasm*.c`:
  typed host ABI, linear-memory heap, browser bridge, bundle Wasm tier on revision 28.
Result (committed 76141143): with `ZIRAN_BUNDLE_EXEC=1` 247 of 280 local tests pass; the
default suite passes. Owner, 2026-11-14: "do everything deleto old shit", "migart 100% to the
new verison"; bundle libraries go (build-time linking replaces them).

Switch-over, in progress (same three specialists):
- Embedding specialist: bundles always revision 28; delete the semantic reader and writer, the
  VM, TinyCC-compiled and linked native bundles (`native-c`), libraries, the VM profile, `--vm`,
  `BundleInstantiatePortable`, `libziran-vm.a`, the VM-internal tests; engine problems mapped to
  the diagnostics users read; docs for one format.
- Wasm specialist: delete the ZIR Wasm lowering and `zi2wasm`; Web tests on revision 28; state
  on revision-28 Wasm instances.
- Lowering specialist: zero-sized returns, a global in `wasm_enums`, `packages`,
  `json_string_scan`, the assembly test timeouts, `#asm` bundles, records in frame slots on the
  native encoder.
- Coordinator: removed `cmd/compiler_serial.zi`, its service helpers and `cmd/law-isolate/`
  (42da58fe, 1,368 lines). After the flip, Kryon switches: its web `libziran.a` builds
  without `BOOTSTRAP=1`, browser players call `InstantiateBundle`.
- Open question to measure on Alpha: bundles in a player ran as TinyCC-compiled native code on
  desktop; on revision 28 they run on the instruction engine.

Kryon lowers but does not run on revision 28 yet. Before `ziran bundle` can cut over:

1. Host calls: copy a host's changes to a mutable slice argument back into the caller's slice
   (the semantic route does); pointer arguments such as `seed: *s32`.
2. Host-returned storage is never reclaimed. A frame loop that asks a host for a string every
   frame grows the instance until its byte limit. It needs a release rule.
3. Done for frames and heap blocks, 2026-11-14 (twelfth batch). Still permanent: an entry for
   each distinct address derived inside a global or inside host-returned storage. Addresses that
   are only loaded from or stored to in the block that makes them need no entry at all; that
   would also cut the work spent on them.
4. The embedding API (`BundleOpen`, `BundleRun`, `HostValue` in `ziran_host.h`) and Kryon's
   bundle player speak the semantic VM's host values; they need a bridge to host images.
5. The browser player: Kryon's `zx-services-test` and `zx-hosted-test` stop in the ZIR-based
   Wasm tier with "host capability ... is outside the wasm s64 -> s64 execution subset"
   (reported by the Kryon task, 2026-11-14). Host values beyond `s64 -> s64` on Wasm belong to
   the revision-28 Wasm lowering that replaces that tier.
6. Done, 2026-11-14: an array literal of 64 or more numeric literals is read-only data, one
   byte-array global that a loop decodes into a frame slot, and a fixed-array local or
   parameter over 4096 bytes lives in a frame slot instead of being passed to every block (each
   pass copied it, so a loop over a table was quadratic). `large_array_literals` (260,000
   elements) lowers and runs. Still open: the verifier and the executable format accept only
   byte-array constants, so a table of wider elements is decoded byte by byte at run time; a
   constant of any scalar array type would let it be loaded once. That touches the verifier,
   the executable reader and writer and both native encoders.

7. Done, 2026-11-14: `ziran check` reported Kryon's direction pad laws as unknown. The law
   evaluator compared a returned record's type by spelling, so a procedure declared
   `-> DirectionPad.DirectionPadHold` that forwards the imported module's record was refused.
   It now compares the declared types (`compile_return_value` in `cmd/zir/zir_parse_typed.c`),
   and an untyped `.{...}` return takes the return type. All 31 Kryon law files pass.

Owner, 2026-11-14: "make sure you also remove any legacy code so we are not to omany liens of
coed". Each change deletes what it replaces. The cutover deletes the semantic bundle writer and
VM (`cmd/zir/zir_bundle*.c`, `zir_vm*.c`: about 12,800 lines) and the ZIR-based Wasm lowering
(`cmd/zir-wasm/zir_wasm_lower.c`: 3,400 lines); item 4 of the table at the top then leaves one
native encoder instead of two. Neither can go earlier: `zi2wasm` and the bundle Wasm tier
(`cmd/zir/zir_zx_wasm.c`) still call the old Wasm lowering. Revision 28 lowers every `Simd`
operation since 2026-11-14, so its SIMD tests can move to it. `cmd/compiler_serial.zi` (1,147 lines) is built, imported and
tested by nothing; it mirrors the C ZIR codec and reads like the next module of the self-hosted
compiler, so it is left for the owner to decide.

Not yet lowered: `VecClone` and `VecGet` of elements that own storage, map types, unions with
record members, initializers that call procedures, indirect calls through fields.

Lowering coverage, 2026-11-14 (`claude-ziran-zx-lowering-coverage`), all in
`cmd/zir-asm/zir_asm_lower.c`. With `ZIRAN_BUNDLE_EXEC=1` the suite went from 219 to 242 of 280
passing; `wasm_enums` and `global_owned_storage` were fixed after that run.
- `using`: the checker writes a promoted member as the dotted path of its `using` fields;
  `assembly_field_path` walks it for reads, addresses (also through a pointer) and stores.
- `Simd`: `assembly_simd` lowers the whole `zi_ref` table lane by lane, integer and float lanes,
  with branch-free masks for choices; shifts past the width and float-to-integer conversion are
  guarded because the engine traps where `zi_ref` saturates. Memory operations read slices,
  strings, fixed arrays and pointers; a pointer lane is reached as the last element of a view
  one past it, which the engine checks against the pointer's storage. Native vectors are one
  block, and `#target` procedures lower from their body.
- Generic records applied in several modules (`__type_<hash>` copies with one body) are one
  lowered type; `VecResizeBounded`; module-qualified constants and enum members named through
  their type in global initializers; `x: *T = null`; read-only array literals and large arrays
  in frame slots (item 6 above).
- Owned globals: a move out of a global or a member of one empties it, and assigning one drops
  what it held after the new value is made (`global_owned_storage` gave 738 for 333 and kept
  every old value alive).
- A reachable C or Go foreign import and a union with a non-scalar member are refused with the
  semantic bundle writer's words.
- Found outside lowering: the native x86-64 encoder traps on an array field of an address-taken
  local record (`block: Block; p := *block; block.bytes[3]`, also `[8]u8`), so records stay
  values and only fixed arrays move to frame slots. The interpreter fails `calling` when a
  function returns a zero-sized value (`[0]s32`, `generic_procedure`). Out-of-bounds and
  runaway-recursion failures on revision 28 print `problem=pointer` and `problem=value-limit`,
  and an unmatched case `problem=unreachable`, where `simd`, `portable_limits` and `match` grep
  for "out of bounds", "nested calls filled the stack" and "unreachable code executed".
  `zx_native_asm` expects the bundle to carry `#asm` machine code, a semantic-only feature.

### Wasm from revision-28 ZX

Landed as `zi2asm --emit wasm` (`cmd/zir-wasm/zx_wasm_lower.c`): the verified ZX payload lowers
to one wasm32 module. Source and saved-ZX input give identical bytes; hello is 262 bytes.
`tests/zx_lowered_parity.sh` emits each program twice, compares the bytes, runs the module in
node and compares its output with the interpreter. Every program that runs to its end must run
as wasm: 69 of its 70 (one traps).

Done, 2026-11-14 (Wasm specialist):
- Typed host ABI, the interpreter's: one `(zx, host) (capability, record) -> ok` import; a
  16-byte record entry per argument and one for the result; scalars and pointer handles as
  canonical bits, other shapes as host images built and checked by per-type helpers in the
  module; result images placed with the exported `zx_reserve` and kept by the module; changed
  `[]T` elements copied back; the host sees each slice's element address to reject aliasing.
  `zxhost` names the capabilities. Print services keep `host_i64`, so published site modules run.
- A heap for `resize`: power-of-two blocks with a 16-byte header, free lists by size, the top
  block grows in place and gives its bytes back; memory that cannot grow gives null.
- `zx_initialize` runs module initializers when an instance opens.
- The bundle Wasm tier opens revision-28 bundles (`ZxWasmOpenExec`, routed by `zir_host.c`); the
  web provider moves bytes and `ExecHosts` (`zir_host_exec.c`) translates images to the page's
  `VmHostValue` bindings. `tests/zx_wasm.sh` runs a typed bundle on it under node, built by emcc
  without `BOOTSTRAP=1` (a bootstrap build has no engine and opens no revision-28 file), and
  compares it with the engine.

Not done: semantic bundles still use the ZIR-based `zir_wasm_lower`, and `zi2wasm` (used by
`ziran build --target=wasm`, the site targets, the playground and benchmarks) still exists; both
go with the semantic route. With `ZIRAN_BUNDLE_EXEC=1`, `zx_linked_web` and `zx_web_lifecycle`
stop at `zi2zx native-c`, which writes no linked C for a revision-28 bundle; `wasm_enums` stops
at LIR lowering of a global used in an identifier expression; and state export/import of a
revision-28 Wasm instance is not mapped from its `zxstate` table yet.

Owner, 2026-11-13: "i'm ondireng if zx and native basicalyl use the same piplen if we just
unife it under native and show zx as a step in teh pielpn while ofcuesr being able to do zx
as teh portabel lower prat for all pltaforms or am i misisng something".

One pipeline: `.zi -> ZIR -> lowered LIR -> verify -> LIR optimizations -> .zx`
(portable, linked, verified). Then either the portable interpreter on any platform, or
native lowering (`.zx -> machine code -> ELF`). Points to keep:
- `.zx` stays a first-class output (ship it and run it anywhere), not only an internal step.
- Native lowers only from the verified `.zx` bytes, so one `.zx` always gives the same ELF.
- LIR-level optimizations run before `.zx` is written, so the interpreter and native both
  benefit; machine-level ones run after.
- Host capabilities are declared once in `.zx`. The interpreter binds them at run time;
  native binds them at lowering time to target veneers. Missing ones are hard errors.
- The VS Code Native view shows the stages ZIR -> LIR -> ZX -> asm -> ELF. C and Go stay
  separate generated routes, never "native".
- Check whether the Wasm route (`zir_zx_wasm`) already lowers from `.zx`; if it does, it is
  another lowering of the same pipeline.

## Earlier question: should `.lirb` just be `.zx`?

Before unification, ZX revision 28 stored the same deterministic lowered payload
as `.lirb`, inside a container with a 128-byte manifest, identity, entry and
capability bindings. The current payload is LIR schema 4 / internal codec 2;
the executable manifest's contract ID remains schema 3 (see `docs/ZX.md`).
`zi2asm` lowers `.zx` straight to ELF (`hello.zx -> hello-zx.elf`).

Historical proposal (approved and implemented above):
- Make `.zx` the only on-disk portable program format. Native lowers from `.zx`, and the
  interpreter runs `.zx`.
- Remove `.lirb` as a separate artifact (`--emit exec`/`--input exec`). Keep
  `--emit lir` as a text dump for inspection only.
- The VS Code Native view shows `.zx -> final.elf`.
