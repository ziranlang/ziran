# ZX executable instruction container — revision 29

This document specifies the `.zx` executable format. The format owner is `cmd/compiler_bundle.zi`;
`cmd/compiler_exec_link.zi` binds it to the deterministic LIR codec and
verifier (current payload schema 5). `zi2zx` writes it. `ziran run`, `zi2zx run`,
the embedding API (`include/ziran_host.h`), `zx2wasm`, and `zx2native` read it.
Checked packages use the [ZX library format](zx-library-format.md).

## Integers, limits and ordering

All integers are unsigned little-endian unless a field is explicitly signed.
The complete file is bounded to 268,435,456 bytes. Section count is 3–1,024.
The instruction payload is non-empty and at most 268,435,456 bytes. Asset names
and lengths use the existing u32 codec and its canonical validation.

Directory kinds are strictly increasing and unique. Every writer section uses
alignment 8. A reader accepts power-of-two alignment through 4,096 for optional
extensions, but requires the offset to equal the first aligned byte after the
previous section and every intervening padding byte to be zero. It rejects
integer overflow, unordered/duplicate entries, gaps, bad padding, overlap,
truncation, and trailing bytes. There is no recovery scan or legacy fallback.

## Header (64 bytes)

| Offset | Width | Value |
| ---: | ---: | --- |
| 0 | 4 | `5a 58 00 00` (`ZX\0\0`) |
| 4 | 4 | format revision `29` |
| 8 | 4 | header size `64` |
| 12 | 4 | directory record size `32` |
| 16 | 8 | required feature mask, `1` for apps or `3` with module linkage |
| 24 | 8 | optional feature mask |
| 32 | 4 | section count |
| 36 | 4 | container flags; zero |
| 40 | 8 | exact total file length |
| 48 | 8 | directory offset; `64` |
| 56 | 4 | identity algorithm; `1` means SHA-256 |
| 60 | 4 | reserved; zero |

Required feature bit 0 retains the manifest contract name `portable-lir-schema-3`.
The internal payload independently carries and verifies current LIR schema 5.
Required feature bit 1 advertises module linkage. A reader requires bit 0 and
rejects unknown required bits. Optional bits are non-executable hints and may be ignored.
Any other revision is unsupported, and the reader never sends a payload to the
`.zir` parser.

## Directory record (32 bytes)

| Offset | Width | Meaning |
| ---: | ---: | --- |
| 0 | 4 | section kind |
| 4 | 4 | flags; bit 0 means required |
| 8 | 8 | absolute file offset |
| 16 | 8 | byte length |
| 24 | 4 | power-of-two alignment |
| 28 | 4 | reserved; zero |

Implemented kinds are:

| Kind | Required | Meaning |
| ---: | --- | --- |
| 1 | yes | fixed executable manifest |
| 2 | yes | deterministic stripped internal instruction payload |
| 3 | yes | canonical assets, including an empty table |
| 4 | reserved | unused. Assembly records are part of kind 2 |
| 5 | iff required feature bit 1 is set | module linkage bytes |
| `0x80000001` | no | opaque debug attachment |
| `0x80000002` | no | opaque source attachment |
| `0x80000003` | no | host interface: the names an embedder sees (below) |
| `0x80000100` and above | no | producer extension attachments |

An unknown optional section is bounded and skipped. An unknown required section
rejects before LIR decoding. That includes a required kind-4 section. Native
assembly is represented by the bounded templates in the verified program.

## Host interface (kind `0x80000003`)

The section is mandatory and nonempty when required feature bit 1 is set.
Its directory flags are zero in both app and module bundles.

The instruction payload is structural: a record is its field types, a string
is `{data, count}`. An embedder's host values carry names, so `zi2zx` writes
them here, as UTF-8 text of tab-separated lines. Type nodes come
first, each naming only earlier nodes:

    node ID scalar|string|pointer SPELLING
    node ID slice|array SPELLING CHILD
    node ID record SPELLING NAME CHILD NAME CHILD ...

