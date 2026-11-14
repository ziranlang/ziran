# Direct assembly implementation

## Status: experimental backend, not the completed upgrade

The ten-step plan is **not finished**. `zi2asm` lowers checked source or
saved `.zir` to verified, typed SSA and directly encodes a Linux x86-64 ELF
executable. Every `.zx` is the [revision-28 executable
container](zx-executable-format.md) of those verified instructions: `zi2zx
bundle` and `zi2asm --emit zx` write it, and `ziran run` and the embedding API
run it on the portable instruction engine. The C, Go and wasm source-output
routes are separate.
Delivery order and the plan review are in [milestones](milestones.md).
The [gap policy](gap-policy.md) is normative: unsupported direct-target work
fails with a stable diagnostic and remains visible in the completion matrix;
portable, legacy and alternate backends are never fallback success paths.

The experimental direct pipeline is:

```text
.zi -> .zir -> lowered LIR -> verifier -> LIR optimizations -> .zx
                         |                                          |
                    --emit lir                            verified ZX reader
                                                                    |
                                          portable interpreter OR target lowering
                                                                    |
                                                       ELF + native range map
```

The compiler tool still uses the normal compiler bootstrap. Once built,
`zi2asm` writes machine bytes itself: it does not generate C or launch an
assembler, C compiler, or system linker to produce this executable.

## Implemented boundary

- Fixed-width signed/unsigned integers, bools, float32 and float64, local scalar
  bindings, arithmetic, ordered/unordered comparisons and checked conversions.
- Branches, lazy `&&`, `||` and `ifx`, while and counting loops, loop exits, and
  direct scalar calls including recursion. Native calls support six integer and
  eight floating-point register arguments, independently.
- Record and fixed-array construction, extraction, local field/index mutation,
  record-returning direct calls, nested value copies and simultaneous aggregate
  block transfers in lowered IR and native ELF output. Local fixed arrays accept
  dynamic integer indices with signedness-aware bounds checks. Assignment
  destinations and every nested index are evaluated once before the right side.
  Aggregate programs use the explicitly documented
  [packed internal ABI](native-aggregate-abi.md), not the external System V
  aggregate ABI; mixed direct calls are not limited to argument registers.
- Portable and native procedure values, arguments/results and indirect calls
  through local slots, record fields and fixed-array elements. Native procedure
  slots are checked tokens with explicit signatures: null, invalid and
  wrong-signature targets trap instead of becoming arbitrary machine addresses.
  Dispatch is currently an unoptimized matching scan. External C/System/Host
  callbacks remain unsupported; this is the packed internal ABI, not a foreign ABI.
- Static integer/bool constant globals and zero-default records/fixed arrays,
  including nested field mutation, imported globals, file-private definitions
  and local shadowing in source/saved IR and the portable instruction engine.
  Global/record-field addresses, pointer arguments/results and dereference
  reads/writes also lower to checked address operations. Assignments evaluate
  the destination once before the right-hand side; compound assignments retain
  the preceding value even when a call mutates an alias. Local bool, integer,
  float, pointer, record, fixed-array and procedure-value bindings can have
  their address taken; such locals become checked frame allocations. Dynamic
  fixed-array element addresses
  rooted in globals or checked pointers are bounds-checked, including nested
  records/arrays and addresses returned across procedure calls.
  Retained tests compare C, Go and explicit old-VM execution where the old VM
  supports the construct. A call-valued dereference assignment has C/Go/new-engine
  parity, but is outside the old VM's portable subset. Dependency-ordered
  runtime module initializers execute once per portable instance before entry;
  a failed initializer is sticky and entry is not started. Native ELF runs the
  same ordered initializer list before entry. Mutable global state, readonly
  constant storage and initializer state are owned by the execution instance.
- Block-parameter SSA with simultaneous edge copies, explicit ordered effects,
  copied source origins, and a bounded verifier that rejects invalid definitions,
  dominance, CFG edges, types, and effect order.
