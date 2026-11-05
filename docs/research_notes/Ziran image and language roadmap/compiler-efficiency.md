# Compiler efficiency track for Ziran codecs

## Scope and evidence status

This note preserves the completed compiler research from managed session
`glm-ziran-ziran-codec-compiler-research`, task
`514fcc7de39aa65d1a97a3ffdf99d32e8ef4`. The worker reached its tool-call
ceiling before writing this file, but supplied a completed inline result; the
parent verified the handoff and this note records it without adding a new
research round. No compiler benchmark was run, so every performance proposal
below is an evidence-gated hypothesis rather than a measured result.

The compiler currently emits generated C, generated Go, portable `.zx`, and a
WASM subset. It models fixed-width scalars, fixed arrays, checked slices, and
owned `Vec`; it has checked GNU `#asm` alternatives, CPU guards, and cached
dispatch in `cmd/zir/zir_asm.c`, `cmd/zir/zir_asm.h`, and
`tests/spec/native_assembly_test.zi`. Go has more restrictive ABI0 assembly in
`cmd/zir-go/zir_go_asm.c` and `cmd/zir-go/zir_go_amd64.c`. C compilation is
configured in `cmd/zir-c/zir_c_build.c` and defaults to `-O2`.

No first-class packed SIMD IR/type, compiler-owned SIMD intrinsic, proved
no-alias fact, or language-level bounds-elimination pass was found. Downstream
C or Go compilers may still optimize some checks. Ziran indexing guards are in
`include/zir_slice.h` and `include/zir_bounds.h`. `TypeLayout` in
`cmd/zir/zir_check.c` uses a fixed eight-byte logical pointer model; native,
portable, and 32-bit ABI use needs an audit, but that mismatch is not yet a
proved bug.

## Current parallel behavior and tests

C `#parallel` uses workers. Go and portable `.zx` currently document a serial
downgrade. `docs/FEATURES.json` and `docs/IMPLEMENTATION_STATUS.md` are the
source of truth for that status. Existing relevant suites are
`tests/assembly_features.sh`, `tests/assembly_blocks.sh`,
`tests/go_direct_assembly.sh`, `tests/go_assembly.sh`,
`tests/generic_assembly.sh`, `tests/parallel.sh`,
`tests/parallel_captures.sh`, and `tests/parallel_results.sh`. Existing
benchmark infrastructure records command lines, versions, hashes, hardware,
and checked outputs, but it does not establish general C/Go performance parity.

## Roadmap

1. Measure generated C, generated Go, portable `.zx`, and the WASM subset on
   representative integer buffer kernels before adding language features.
   Record disassembly, input bounds, alignment, target ABI, compiler version,
   cold/warm behavior, allocations, and checked-output equality.
2. Add target and ABI capability metadata covering pointer width, address
   space, vector registers, alignment, calling convention, assembly dialect,
   feature guards, and legal lowering forms. Audit the logical eight-byte
   pointer model against native and 32-bit targets before relying on layout
   assumptions.
3. Add packed-vector semantics and compiler-owned intrinsics only after a
   measured kernel need exists. Portable `.zi` scalar semantics must remain
   exact. Intrinsics should lower per target and cache dispatch, rather than
   exposing one backend's spelling as language semantics.
4. Prove bounds and alias facts in `.zir`, then remove only checks proven
   redundant. A downstream compiler's optimization is not a Ziran guarantee.
5. Add allocation-free leaf-loop contracts, bounded worker queues, and
   cancellation/backpressure tests. Go and `.zx` should either preserve stated
   serial semantics or gain explicit capability metadata; they must not
   silently claim C-level parallelism.
6. Permit literal checked `#asm` or compiler lowering only when machine-code
   inspection and a differential oracle prove that compiler output cannot meet
   a measured kernel gate. Every feature variant needs native/source/IR parity,
   `objdump` or `llvm-objdump` inspection, and randomized boundary vectors.

The priority is therefore measurement first, then portable packed-vector
semantics, compiler-owned lowering and dispatch, sound optimization, and only
finally literal assembly. This keeps codec source authored in `.zi`, supports
non-UI users, preserves portable semantics, and does not invent speedups before
machine-code evidence exists.
