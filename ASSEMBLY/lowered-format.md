# Lowered instruction binary format

Status: foundational codec for steps 02 and 05. This is not the ZX container
revision from step 05, does not wrap checked `.zir`, and is not an installed
execution format by itself.

The implementation is
[`cmd/compiler_exec_serial.zi`](../cmd/compiler_exec_serial.zi). It serializes
the complete current `LirProgram` record with explicit fields. It never copies
a C struct, stores a host pointer, depends on host alignment, or uses native
integer byte order.

## Public API and ownership

The exported Ziran API is:

```zi
LirWrite(program: LirProgram, output: []u8) -> LirCodecResult
LirRead(owner: *LirReadOwner, input: string, limits: LirLimits) -> LirCodecResult
LirReadProgram(owner: *LirReadOwner) -> LirProgram
LirReadFree(owner: *LirReadOwner)
```

Normal generated-module naming applies. C consumers see the
`compiler_exec_serial_` prefix; Go and portable callers use the generated module
API rather than a separate ABI shim.

`LirWrite(program, [])` validates the program and returns the exact required
`size`. A nonempty output shorter than that size returns `Output`, reports the
same size, and changes no byte. A sufficient output receives exactly `size`
bytes; any suffix beyond that size is unchanged. The writer traverses every
table in stored ID order, so the same program produces the same bytes.

`LirReadOwner` must start zero-initialized (ordinary Ziran locals do). `LirRead`
first releases any preceding value, makes three bounded passes (shape, copied
data, then borrowed views), and calls `LirVerify` before reporting success. It
copies all strings and constant byte arrays. The input may therefore be changed
or released as soon as the call returns. On any failure the owner is empty.

`LirReadProgram` returns a borrowed program. Every table, nested slice and
string in it expires at the next `LirRead` or `LirReadFree` on that owner.
Consume those views first. Empty and repeated frees are harmless. The owner
must not be moved or resized while a program view is live, and input to a new
read must not alias the owner's preceding storage.

These rules use ordinary Ziran slices and owned vectors, so they apply to C,
Go, and portable VM output without a host-only allocator or garbage-collected
lifetime exception.

## Integer and sequence encodings

Every integer is little-endian and has the width named below:

- `u8`, `u32`, `u64`: unsigned 8-, 32-, and 64-bit values.
- `s32`, `s64`: the two's-complement bit pattern written as `u32` or `u64`.
- `string` and byte slice: `u64 byte_count`, followed by exactly those bytes.
- ID/type/value list: `u32 count`, followed by `count` `s32` elements.
- record/table count: `u32 count`.

Strings and byte slices are binary. Embedded NUL is never a terminator. The
current `LirVerify` policy still rejects NUL in module, source, global and
function names; such a file reaches verification as binary data and returns
`Verify`, rather than being truncated by the codec.

Counts are limited to `INT32_MAX`, because lowered IDs are signed 32-bit
values. The whole encoded file and the default decoded-storage budget are at
most 256 MiB. Caller-supplied `LirLimits` independently bound types, functions,
blocks, values, instructions, decoded storage and graph work. All additions,
multiplications, offsets and lengths are checked before allocation or slicing.

## Header and section directory

The fixed header is 64 bytes:

| Offset | Field | Required value |
|---:|---|---|
| 0 | `u32 magic` | `0x52494c5a` (`ZLIR`) |
| 4 | `u32 format_version` | `1` |
| 8 | `u32 lir_schema_version` | current `LirVersion` |
| 12 | `u32 header_size` | `64` |
| 16 | `u64 required_features` | bit 0 (`base`) only |
| 24 | `u64 flags` | `0` |
| 32 | `u32 section_count` | `1` or `2` |
| 36 | `u32 reserved` | `0` |
| 40 | `u64 total_size` | exact input byte count |
| 48 | `u64 directory_offset` | `64` |
| 56 | `u64 reserved` | `0` |