then one line per host capability, by LIR function index, with its result
node (`-1` for void) and parameter nodes, and one with its declaration as
written (variadic and reads-arguments flags, the declaration line, its
parameters and its return type):

    host FUNCTION RESULT PARAMETER ...
    abi FUNCTION VARIADIC READS SIGNATURE ARGUMENTS RETURN

then one line per module global, by LIR global index, with its type node,
and the entry's parameters and return type as written:

    global GLOBAL NODE
    entry ARGUMENTS RETURN

A spelling is the type as declared at that position, which a host sees as the
value's type name. Declaration text has its tabs and line ends as spaces. The
embedding API (`include/ziran_host.h`) uses these lines to translate host
values, copy a host's changes to `[]T` arguments back, export and import
module globals as state, and compare two files' capability ABIs. App bundles
may omit this section.

## Manifest (kind 1, exactly 128 bytes)

| Offset | Width | Meaning |
| ---: | ---: | --- |
| 0 | 4 | manifest schema `1` |
| 4 | 4 | LIR schema `3` |
| 8 | 4 | signed entry function ID; nonnegative |
| 12 | 4 | reachable host capability count, at most 4,096 |
| 16 | 4 | number of target-only functions, recomputed from the verified program |
| 20 | 4 | asset count |
| 24 | 4 | verified proof status; currently `0` |
| 28 | 4 | reserved; zero |
| 32 | 32 | program identity |
| 64 | 32 | asset identity |
| 96 | 32 | combined executable-content identity |

The program identity is SHA-256 of `ZX29-PROGRAM\0` followed by the exact kind-2
bytes, including assembly. The asset identity is SHA-256 of `ZX29-ASSETS\0`, the u32 asset count,
then each sorted asset's u32 name length/name bytes and u32 data length/data
bytes. For a bundle with required-feature mask `1`, the combined identity is
SHA-256 of `ZX29-CONTENT\0`, the entry ID as a little-endian u32, the mask as
a little-endian u64, the program identity and the asset identity.

For a bundle with required-feature mask `3`, the combined identity is SHA-256
of `ZX29-MODULE-CONTENT\0`, the entry ID as a little-endian u32, the mask as
a little-endian u64, the program identity, the asset identity, the linkage byte
length as a little-endian u64, the exact kind-5 bytes, the host-interface byte
length as a little-endian u64 and the exact kind-`0x80000003` bytes.

The owned reader checks all three identities before exposing the executable.
Module linkage and its host interface affect the module bundle's content
identity. Debug, source and unknown optional attachments are outside the
combined identity. Proof status zero means no
opaque producer law/proof claim becomes verified merely by being attached or
hashed.

## Required instruction program (kind 2)

The payload uses the internal deterministic instruction codec from
`compiler_exec_serial.zi`: header, required core section, and no debug section.
It is not a standalone executable artifact: `.lirb`, `--emit exec` and
`--input exec` have been removed. ZX is the portable program boundary.
It contains LIR version, module identities, type graph, constants and binary
constant bytes, globals and initializers, functions, parameter/value type IDs,
blocks, instructions, operands, edges, symbol IDs, effect ordering, alignments
and the entry ID. It has no host pointers or compiler/C record layouts.

The executable writer strips source names and origins, then calls the
LIR verifier and codec. The reader copies all LIR strings, blobs and tables into
`ZxExecOwner`, rejects a debug-bearing required payload, verifies it, checks that
the manifest and program entry IDs agree, and exposes a borrowed `LirProgram`
until `ZxExecReadFree`. The original container bytes can be released immediately.

Reachability starts at the entry. Direct calls, host calls and function-address
constants add their symbol IDs; an indirect call conservatively adds every
portable function, since host procedure values do not exist.
Duplicate module identities, duplicate module/name function definitions and
duplicate module/name globals reject. Each reachable bodiless `Host` function is
one capability; its module, name and scalar signature are already in the
verified program. The writer stores the recomputed count and the reader rejects
a different manifest count (`capability-count`) instead of trusting producer
metadata. Other external functions and non-portable calling conventions reject.
The reader also checks the target-only procedure count against the verified
program. The portable engine and Wasm reject a nonzero count before loading
initializers. The native compiler checks architecture, OS, ABI and usable CPU
features. Its first eligible template wins in declaration order. With no
eligible optional template it compiles the portable body for the requested
native backend. Native code is cached by the required program identity,
compiler/backend revision, architecture, OS, ABI and effective CPU features.
The loader uses source maps and origins from optional attachments for diagnostics.

