# Native x86-64 aggregate value ABI

Status: implemented for verified direct calls between functions in one
`LirProgram`. This is a portable-internal ABI used by the opt-in Linux x86-64
encoder. It is not the System V aggregate ABI and is not an external calling
convention.

## Scope

`LirX64Encode` and `LirX64Elf` select this ABI when a function parameter,
result, or SSA value is a record or fixed array. Every generated portable
function in that program then uses the convention below. A scalar-only program
keeps the existing register convention and its existing instruction bytes.

The process entry still has no parameters and must return a scalar process
status. Aggregate entry results are unsupported. Named ELF function symbols
describe real code ranges for inspection; their presence does not make these
internal functions callable through the platform C or System V ABI.

Only `LirCalling.Portable`, non-variadic, defined functions are accepted.
`C`, `System`, `Host`, external and variadic functions remain explicitly
unsupported. Indirect calls, function values, callbacks, globals and memory
operations are outside this ABI. A pointer null is supported as an eight-byte
value, including inside an aggregate; function null remains outside the native
surface.

## Value layout

Values use the portable executor's recursive packed layout:

| LIR value | Size and representation |
| --- | --- |
| `bool` | one byte, `0` or `1` |
| fixed integer | `bits / 8` little-endian bytes |
| `f32`, `f64` | four or eight raw IEEE-754 bytes |
| pointer | eight little-endian bytes |
| fixed array | `count` consecutive element layouts |
| record | field layouts concatenated in declaration order |
| empty record or zero-length array | zero bytes |

There is no field or tail padding. Nested records and arrays apply the same rule
recursively. Aggregate construction concatenates operands, extraction copies
the selected byte interval, and insertion copies the source aggregate before
replacing the selected interval. These are value copies and never aliases.

Floating-point copies preserve every bit, including signed zero and NaN
payloads. Scalar operations continue to use the existing normalization,
comparison, trap and conversion rules; aggregate storage does not reinterpret
their bits.

## Direct calls

`rdi` points to a packed argument block containing parameters in signature
order. A no-argument call supplies a null `rdi`. The caller first copies every
operand into its frame scratch area, so argument preparation cannot destroy a
live source or a parallel-copy cycle. The callee copies the packed parameters
into its own SSA slots in the prologue.

Results use these locations:

| Result | Location |
| --- | --- |
| `void` | none |
| bool, integer or pointer | normalized `rax` |
| `f32` or `f64` | `xmm0`, with the declared width |
| record or fixed array | caller-owned packed storage addressed by `rsi` |

An aggregate-returning callee saves `rsi` in its frame before executing any
nested call. On return it copies the result SSA value to that address. This
supports recursive calls, nested aggregates, mixed scalar/aggregate signatures
and signatures whose packed values exceed the platform argument-register
limits. It does not classify values into System V INTEGER, SSE or MEMORY
classes, so no external interoperability is implied.

Generated calls obey the x86-64 stack rule: `rsp` is 16-byte aligned before
`call`. Every SSA value has distinct frame storage sized by its packed type.
Each frame has one reusable scratch interval large enough for its widest block
edge or direct-call argument block. Aggregate copies use `rep movsb`; generated
code assumes the ABI-required clear direction flag and does not change it.

## Control flow and calls

Block arguments are simultaneous assignments. The encoder copies all edge
arguments to scratch first and then copies scratch to the destination block
parameters. This handles scalar, aggregate and mixed cycles without ordering
the assignments or using a source slot as temporary storage.

All live SSA values reside in distinct frame slots across a call. Caller-saved
register clobbers therefore cannot lose live values, including aggregate
values used after recursive calls. Scalar and float results are stored back to
their result slots immediately; aggregate results are written directly to the
result slot selected by the caller.

## Encoding, maps and limits

The encoder verifies the complete `LirProgram` before planning layouts. Type
sizes, record-member offsets, frame sizes and maximum scratch intervals are
computed with checked arithmetic and bounded vectors. A layout or final code
size above the encoder's signed 32-bit displacement limit returns
`LirProblem.Limit`.

Measurement and validation passes run before any output byte is written. A
zero-length output reports the required size. A short byte buffer or range
buffer reports that size and leaves the supplied buffer unchanged.

Native ranges are emitted from the same instruction pass as the final bytes.
They cover the bootstrap, trap, each function prologue and each LIR instruction
as contiguous actual byte intervals. ELF function symbols start at the
corresponding prologue and end at the next function start or the end of code;
their `st_size` is that exact difference.

`tests/spec/compiler_lowered_x64_test.zi` executes generated ELF files for
nested records and arrays, null fields, extraction and insertion, aggregate
parallel-copy cycles, recursive aggregate results, mixed calls beyond register
limits, live values across calls, signed-zero and NaN payloads, and unsigned
integer/float conversion. The same cases exercise deterministic bytes, exact
range coverage, ELF symbol extents and short-buffer atomicity. A separately
compiled C oracle checks the mixed-call result without asserting C layout or
ABI compatibility.
