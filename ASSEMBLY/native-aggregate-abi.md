# Native x86-64 packed portable value ABI

Status: implemented for verified direct and typed indirect calls, static
globals, and checked global memory operations in one `LirProgram`. This is a
portable-internal ABI used by the opt-in Linux x86-64 encoder. It is not the
System V aggregate ABI and is not an external calling convention.

## Scope

`LirX64Encode` and `LirX64Elf` select this ABI when a program contains a
global, a checked memory instruction, a function type, or a function
parameter, result, or SSA value that is a record or fixed array. Every
generated portable function in that program then uses the convention below. A
previously accepted scalar-only program keeps the existing register convention
and byte-identical instruction encoding.

The process entry still has no parameters and must return a scalar process
status. Aggregate entry results are unsupported. Named ELF function symbols
describe real code ranges for inspection; their presence does not make these
internal functions callable through the platform C or System V ABI.

Only `LirCalling.Portable`, non-variadic, defined functions are accepted.
`C`, `System`, `Host`, external and variadic functions remain explicitly
unsupported. This ABI does not accept untyped host callbacks or claim that its
function values are C or System V function pointers. Dynamic global
initializers, copies over runtime-selected byte ranges, and zeroing over
runtime-selected byte ranges remain outside this native surface. The source
lowering currently emits static scalar initializers and
recursively zero-initialized aggregates; it does not expose initialized
aggregate literals or standalone global function slots.

## Value layout

Values use the portable executor's recursive packed layout:

| LIR value | Size and representation |
| --- | --- |
| `bool` | one byte, `0` or `1` |
| fixed integer | `bits / 8` little-endian bytes |
| `f32`, `f64` | four or eight raw IEEE-754 bytes |
| pointer | eight-byte one-based checked address slot; zero is null |
| function | eight-byte one-based `LirProgram.functions` slot; zero is null |
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

## Static globals and checked pointers

Globals are concatenated in declaration order with the same packed value
layout and no padding. A verified bool, fixed integer, `f32`, or `f64`
initializer contributes its normalized little-endian bytes. A global without a
constant is initialized to all zero bytes, including nested records, fixed
arrays, pointers, and function values. The encoder does not execute a startup
initializer function. The verifier continues to reject `GlobalAddress` for a
non-writable global, so native code cannot obtain a pointer that bypasses the
portable readonly rule.

Pointer values are not virtual addresses. The encoder plans a canonical table
of each global root and each reachable record field or fixed-array element,
identified by global, packed offset, and exact LIR type. `GlobalAddress`
returns the root's one-based slot. `FieldAddress` accepts only a matching
record slot and returns the canonical child slot. `ElementAddress` accepts only
a matching array slot and an in-bounds integer index, then returns that
element's canonical slot. A negative signed index, an out-of-bounds index, or a
null, forged, or wrong-type array slot branches to `_zir_trap` and exits with
status 125. Elements with the same zero-sized type and address intentionally
share a slot.

Equal pointers therefore compare equal after calls, results, aggregate copies,
global loads and stores, and block parallel copies, including aliases to
zero-sized fields and elements.

`Load` and `Store` compare the supplied slot with the preplanned entries of the
exact pointee type before accessing data. Null, forged, table-out-of-range, and
wrong-type values branch to `_zir_trap` and exit with status 125. A successful
operation uses only the fixed data address and extent attached to that entry,
so a pointer value can neither select bytes outside the global image nor write
generated code. Arbitrary pointer arithmetic remains unsupported.

## Checked frame allocation

Programs with `Allocate` map one private, anonymous 256 MiB RW region at
startup with Linux `mmap`. Mapping failure branches to `_zir_trap` and exits
with status 125. The initializer has its own text-relative native range;
`_start` and `_zir_trap` retain their exact 21-byte and 14-byte extents. `rbx`
keeps the region base throughout this internal ABI. The region is never
executable and is reclaimed when the process exits.

Every executed `Allocate` appends a fresh eight-byte-aligned record to that
region. Its 32-byte descriptor contains the previous record for the same static
site, the allocating frame's `rbp`, the complete pointer token, and that frame's
generation marker, at offsets 0, 8, 16, and 24. Pointee-sized storage follows at
offset 32; the next record starts after rounding the complete size up to eight
bytes. The optional initializer has the exact pointee type and is copied into
the new storage; otherwise the scalar storage is set to zero. Accepted pointees
are bool, fixed-width integer, `f32`, `f64`, and pointer values. Aggregate and
function pointees remain rejected by the verifier.

The checked 64-bit pointer is `generation << 24 | slot`. Its low 24-bit slot
identifies one static allocation site numbered after the canonical global
address slots. Site planning and global slots share 16,777,215 nonzero values.
The RW ELF data extent appends one eight-byte newest-record pointer per site,
then a shared eight-byte generation counter and eight-byte region bump offset.
Allocation programs retain the six-section ELF layout even without globals.
The counter, heads and bump offset start at zero. Frame entry and every executed
allocation increment the counter, trapping before an increment would exceed
`2^40 - 1`. Each frame stores its own generation marker at `[rbp-8]`.

