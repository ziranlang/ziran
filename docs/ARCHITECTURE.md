# Ziran architecture

Ziran owns the language, compiler, standard library and executable runtime.
Applications own product code; Kryon owns UI widgets and rendering.

## Compiler pipeline

Source → ZIR → LIR → optimized LIR → ZX → native (ELF/PE, x86-64/AArch64/ARMv7). WebAssembly lowers from verified ZX through `zx2wasm`. C and Go export from ZIR through `zi2c` and `zi2go`.

The frontend checks imports, types, expressions, laws and ownership rules before
writing `.zir`. The linker includes reachable code, lowers to LIR, verifies and
optimizes it, and writes verified ZX instructions. The native backend consumes
those instructions and applies CPU lowering and platform ABI services.
`ziran build FILE.zi` selects native output by default. `zx2native --emit elf`
writes ELF on Linux and PE32+ on Windows; supported CPU/platform combinations
and service limits are diagnosed by the selected backend.

Checked procedure specializations carry ordered assembly templates alongside
portable LIR. The verifier checks their bindings against the common signature.
The native loader chooses an eligible template during compilation and keys the
compiled-code cache by executable identity and usable target features. When
no optional template matches, the same backend compiles the portable LIR.
A target-only procedure requires a matching native target before initialization.

In-process native execution is the default on Linux x86-64 and on AArch64 and
ARMv7 under Linux or Android. Use `zi2zx run --backend engine` to select the instruction
engine explicitly. A failed native lowering stops at preflight. Browser
execution uses the Wasm tier. See [ZX](ZX.md).

Native instances retain globals, heap and checked pointer slots between calls.
Temporary frame storage lives only during an active outermost invocation,
including its reentrant host callbacks, and is released on return or trap.
Idle instances do not reserve the frame limit, keeping multiple bundles usable
in a 32-bit address space. The full limit and pointer lifetime checks still
apply during execution.

## Safety and capabilities

Safety is manual by default. `--checks` enables build-wide runtime checking;
`#checks` retains bounds checks for a selected region. The instruction engine
always checks. See [the guide](USAGE.md#checks).

Host capabilities describe typed services required by linked code. Host
bindings implement them; CPU instruction lowering and ABI veneers implement
the machine target. A missing binding or unsupported instruction is an error.
Programs without graphical imports run without a graphical host.

## Shared checked representation

C and Go exporters consume the same checked [ZIR](ZIR.md) model. All routes
must preserve observable language behavior within their supported subset.
Contexts own parsed declarations, interned text, source spans and scratch
storage. Linked programs retain contexts whose data they borrow. See
[compiler embedding](COMPILER_API.md) for lifetime and concurrency rules.