- Deterministic raw x86-64 encoding and ELF output with `.text`, `.strtab` and
  `.symtab` sections. Startup, trap and `module:function` symbols refer to the
  actual linked function bytes, so final disassembly has meaningful labels.
- Static globals and checked global/record-field pointer slots in direct ELF,
  with zero/scalar initialization, packed aggregate loads/stores, pointer
  parameters/results and separate RX text, read-only data and RW data segments.
  Writes through read-only data tokens trap. Native dispatch
  validates tokens and exact pointee types; it is currently a bounded linear
  scan, not an optimized pointer-access path. No generated-C bridge is used.
- Verified lowered IR supports frame-local allocation of every current scalar,
  record, fixed-array and procedure-value type in both the portable engine and
  native ELF.
  Every execution receives fresh zero-default or explicitly initialized storage.
  Native active-frame descriptors preserve aliases through calls and recursion;
  stale, wrong-type, forged, and exhausted-generation tokens trap with status 125.
- `#foreign host_api` calls lower to bodiless `Host` functions and explicit
  `host.call` instructions from source and saved IR. A host value is a bool,
  integer, float, pointer, which crosses as an opaque handle, or a record,
  fixed array or view (`string`, `[]T`) of host values, nested at most 64
  deep. Procedure values and types that reach themselves through a view are
  rejected. The platform services are the named functions of the
  `ziran.platform` module; a function of that name elsewhere is an ordinary
  host capability. The revision-28 container records the recomputed capability
  count. Embedders use `LirExecBindHost` for the scalar ABI or
  `LirExecBindHostTyped` for the typed ABI. Typed scalars and pointers cross in
  `bits`. Every other value crosses in `bytes` as one host image: the value's
  own bytes, then the elements of each view inside it. In the image a view's
  two words are the byte offset of its first element from the start of the
  image and its element count; an empty view is two zero words. Arguments are
  validated and copied. Returned views become instance-owned read-only storage
  that remains valid for globals and later calls; a malformed image fails the
  call and keeps nothing. A host's changes to an argument image are not copied
  back yet.
  A `--bind` provider already present in the checked Ziran program lowers to a
  direct portable call. Native output accepts only the exact typed
  target-owned console services described below; arbitrary host capabilities
  still fail until native host trampolines exist.
- `string` lowers to the record `{data: *u8, count: s64}`. Literals become
  deduplicated read-only byte-array globals (empty literals use a null data
  pointer); copies, reassignment, returns, `.count`, `.data`, checked indexing,
  slicing and equality/inequality work in source, saved IR and the portable
  engine. Slices use checked `{data, count}` views over fixed arrays or other
  byte views, including empty/null views. Stores through read-only bytes trap.
  Direct native output supports the read-only data section and checked indexing.
  Its console formatter also accepts checked text views over frame-allocated
  fixed byte arrays, including nonzero and nested slices, with allocation
  generation, owner-frame, extent and lifetime validation. Byte comparison
  retains the stable `assembly.gap.native-string-compare` gap; slice shapes
  outside the verified checked-token forms remain outside this implemented subset.
  String concatenation is not a current checked-language string operation.
- A verified, target-neutral [machine IR](machine-ir.md) boundary lowers the
  current LIR subset for Linux x86-64 SysV with virtual registers, fixed-register
  constraints, explicit flags, call clobbers and parallel copies. Deterministic
  allocation, an allocated-program verifier and x86-64 encoding feed the
  internal ELF writer through `zi2asm --machine --emit elf`. Native ELF still
  retains a separate direct-LIR prototype while M2 is open.
- The direct Linux x86-64 console veneer emits a static ELF with its own
  `_start`, Linux `write` syscall 1 through a partial-write/`EINTR` retry loop,
  and `exit_group` syscall 231. It has no libc, CRT, `PT_INTERP` or `PT_DYNAMIC`.
  Literal formatting covers strings/text views by byte length, bools, every
  fixed signed/unsigned integer width and `%%`; floats and other platform
  services are named hard gaps.
