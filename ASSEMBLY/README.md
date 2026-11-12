# Direct assembly implementation

## Status: experimental backend, not the completed upgrade

The ten-step plan is **not finished**. There is now an opt-in `zi2asm` tool
that lowers checked source or saved `.zir` to verified, typed SSA and
directly encodes a Linux x86-64 ELF executable. The existing C, Go, wasm and
ZX paths are unchanged. In particular, `.zx` has **not** been replaced by the
new instruction format.

The prototype's actual path is:

```text
.zi or .zir -> canonical checker -> lowered IR -> verifier -> .lirb
                                        |                      |
                                  --emit lir          --input exec
                                        |                      |
                                        +---- x86-64 encoder ---+
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
  block transfers in lowered IR and native ELF output. Array indices currently
  must be literals. Aggregate programs use the explicitly documented
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
  the preceding value even when a call mutates an alias. Address-taking of local
  storage still requires further lowering. Dynamic fixed-array element addresses
  rooted in globals or checked pointers are bounds-checked, including nested
  records/arrays and addresses returned across procedure calls.
  Retained tests compare C, Go and explicit old-VM execution where the old VM
  supports the construct. A call-valued dereference assignment has C/Go/new-engine
  parity, but is outside the old VM's portable subset. Native global data is
  supported; source aggregate/float initializers and runtime module initializers
  still require lowering. Runtime initializers are rejected even when their value is
  unused, rather than silently dropping their effects.
- Block-parameter SSA with simultaneous edge copies, explicit ordered effects,
  copied source origins, and a bounded verifier that rejects invalid definitions,
  dominance, CFG edges, types, and effect order.
- Deterministic raw x86-64 encoding and ELF output with `.text`, `.strtab` and
  `.symtab` sections. Startup, trap and `module:function` symbols refer to the
  actual linked function bytes, so final disassembly has meaningful labels.
- Static globals and checked global/record-field pointer slots in direct ELF,
  with zero/scalar initialization, packed aggregate loads/stores, pointer
  parameters/results and separate RX text/RW data segments. Native dispatch
  validates tokens and exact pointee types; it is currently a bounded linear
  scan, not an optimized pointer-access path. No generated-C bridge is used.
- `--emit lir` and `--emit map` inspection views. Map ranges are half-open,
  relative to `.text`, and identify function, block, instruction and source.
  The map also records actual function extents, total text size and the complete
  source-position/origin-parent table, including checked end-line/end-column
  coordinates. Unavailable end positions are `0/0`; unavailable source byte
  offsets are `-1/-1`, not fabricated zero-length spans. LIR schema 2 rejects
  older `.lirb` artifacts at a rebuild boundary. Textual names are quoted and escaped rather
  than allowed to inject extra lines into the instruction listing. Metadata
  names and paths must be nonempty, NUL-free valid UTF-8; malformed wire metadata
  is rejected before reaching JSON output, while valid Unicode is preserved.
- `--emit exec` writes deterministic `.lirb` instructions; `--input exec` verifies
  and consumes them without loading source or checked `.zir`. The same decoded
  instructions produce listings, native maps and ELF bytes. This standalone
  instruction format is not the replacement ZX container yet.
- `--run` executes the same verified instructions in a bounded portable engine,
  with independent fuel, work and frame limits. Its explicit frames and parallel
  block transfers also handle the currently lowered records and fixed arrays.
  This is an opt-in reference path, not a completed ZX runtime replacement.
- Recoverable errors for unsupported constructs and atomic file replacement;
  rejected programs do not overwrite an existing output.
- Opt-in `--opt` transforms a separately owned copy of the verified instructions
  with exact scalar constant folding, identity-copy propagation, constant-branch
  CFG simplification and nontrapping dead-instruction removal. Each enabled pass
  is verified. `--emit lir` exposes the selected transformed program, while
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
`--emit exec -o -` writes binary instruction bytes to standard output.
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
`optimization_request` records this invocation's `--opt` policy; `none` does
not prove that an input `.lirb` was never optimized by its producer. Maps now
encode the image to obtain this identity; they are inspection work, not a
cheap source-only preview or a runtime performance measurement.

Native maps support bounded output without renumbering identities:

```sh
build/bin/zi2asm --input exec --emit map --function-id 0 --block-id 0 \
    -o build/assembly_scalar.block.map.json build/assembly_scalar.lirb
build/bin/zi2asm --input exec --emit map --address-begin 35 --address-end 80 \
    -o build/assembly_scalar.address.map.json build/assembly_scalar.lirb
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
    --emit exec -o build/assembly_scalar.lirb tests/spec/assembly_scalar.zi
build/bin/zi2asm --input exec --emit lir -o - build/assembly_scalar.lirb
build/bin/zi2asm --input exec -o build/assembly_scalar build/assembly_scalar.lirb
build/bin/zi2asm --input exec --run --fuel 10000 --work 100000 \
    build/assembly_scalar.lirb
build/bin/zi2asm --opt --input exec --emit lir -o build/assembly_scalar.optimized.lir \
    build/assembly_scalar.lirb
build/bin/zi2asm --opt --input exec --emit opt-report -o build/assembly_scalar.opt.json \
    build/assembly_scalar.lirb
```

Without `--opt`, the existing unoptimized output remains the default. Optimization
can precede portable execution or any output kind, including standalone native
ELF encoding. Reports require `--opt` explicitly and can also use `-o -`.
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

No full-plan completion gate is closed by this prototype. Outstanding work
includes complete language lowering (full global initialization, pointers, dynamic aggregates,
strings, cleanup, allocation, collection loops, foreign/host services and concurrency), the
complete portable runtime and new ZX decoder/container, optimization,
machine IR and register allocation, relocatable objects and a real linker,
other architectures and OS ABIs, shared native-ZX execution, runtime integration,
source/unwind debug information, profiler/JIT mapping and the final compatibility cutover.

Unsupported constructs fail explicitly; there is no silent fallback to C or
TinyCC inside `zi2asm`. Mixed signedness comparisons currently require explicit
casts. The bounded `.lirb` decoder validates the current instruction schema;
it is not the future ZX container verifier, linker or capability boundary.
No Alpha benchmark campaign, cross-platform execution validation or
installed-tool verification is claimed. A committed prototype milestone does
not close the full upgrade's feature or validation gates.

## Implementation and local tests

- [Lowered schema](../cmd/compiler_lowered_ir.zi),
  [verifier](../cmd/compiler_lowered_verify.zi),
  [encoder](../cmd/compiler_lowered_x64.zi).
- [Instruction codec](../cmd/compiler_exec_serial.zi),
  [wire format](lowered-format.md), [portable engine](../cmd/compiler_exec.zi).
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
Alpha's physical host, never the personal VM or current desktop. The final
report must include matched before/after ZX startup, throughput, memory and
artifact sizes, raw samples, excluded samples and any regressions; no speedup
is claimed until those measurements exist. The report processor keeps absolute
medians, favorable factors and candidate metric percentage changes distinct;
missing or noncomparable cells remain visible. The codec, verifier and execution
oracle tests explicitly select `--vm`; automatic/native ZX execution is not
evidence that the portable VM ran.

## Ten-step completion plan

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