`Load` and `Store` first select sites of the exact pointee type, then scan that
site's persistent records newest first. Each record address is checked against
the mapped region bounds before reading its metadata. A complete token match
must name an owner on the active saved-`rbp` chain, which startup terminates with
zero, and its saved marker must match the owner's current marker. Only after
these checks can the operation access the record's scalar payload. A null,
forged, wrong-type or stale token traps with status 125; returned-frame memory
is never dereferenced while searching for an owner.

Returning or reusing a frame invalidates its earlier pointers, while aliases
remain valid in the allocating frame and active callees. Re-executing a static
site, including in loops and recursion, creates distinct storage and tokens.
Records remain in the region after their owners return so later traversal never
follows metadata on reclaimed stacks. Space is not reused within an execution;
a request that would exceed the region traps with status 125 before its record
or payload is written. This explicit capacity is a native implementation limit,
not a portable engine storage guarantee. Allocation pointers are not stable
across executions and must not be compared across processes, engines, or runs.

The checked dispatch is deliberately simple: memory operations scan matching
static address slots and then the persistent allocation descriptors when that pointee
type has allocation sites, just as typed indirect calls scan matching functions.
It is not claimed as an optimized dispatch. Planning caps canonical address
slots at 1,048,576 and both address canonicalization work and each program's
static memory-, element-address-, or indirect-dispatch comparison product at
16,000,000. Exceeding a cap returns `LirProblem.Limit`.

`FunctionAddress` stores the target function index plus one. The same target
therefore compares equal across repeated addresses of the same verified
function type. `Null(Function)` stores zero and compares equal only to another
null of that type. Function values remain slots rather than process addresses;
copying them through parameters, results, block edges, records or arrays
preserves the slot exactly.

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
| bool, integer, pointer or function | normalized `rax` |
| `f32` or `f64` | `xmm0`, with the declared width |
| record or fixed array | caller-owned packed storage addressed by `rsi` |

An aggregate-returning callee saves `rsi` in its frame before executing any
nested call. On return it copies the result SSA value to that address. This
supports recursive calls, nested aggregates, mixed scalar/aggregate signatures
and signatures whose packed values exceed the platform argument-register
limits. It does not classify values into System V INTEGER, SSE or MEMORY
classes, so no external interoperability is implied.

For `CallIndirect`, the first operand is the function slot and the remaining
operands form the packed argument block. The encoder emits a signature-specific
checked dispatcher at the call site. It compares the slot only with defined
portable functions whose parameter and result type IDs exactly match the
verified function type, then uses an ordinary relative call to the matched
function. Zero, an out-of-range slot and a slot naming a function with another
signature all branch to `_zir_trap` and exit with status 125. Generated code
never treats an unchecked value as an indirect machine-code address.

Generated calls obey the x86-64 stack rule: `rsp` is 16-byte aligned before
`call`. Every SSA value has distinct frame storage sized by its packed type.
Each frame has one reusable scratch interval large enough for its widest block
edge or direct- or indirect-call argument block. Aggregate copies use
`rep movsb`; generated code assumes the ABI-required clear direction flag and
does not change it.

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

An ELF without globals retains the original single RX load segment, 120-byte
text file offset, virtual text address, section layout, and instruction bytes.
An ELF with globals has a 176-byte header with two `PT_LOAD` entries: the first
maps the headers and text RX, and the second maps packed global bytes RW on a
distinct page. No load segment is RWX. `.data` is non-executable, and each
global has a qualified `module:name` `STT_OBJECT` symbol with its exact packed
address and extent. Function symbols and native ranges continue to describe
only real text bytes.

`LirX64ElfLayout(program) -> LirX64ImageLayout` verifies and measures without
writing output. It returns `problem`, `file_size`, `text_offset`,
`text_address`, `text_size`, `data_offset`, `data_address`, and `data_size`.
The data fields are zero for a program with neither globals nor allocations. `LirX64Ranges` remains
text-relative; consumers add `text_offset` for file positions or
`text_address` for virtual addresses. Raw `LirX64Encode` bytes use the same
planned data virtual address as the paired ELF image.

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
range coverage, ELF symbol extents and short-buffer atomicity, plus
function-valued arguments and results, nested callbacks, recursive indirect
calls, function-valued block copy cycles, and null and wrong-signature target
traps. Static-global fixtures add narrow signed and unsigned integers, bool and
`f64` initializers, recursively zeroed aggregates, mutation across calls,
nested field addresses, pointer call/result and block-copy cycles, RX/RW
segment permissions, object symbols and extents, and null, forged,
table-out-of-range, and wrong-type pointer traps. Allocation fixtures add
per-site heads, generation counter and bump offset in six-section ELF data, initialized and zero-default scalar values,
same-site loops, recursion and active aliases, structured stale/reused-frame and
generation-overflow and region-capacity traps, including their last valid boundaries. Dynamic-element fixtures add
nested arrays, array-pointer parameters and results, zero-sized element
aliases, signed and unsigned bounds traps, patched wrong-parent tokens, and
bounded slot planning. Separately compiled C oracles check the aggregate,
typed-callback, global, and element-address results without asserting C layout
or ABI compatibility.