- `--emit lir` and `--emit map` inspection views. Map ranges are half-open,
  relative to `.text`, and identify function, block and instruction.
  Native always reads the verified, optimized ZX image, even for source input;
  the current required ZX payload strips source origins, so native map origins
  are empty. Source LIR inspection retains origins. Debug attachments and full
  source-to-native mapping remain a hard gap. LIR schema 4 rejects older ZX
  payloads at a rebuild boundary. Textual names are quoted and escaped rather
  than allowed to inject extra lines into the instruction listing. Metadata
  names and paths must be nonempty, NUL-free valid UTF-8; malformed wire metadata
  is rejected before reaching JSON output, while valid Unicode is preserved.
- `--emit zx` always optimizes and writes the deterministic portable executable;
  `--input zx` verifies and consumes it without source or `.zir`.
  Source ELF output serializes and reads that same ZX boundary first, so a ZX
  image gives byte-identical native output through either entry route. No
  standalone `.lirb`, `--emit exec` or `--input exec` route remains. The internal
  instruction codec is only a ZX payload.
- `--run` executes the same verified instructions in the bounded portable
  engine that runs every bundle, with independent fuel, work and frame limits.
- Recoverable errors for unsupported constructs and atomic file replacement;
  rejected programs do not overwrite an existing output.
- ZX production always transforms a separately owned copy of verified instructions
  with exact scalar constant folding, identity-copy propagation, constant-branch
  CFG simplification and nontrapping dead-instruction removal. Each enabled pass
  is verified. Source `--opt --emit lir` exposes the transformed program, while
  `--opt --emit opt-report` emits deterministic JSON transformation counters.
  Trapping operations, ordered effects and executed fuel checkpoints remain
  observable. This is not the complete optimization pipeline or register allocator.

This is a stack-slot backend, **not** an optimizing register allocator. It has
no demonstrated performance advantage. Native `Fuel` records emit no bytes and
appear as zero-length map ranges: execution is unmetered AOT, not the proposed
budgeted ZX instruction engine. Explicit native traps terminate with status 125.

## Build, inspect, execute

From the repository root:

```sh
env -u DISPLAY -u WAYLAND_DISPLAY build-queue -- sh -eu <<'SH'
make assembly JOBS=2
build/bin/zi2asm --root tests/spec --entry assembly_scalar:Answer \
    --emit lir -o build/assembly_scalar.lir tests/spec/assembly_scalar.zi
build/bin/zi2asm --root tests/spec --entry assembly_scalar:Answer \
    --emit map -o build/assembly_scalar.map.json tests/spec/assembly_scalar.zi
build/bin/zi2asm --root tests/spec --entry assembly_scalar:Answer \
    -o build/assembly_scalar tests/spec/assembly_scalar.zi
status=0
build/assembly_scalar || status=$?
test "$status" -eq 42
SH

objdump -d --adjust-vma=-0x400078 build/assembly_scalar \
    > build/assembly_scalar.disasm
```

The address adjustment makes disassembly addresses match the map's text-relative
offsets for this ELF layout. These offsets are neither source line numbers nor
addresses from a running process. `--emit lir -o -` and `--emit map -o -`
write inspection data to standard output; ELF output requires a file path.
`--emit zx -o -` writes portable executable ZX bytes to standard output.
`--diagnostics=json` includes structured diagnostic codes.

