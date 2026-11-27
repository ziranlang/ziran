# Implementation status

This reference covers Ziran 0.6.0 and Unreleased work on master. Query the
installed tool with `ziran version`, `ziran features --json` and
`ziran capabilities --json` for its exact contracts and target limits.

## Compilation and execution

Source → ZIR → LIR → optimized LIR → ZX → native (ELF/PE, x86-64/AArch64/ARMv7). WebAssembly lowers from verified ZX through `zx2wasm`. C and Go export from ZIR through `zi2c` and `zi2go`.

`ziran build FILE.zi` defaults to direct native output. Explicit targets are
`--target=c`, `--target=go`, `--target=wasm` and `--target=zx`.
`ziran run FILE.zi` checks and runs the entry; Linux x86-64, AArch64 and ARMv7
use native execution. Native lowering failures report a named gap before
execution. They do not retry on the instruction engine.

ZX is revision 29 executable output. `zi2zx --emit lir` exposes the linked
inspection view; `--opt --emit lir` exposes optimized LIR and
`--opt --emit opt-report` reports optimizations. Optimization precedes ZX
emission. `zx2native` consumes decoded and verified ZX; `zx2wasm` emits wasm32.
ZX retains `#asm` templates alongside portable bodies. Native targets select
compatible implementations at compilation or load. The engine and Wasm use
portable bodies and reject target-only programs before initialization.
See [ZX](ZX.md), [ZIR](ZIR.md) and [architecture](ARCHITECTURE.md).

## Language and library reference

The checker supports procedures, records, enums, generic specialization,
imports, fixed arrays, slices, vectors, compile-time execution, laws and
explicit foreign bindings within the contracts reported by `ziran features`.
A checked program can still use operations unavailable on its selected target;
check the actual target before distributing it.

- [Language reference](LANGUAGE_DIRECTION.md)
- [Compiler and standard library guide](USAGE.md)
- [Owned values](OWNED_VALUES.md)
- [Laws and proofs](LAWS.md)
- [Unicode](UNICODE.md)
- [Packages](PACKAGES.md)
- [Feature registry](FEATURES.md)
- [Authoring workflow](AUTHORING.md)

## Runtime safety

Safety is manual by default. `--checks` opts into build-wide runtime checks;
`#checks` retains bounds checks in selected regions. The instruction engine
always checks. Generated Go also has its own runtime checks. These mechanisms
have different coverage; see [checks](USAGE.md#checks).

## Learning

Native builds record a local corpus by default. `ziran learn off` disables
recording; `ZIRAN_LEARN=off` overrides it for a command. Benchmarks run with
learning off. C-teacher verification and policy training are offline commands;
normal builds use the checked-in policy table. See [learning](ZX.md#learning).