Outer validation uses the caller's `LirLimits` as well as the inner codec and
verifier. The reader rejects a container larger than `limits.bytes` before
framing or hashing and charges `limits.work` for container bytes, program and
asset hashing, module linkage and module host-interface hashing and copying,
symbol-name bytes, hash-table initialization/probes, reachability and temporary
copies. Temporary asset, symbol and reachability allocations are
checked against the byte budget before allocation. Symbol uniqueness uses
deterministic open-addressed tables; hash collisions remain explicitly charged.
The first reachable indirect call adds every function once, and later indirect
calls do not rescan that set. A budget failure returns `limit`, carries an LIR
`Limit` diagnostic and leaves the owned reader invalid. The writer applies the
same accounting with `LirDefaultLimits`; these limits do not change which
instructions or sections the format accepts.

## Assets (kind 3)

The section is the existing asset codec: u32 count, then for each asset a u32
name length, name bytes, u32 data length and exact data bytes. Names are sorted
bytewise and unique relative paths. Empty, `.`, `..`, absolute-like, backslash,
colon and NUL-containing names reject. Data is arbitrary binary, including NUL.
The section must be consumed exactly.

The raw framing API returns asset views borrowing the file. The owned executable
API validates and hashes them during the read; the embedding API copies them
when it opens the file and exposes them through `BundleAsset*`. `zi2zx
--asset-dir` stores them.

## Module linkage (kind 5)

The section is present exactly when required feature bit 1 is set. Its flags
are `1`, alignment is `8`, and its payload consists of the compiler's canonical
module linkage bytes. The payload must be nonempty and fits within the container
and reader byte limits. The framing API treats the payload as bytes. The owned
reader verifies the combined identity and copies the payload before returning.
The linkage codec defines the interpretation of these bytes.

## APIs and failure behavior

`ExecutableEncode` measures when output is empty. A short output returns its
required size and changes no byte. `ExecutableDecode` frames sections and assets
without interpreting LIR. `ZxExecWrite` produces a stripped deterministic
container; `ZxExecRead`, `ZxExecReadProgram`, and `ZxExecReadFree` provide the
bounded owned verified program. Empty and repeated frees are safe.
`ZxExecWriteModule(program, assets, output, interface, debug, module_linkage)`
writes the module form when linkage is nonempty. Empty linkage produces the app
form. `ZxExecReadModuleLinkage(owner)` returns bytes owned by a successful read
and valid until that owner is reused or freed. It returns an empty string for
an app bundle or an invalid owner.

The format-specific tools keep generated-output replacement semantics: they write a sibling
temporary and replaces the requested path only after a complete successful
write. `-o -` streams revision 29 to a pipe. A malformed input cannot replace a
previous output. Diagnostics distinguish legacy/unknown versions, required
features or sections, flags, ordering, alignment, overlap, truncation, trailing
bytes, manifest fields, assets, content identity, LIR codec/verifier rejection,
entry mismatch, capability, variant, external and symbol failures.

## Remaining gaps

- Generic cross-container symbol tables and thin/add-on composition remain
  open. Landed since this text was written: module-linked bundles bind required
  checked modules through the kind-5 linkage section with one combined
  verified identity, and [`.zx` libraries](zx-library-format.md) exist as a
  separate package format; neither creates a general cross-container symbol
  table.
- The optional debug/source attachment codec exists (section `0x80000001`,
  identity-bound and reader-reverified). Optimization remarks, law witnesses
  and proof revalidation are not implemented; optional bytes remain
  non-executable and proof status remains unverified. Laws are checked from
  source.

The retained focused tests cover deterministic source/saved output, stripped
portable execution, owned input lifetime, legacy and feature rejection, unknown
required/optional sections, identity corruption, overlap, truncation, trailing
bytes, binary/NUL assets, pipe output and failed-output preservation.