Maps report actual ELF file/text/data sizes, file offsets and virtual addresses
in `image`. Scalar images retain the 120-byte text offset; global images use
two program headers and a 176-byte text offset. Use the reported
`text_virtual_address` rather than the scalar example's fixed adjustment when
disassembling another image. Reported addresses describe the linked AOT ELF,
not an observed running process or a future relocatable/JIT code allocation.
`identity` is SHA-256 of the complete actual ELF, including initialized data
and symbol metadata, so a sidecar can be checked against the exact executable.
`variant` and `abi` label the baseline x86-64/internal-ABI implementation.
`optimization_request` identifies the canonical verified-basic optimization
boundary. `--opt` on ZX input does not rerun passes. Maps
encode the image to obtain this identity; they are inspection work, not a
cheap source-only preview or a runtime performance measurement.

Native maps support bounded output without renumbering identities:

```sh
build/bin/zi2asm --input zx --emit map --function-id 0 --block-id 0 \
    -o build/assembly_scalar.block.map.json build/assembly_scalar.zx
build/bin/zi2asm --input zx --emit map --address-begin 35 --address-end 80 \
    -o build/assembly_scalar.address.map.json build/assembly_scalar.zx
```

Function/block IDs refer to the selected lowered program (after `--opt`, if
enabled). A block filter requires a function ID. Address windows are nonempty,
half-open text-relative byte ranges and can intersect a function/block filter.
Matching instructions keep their complete original ranges rather than being
clipped; zero-byte sites are included when their position lies in the window.
Filtered maps retain full extents for selected functions and only referenced
origins plus their parent chains. An empty intersection is explicit, not an
error or an excuse to dump the whole program. `selection`, `selected_ranges`
and `total_ranges` distinguish filtered output from a complete byte map.
These options bound inspection output, not compilation work: the current
encoder still verifies and lays out the complete program.

```sh
build/bin/zi2asm --root tests/spec --entry assembly_scalar:Answer \
    --emit zx -o build/assembly_scalar.zx tests/spec/assembly_scalar.zi
build/bin/zi2asm --input zx --emit lir -o - build/assembly_scalar.zx
build/bin/zi2asm --input zx -o build/assembly_scalar build/assembly_scalar.zx
build/bin/zi2asm --input zx --run --fuel 10000 --work 100000 \
    build/assembly_scalar.zx
build/bin/zi2asm --opt --input zx --emit lir -o build/assembly_scalar.optimized.lir \
    build/assembly_scalar.zx
build/bin/zi2asm --opt --root tests/spec --entry assembly_scalar:Answer \
    --emit opt-report -o build/assembly_scalar.opt.json tests/spec/assembly_scalar.zi
```

Only source `--emit lir` is unoptimized by default. All ZX, portable execution,
native map and ELF production goes through optimization and verified ZX bytes.
Reports require source input and `--opt` explicitly and can also use `-o -`.
Their `work` and `scratch_bytes` fields are bounded compiler-accounting counters,
not elapsed time, process RSS or a performance measurement. A shorter listing or
smaller executable is not evidence of faster execution.

The scalar fixture deliberately returns **42**, not zero. Source and saved-IR
tests require byte-identical lowered dumps, maps and executables. Native map
tests check that ranges cover the exact encoded instruction bytes without gaps.

Retained regressions also compare successful source programs with the C backend
and exercise integer/bool conversion, returning constant loops, loop exits,
non-truncating shift counts and executable permissions on unchanged output.
Generated ELF execution is required on Linux x86-64; other hosts still validate
encoding and inspection data, without claiming target execution.

## Still required

No M2–M6 completion gate is closed by this M0/M1 work. Outstanding language and
runtime work includes resizable owned vectors and other dynamic collections,
their generated cleanup/move paths, nested or reentrant host-to-bundle calls,
foreign C/system services, concurrency and the remaining matrix operations.
The new ZX container still needs complete library composition and capability
metadata cross-checking; native string-comparison instructions, slice shapes
outside the checked-token subset, and arbitrary host trampolines remain gaps.
The remaining M2 integration,
relocatable objects and a general linker, other architectures and OS ABIs,
source/unwind information, profiler/JIT mapping and final cutover remain M2–M6.

