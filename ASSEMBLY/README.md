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
  and record-returning direct calls in lowered IR. Array indices currently must
  be literals. Aggregate native encoding is not implemented yet.
- Portable procedure values, arguments/results and indirect calls through local
  slots, record fields and fixed-array elements in lowered IR. Procedure slots
  have explicit signatures; native and foreign callback integration remain
  separate work.
- Block-parameter SSA with simultaneous edge copies, explicit ordered effects,
  copied source origins, and a bounded verifier that rejects invalid definitions,
  dominance, CFG edges, types, and effect order.
- Deterministic raw x86-64 encoding and ELF output with `.text`, `.strtab` and
  `.symtab` sections. Startup, trap and `module:function` symbols refer to the
  actual linked function bytes, so final disassembly has meaningful labels.
- `--emit lir` and `--emit map` inspection views. Map ranges are half-open,
  relative to `.text`, and identify function, block, instruction and source.
  The map also records actual function extents, total text size and the complete
  source-span/origin-parent table. Textual names are quoted and escaped rather
  than allowed to inject extra lines into the instruction listing.
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

This is a stack-slot baseline, **not** an optimizing register allocator. It has
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

```sh
build/bin/zi2asm --root tests/spec --entry assembly_scalar:Answer \
    --emit exec -o build/assembly_scalar.lirb tests/spec/assembly_scalar.zi
build/bin/zi2asm --input exec --emit lir -o - build/assembly_scalar.lirb
build/bin/zi2asm --input exec -o build/assembly_scalar build/assembly_scalar.lirb
build/bin/zi2asm --input exec --run --fuel 10000 --work 100000 \
    build/assembly_scalar.lirb
```

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
includes complete language lowering (globals, pointers, dynamic aggregates,
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
installed-tool verification is claimed. Work remains uncommitted while the
requested feature set and required validation remain incomplete.

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

```sh
env -u DISPLAY -u WAYLAND_DISPLAY build-queue -- sh -eu <<'SH'
make assembly JOBS=2
sh tests/direct_assembly.sh build/bin/ziran
sh tests/compiler_lowered_verify.sh build/bin/ziran
sh tests/compiler_lowered_x64.sh build/bin/ziran
sh tests/lowered_serial.sh build/bin/ziran
sh tests/lowered_exec.sh build/bin/ziran
make check check-bootstrap JOBS=2 CHECK_JOBS=2
SH
```

These are local tests, not GitHub CI results. Benchmarks must run directly on
Alpha's physical host, never the personal VM or current desktop. The final
report must include matched before/after ZX startup, throughput, memory and
artifact sizes, raw samples, excluded samples and any regressions; no speedup
is claimed until those measurements exist.

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
