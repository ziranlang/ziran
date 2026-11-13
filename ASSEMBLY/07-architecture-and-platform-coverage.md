# Step 07 — Complete architecture, ABI, and platform coverage

Status: planned. Depends on [step 06](06-optimization-and-native-backend.md). Coordinates with [encoding/linking](08-assembler-objects-and-linker.md) and [runtime integration](09-native-runtime-and-integration.md).

## Objective

Run one portable executable program across the maintained platform matrix while allowing explicit CPU variants. Match existing promised coverage rather than declaring a Linux-only backend to be the completed ZX upgrade.

## Existing integration points

- [Current wasm lowering](../cmd/zir-wasm/zir_wasm_lower.c) and [wasm bundle integration](../cmd/zir/zir_zx_wasm.c).
- [Browser execution integration](../cmd/zir/zir_zx_wasm_web.c).
- [Target diagnostics](../tests/target_diagnostics.sh), [ARM/CPU feature tests](../tests/assembly_features.sh), and [wasm tests](../tests/zx_wasm.sh).
- [Platform and runtime coverage documentation](../docs/IMPLEMENTATION_STATUS.md).

Proposed modules: `cmd/compiler_target.zi`, `cmd/compiler_aarch64.zi`, `cmd/compiler_arm.zi`, `cmd/compiler_wasm.zi`, and platform-specific ABI modules under `cmd/`. Keep target tables explicit and dynamically sized where appropriate.

## Implementation work

- [ ] Establish exact target triples, pointer widths, endianness, data models, object formats, stack rules, symbol rules, and feature baselines from the step-01 matrix.
- [ ] Implement AArch64 instruction selection, calling conventions, stack frames, relocatable addresses, NEON operations, literal placement, branch ranges, and platform-reserved registers.
- [ ] Implement the maintained 32-bit ARM target: ARM/Thumb policy, instruction availability, 64-bit scalar operations, soft/hard-float ABI distinctions, alignment, NEON variants, interworking, and veneers.
- [ ] Complete x86-64 Windows and macOS ABI differences, including Windows shadow space/unwind requirements and platform-specific symbol/TLS/layout rules. Include additional x86 platform rows only where the matrix requires them.
- [ ] Complete Apple ARM64 rules, Android ABI/API requirements, TLS, dynamic loading, instruction-cache synchronization, and platform limits on executable memory.
- [ ] Add required 32-bit x86 support if current maintained targets promise it; do not hide a missing target behind the x86-64 implementation. New RISC-V support is separately scoped unless explicitly included in step 01.
- [ ] Lower the same portable IR directly to WebAssembly instructions, function tables, structured control flow, data, imports, and exports. Preserve numeric traps/conversions and the declared memory model at the wasm boundary.
- [ ] Support browser AOT generation and the portable instruction engine when runtime native compilation is unavailable. Do not keep generated C/Emscripten as the hidden code generator for the new ZX instruction path.
- [ ] Implement baseline and optional SIMD modes, including legal wider-vector decomposition. Match lane results with the portable engine; explicit native variants select only on compatible CPUs/OS state.
- [ ] Centralize feature discovery and target requirement matching. Cross-compilation must use the requested target description, never the build machine's CPU flags.
- [ ] Keep OS APIs in typed host capabilities or native foreign adapters. A Windows/Linux/macOS service binding is distinct from AVX/NEON instruction availability.
- [ ] Define exact failures for absent host facilities, unavailable executable memory, missing CPU requirements, unsupported native extensions, and unavailable platform toolchains.
- [ ] Supply unwind/debug mappings and profiler registration hooks for every native target, not just Linux x86-64.

## Execution matrix and tests

For every required CPU/OS/ABI row, record:

| Evidence | Required outcome |
| --- | --- |
| Source and saved `.zir` | Equivalent lowered instructions and behavior |
| Same portable `.zx` bytes | Successful supported execution across applicable hosts |
| Native AOT | Real executable launch, correct ABI interactions, expected results |
| Native ZX or documented interpreter-only host | Correct loader/capability/state behavior |
| CPU features enabled and masked | Correct declared-variant dispatch; stable preflight error when no implementation matches |
| Debug/unwind/profile data | Correct function and source attribution |
| Negative target cases | Clear diagnostics before invalid execution |

Run semantic tests under emulation when useful and on real hardware where ABI, OS integration, or CPU feature behavior requires it. Test phones on the owner-authorized USB Moto; preserve Wireless Debugging. GUI tests use private displays and only their own processes.

Emulation, cross-compilation, a lower-feature implementation, or another engine
does not close the selected target/feature cell. Missing target hardware leaves
the cell Unknown; missing implementation leaves it Gap.

Benchmarks run directly on Alpha's physical host only; the runner rejects
virtualization. Emulated correctness results are not native performance
measurements. If a required host is unavailable, retain its matrix cells as
incomplete rather than reporting 100% coverage.

## Exit gate

Every required row has execution evidence and every opcode has either correct target lowering or a documented portable implementation. The same portable bundle does not require platform-specific rebuilding to execute its baseline behavior. Platform-specific variants and capabilities remain explicit.

## Handoff

Deliver target tables, ABI/encoding requirements, cross-platform fixtures, hardware limitations, and execution receipts to steps 08–10. Do not infer a platform passed merely because another platform uses the same CPU family.
