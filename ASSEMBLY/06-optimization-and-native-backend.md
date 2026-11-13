# Step 06 — Optimize executable IR and implement the first native backend

Status: in progress; [verified scalar optimization](optimization-pipeline.md) and a [stack-slot scalar/aggregate encoder](README.md), with a [packed internal aggregate ABI](native-aggregate-abi.md). The [target-neutral machine IR](machine-ir.md) boundary, its verifier, deterministic liveness, linear-scan [register allocation](machine-ir.md#register-allocation) with splitting and spilling, a separate allocated-program verifier, and a [direct x86-64 machine-code encoder](machine-ir.md#machine-code-encoder) for verified allocated programs exist for Linux x86-64 SysV. `zi2asm --machine --emit elf` now carries that allocated image through the internal ELF writer and never invokes an external compiler, assembler, linker or alternate engine; native ZX loading is still open. Its native call ABI passes and returns general-register values only: float and vector call arguments and results and stack-passed arguments are [unsupported forms](machine-ir.md#supported-call-forms) rejected before encoding, not partially implemented. The full optimizer, the full SysV call ABI and external System V aggregate interoperability remain pending. The direct LIR encoder now receives fixes only; new native features land on the machine-IR path ([milestone](milestones.md) M2).

Direct native code here means Ziran's own encoders write machine code (and, on the direct LIR path, the ELF executable) from verified IR, without generated C, an assembler, a C compiler or a system linker. The C and Go targets also produce runnable binaries, but they are separate generated-code exporters with their own toolchains; they are never a fallback for an unsupported native form ([gap policy](gap-policy.md), [distinction](machine-ir.md#direct-native-code-versus-generated-code-exporters)). Depends on stable contracts from [steps 02](02-lowered-ir-and-verifier.md)–[05](05-zx-format-and-linking.md). Next: [target coverage](07-architecture-and-platform-coverage.md).

## Objective

The `zi2asm` CLI now runs verified LIR optimization before writing portable ZX.
Both native entry routes read and verify those bytes before selecting direct
LIR or machine-IR target lowering. `--opt` selects optimized source inspection,
not a second optimization pass on ZX input. `--emit wasm` lowers the same
verified ZX to a wasm32 module. Source/debug origin attachments, default
runtime cutover and moving `zi2wasm` and the bundle Wasm tier onto that
lowering remain gaps.

Implement a native backend shared by standalone compilation and native ZX execution. Start with Linux x86-64 System V as the first execution milestone, then complete the required target matrix in step 07. A first working return-value executable is not completion of this step.

## Existing integration points

- [Scalar contracts](../cmd/zir/zir_scalar.h), [range-analysis behavior](../cmd/zir/zir_emit_range.c), and [SIMD behavior](../cmd/zir/zir_simd.c).
- [Native planning](../cmd/zir/zir_native.c).
- [Existing Go assembly translator](../cmd/zir-go/zir_go_amd64.c) for operand/ABI lessons only; Go ABI0 and its runtime assumptions are not Ziran's native ABI.
- [Native and SIMD tests](../tests/simd_native.sh).

Implemented machine-IR modules: `cmd/compiler_machine_ir.zi`, `cmd/compiler_machine_lower.zi`, `cmd/compiler_machine_verify.zi`, `cmd/compiler_abi.zi` (the only module that names target registers), `cmd/compiler_regalloc.zi`, `cmd/compiler_machine_verify_allocated.zi`, and `cmd/compiler_x86_64.zi` with the shared byte encoders in `cmd/compiler_x86_bytes.zi`. Proposed: `cmd/compiler_opt.zi`.

## Implementation work

- [ ] Add a pass manager with explicit preconditions, preserved analyses, verification hooks, timing/counter collection, and deterministic before/after dumps.
- [ ] Implement constant folding, copy propagation, unreachable-block removal, dead instruction elimination, and CFG simplification with exact numeric/effect semantics.
- [ ] Add range propagation, redundant-check elimination, common subexpression elimination, and conservative load/store optimization. Invalidate facts on aliasing writes and effectful calls.
- [ ] Implement inlining with recursion/growth limits, source-origin chains, and the existing `#inline`/`#cold` meaning. Include cross-module decisions in dependency/cache identities.
- [ ] Add loop optimizations only when arithmetic, aliasing, exit, fuel, and trap conditions permit them. Do not treat signed overflow, float reassociation, or host side effects as compiler conveniences.
- [ ] Lower to target instructions while retaining virtual registers. Model fixed registers, tied operands, condition flags, call clobbers, vector register classes, addressing modes, and literal pools explicitly.
- [x] Implement liveness and a register allocator with splitting/spilling, call-aware live ranges, parallel-copy resolution, and deterministic output. Emit reasons and locations for every spill/reload. ([allocation](machine-ir.md#register-allocation), `tests/regalloc.sh`)
- [x] Verify the allocated program: no overlapping live assignments, missing definitions, broken ties, clobbered live values, invalid register classes, or unresolved parallel copies. It also rejects unimplemented call forms and relocation addends the target cannot hold, so they never reach an encoder; allocation publishes only a verified program and leaves the owner's input intact on any failure.
- [ ] Implement SysV argument/result classification, stack alignment, caller/callee-saved registers, aggregate passing, varargs, callbacks, indirect calls, and externally named symbols.
- [ ] Generate prologues/epilogues, frame layout, unwind information, stack/resource checks, and target-correct returns. Keep source/inline locations through all inserted instructions.
- [ ] Implement baseline scalar integer/float instruction selection first, then every required SIMD lane operation and supported CPU variant. Preserve a runnable baseline when optional features are unavailable.
- [ ] Integrate typed native assembly operands and clobbers with allocation. Implement the supported GNU template/constraint surface or issue explicit unsupported-form diagnostics; never drop a selected assembly block silently.
- [ ] Give separately selected portable and native implementations identical external semantics. Validate result/operand layouts; do not assume that an annotated template is proved equivalent merely because another body is checked. A missing native variant is a Gap error, not permission to execute the portable implementation.
- [ ] Implement runtime calls and parallel-region helpers through the same typed ABI machinery, with stable entry points for the native engine.

## Tests

Add independent native-backend fixtures for register pressure, spills across calls, fixed-register division/shifts, flags, cyclic copies, float edge cases, stack alignment, aggregate ABI classes, varargs, and C callbacks.

Current machine-IR local tests: `tests/machine_ir.sh` covers lowering, ABI constraints, malformed-MIR rejection, exact x86-64 encoded bytes, relocations, fuel and traps, and encoder negatives including float and stack call forms and out-of-range global-address addends rejected before encoding. `tests/regalloc.sh` covers allocation, the allocated verifier, incoming-argument order, and late failures that leave the owner's program intact. They use `--target=c` only to run the test programs.

Compare optimized/unoptimized new-native output with the portable instruction engine and existing C execution. Use external handwritten C/assembly ABI fixtures as independent callers/callees where practical. Inspect actual encoded instructions after step 08; text snapshots alone cannot validate encoding or execution.

Run all builds through `build-queue --`. Performance measurements occur only
directly on Alpha's physical host, with raw samples and the repository's
symmetric 1.5×IQR rule. Reject virtualization and leave measurements pending
when Alpha is unavailable rather than substituting another host.

## Exit gate

All required Linux x86-64 feature cells execute with the new backend. Debug/release, source/saved IR, standalone/native ZX entry adapters, callbacks, SIMD, and spill-heavy code agree with their independent expectations. Optimization diagnostics identify which pass changed code and why. Any remaining provisional assembler/linker dependency is explicitly temporary until step 08.

## Handoff

Deliver shared machine/ABI interfaces, the complete first-target implementation, allocation verifier, target fixture corpus, and optimization remarks. Freeze common backend contracts before parallel target implementation to avoid competing register/ABI models.