Unsupported constructs fail explicitly; there is no silent fallback to C, Go
or another engine inside the portable/native assembly route. The C and
Go backends remain separate source-output routes that can compile platform
binaries when explicitly selected. Mixed signedness comparisons require explicit
casts. The bounded revision-28 ZX reader validates container identities, the
instruction schema, entry and declared capabilities before native lowering.
The labeled physical-Alpha portable-engine follow-up is documented in
[the benchmark report](../bench/zx_compare.md#final-pointer-dispatch-cohort-2026-11-13).
It is not the strict full benchmark campaign, cross-platform execution
validation or installed-tool verification. An interim portable result does not
close the full upgrade's feature or validation gates.

## Implementation and local tests

- [Lowered schema](../cmd/compiler_lowered_ir.zi),
  [verifier](../cmd/compiler_lowered_verify.zi),
  [encoder](../cmd/compiler_lowered_x64.zi).
- [Instruction codec](../cmd/compiler_exec_serial.zi),
  [wire format](lowered-format.md), [portable engine](../cmd/compiler_exec.zi).
- [Executable container binding](../cmd/compiler_exec_link.zi) and
  [format](zx-executable-format.md); [machine IR](../cmd/compiler_machine_ir.zi),
  [lowering](../cmd/compiler_machine_lower.zi) and [verifier](../cmd/compiler_machine_verify.zi).
- [Checked-language adapter](../cmd/zir-asm/zir_asm_lower.c),
  [CLI](../cmd/zir-asm/main.c), [build integration](../assembly.mk).
- [Source/saved-IR integration tests](../tests/assembly_cases.zi),
  [verifier fixtures](../tests/spec/compiler_lowered_verify_test.zi),
  [encoder fixtures](../tests/spec/compiler_lowered_x64_test.zi).
- [Comparison processor](../bench/assembly_compare.zi),
  [report schema and methodology](performance-report.md), and
  [synthetic report tests](../tests/assembly_compare_cases.zi). These validate
  reporting, not a measured compiler/runtime speedup.

```sh
env -u DISPLAY -u WAYLAND_DISPLAY build-queue -- sh -eu <<'SH'
make assembly JOBS=2
sh tests/direct_assembly.sh build/bin/ziran
sh tests/compiler_lowered_verify.sh build/bin/ziran
sh tests/compiler_lowered_x64.sh build/bin/ziran
sh tests/lowered_serial.sh build/bin/ziran
sh tests/lowered_exec.sh build/bin/ziran
sh tests/compiler_opt.sh build/bin/ziran
sh tests/assembly_compare.sh build/bin/ziran
make check check-bootstrap JOBS=2 CHECK_JOBS=2
SH
```

These are local tests, not GitHub CI results. Benchmarks must run directly on
Alpha's physical host, with virtualization rejected. The final
report must include matched before/after ZX startup, throughput, memory and
artifact sizes, raw samples, excluded samples and any regressions; no speedup
is claimed until those measurements exist. The report processor keeps absolute
medians, favorable factors and candidate metric percentage changes distinct;
missing or noncomparable cells remain visible.

## Ten-step completion plan

The steps define completion; [milestones](milestones.md) define the delivery
order, per-row cutover and interim measurement points.

1. [Execution contract and completion matrix](01-contract-and-completion-matrix.md)
2. [Lowered IR and verifier](02-lowered-ir-and-verifier.md)
3. [Complete language lowering](03-language-lowering.md)
4. [Portable instruction engine](04-portable-instruction-engine.md)
5. [ZX format and linking](05-zx-format-and-linking.md)
6. [Optimization and native backend](06-optimization-and-native-backend.md)
7. [Architecture and platform coverage](07-architecture-and-platform-coverage.md)
8. [Assembler, objects and linker](08-assembler-objects-and-linker.md)
9. [Native runtime and integration](09-native-runtime-and-integration.md)
10. [Inspection, performance and cutover](10-inspection-performance-and-cutover.md)