Each directory entry is 24 bytes:

```text
u32 kind
u32 flags
u64 offset
u64 size
```

Entry 0 is always kind 1 (`core`) with flag bit 0 (`required`). Entry 1, when
present, is kind 2 (`debug`) with flags zero. Directory and section order are
canonical. The core begins immediately after the directory, debug immediately
after core, and the final section ends at `total_size`. Gaps, overlaps, unknown
sections, unknown flags, nonzero reserved fields, truncation and trailing bytes
are rejected. Unknown required-feature bits are rejected before any table
allocation.

Debug is present exactly when the program contains at least one source or
origin. It is omitted when both tables are empty. A consumer that strips debug
must also clear every function/instruction origin to `LirNoId` before producing
a new verified file.

## Core section

Fields occur exactly in this order:

```text
s32 program.version
s32 program.entry

u32 module_count
repeat module_count:
    string module

u32 type_count
repeat type_count:
    u32 kind
    s32 bits
    u32 flags             # bit 0 signed, bit 1 native_width
    s32 element
    s64 count
    s32 result
    list<s32> fields
    list<s32> parameters

u32 constant_count
repeat constant_count:
    s32 type
    u64 bits
    bytes data

u32 global_count
repeat global_count:
    s32 module
    string name
    s32 type
    s32 constant
    u32 flags             # bit 0 writable

u32 function_count
repeat function_count:
    s32 module
    string name
    list<s32> parameters
    s32 result
    u32 calling
    u32 flags             # bit 0 external, bit 1 variadic
    list<s32> values
    u32 block_count
    repeat block_count:
        list<s32> parameters
        u32 instruction_count
        repeat instruction_count:
            u32 op
            s32 result
            list<s32> operands
            u32 edge_count
            repeat edge_count:
                s32 block
                list<s32> arguments
            u64 immediate
            s32 symbol
            u32 alignment
            s32 effect
            s32 origin
    s32 entry
    s32 origin
```

This covers every current schema field, including currently verifier-gated
types, constants, calling forms and opcodes. The decoder preserves their bytes
and field values, then lets the authoritative verifier accept or reject the
result. Enum values beyond the current declared range and flag bits outside
the masks above are noncanonical format errors before verification.

## Debug section

```text
u32 source_count
repeat source_count:
    string source

u32 origin_count
repeat origin_count:
    s32 file
    s32 line
    s32 column
    s64 begin
    s64 end
    s32 parent
```

Source strings are copied like all other strings. Origin parent chains and
file references are checked by `LirVerify`; hostile depth is handled by its
bounded iterative validation.

## Validation and error boundary

Framing errors are reported as `LirCodecProblem`: magic/version/feature,
flags/field/section, overlap, truncation/trailing, count/limit/allocation,
short output, or invalid owner. A structurally decoded program that fails the
lowered verifier returns `Verify` and the complete `LirDiagnostic`.

The reader does not trust producer flags, a previous successful check, or
optional debug data. It constructs no executable program until the full file
is canonical, all caller limits hold, all owned storage is complete, and
`LirVerify` succeeds. Re-encoding a successful decode is byte-exact.

## Current boundary

- This file has no imports, exports, capability/link tables, assets, native
  variants, proofs, signatures, compression, checksum, or content identity.
  Those belong to the future incompatible ZX revision and linker in step 05.
- It does not execute instructions. The portable engine is step 04.
- It accepts exactly the current verifier's semantic subset. In particular,
  verifier-gated schema fields remain explicitly encoded but cannot become an
  executable decoded program until verifier semantics exist.
- Format version 1 has no compatibility promise with the planned final ZX
  container. Readers reject other versions with an explicit rebuild boundary.

Run the source/saved-IR C, Go and VM regression with:

```sh
env -u DISPLAY -u WAYLAND_DISPLAY build-queue -- \
    sh tests/lowered_serial.sh build/bin/ziran
```
