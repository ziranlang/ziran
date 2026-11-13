# ZX executable instruction container — revision 28

This is the implemented, opt-in ASSEMBLY step-05 foundation. The format owner
is `cmd/compiler_bundle.zi`; `cmd/compiler_exec_link.zi` binds it to the
deterministic LIR codec and verifier (current payload schema 4). `zi2asm --emit zx` produces it,
and `zi2asm --input zx` reads, inspects, re-emits or runs it. Default `ziran`
and `zi2zx` selection remains on versions 26/27 until the full cutover.

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
| 4 | 4 | format revision `28` |
| 8 | 4 | header size `64` |
| 12 | 4 | directory record size `32` |
| 16 | 8 | required feature mask; exactly bit 0 today |
| 24 | 8 | optional feature mask |
| 32 | 4 | section count |
| 36 | 4 | container flags; zero |
| 40 | 8 | exact total file length |
| 48 | 8 | directory offset; `64` |
| 56 | 4 | identity algorithm; `1` means SHA-256 |
| 60 | 4 | reserved; zero |

Required feature bit 0 retains the manifest contract name `portable-lir-schema-3`;
the internal payload independently carries and verifies current LIR schema 4.
A reader rejects any missing
or additional required feature. Optional bits are non-executable hints and may
be ignored. Versions 26 and 27 produce the explicit diagnostic “legacy ZX
version contains semantic IR; rebuild with zi2asm --emit zx”. Other revisions
are unsupported. The reader never sends their payload to the `.zir` parser.

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
| 4 | not accepted as required | reserved native variants |
| `0x80000001` | no | opaque debug attachment |
| `0x80000002` | no | opaque source attachment |
| `0x80000100` and above | no | producer extension attachments |

An unknown optional section is bounded and skipped. An unknown required section
rejects before LIR decoding. A required kind-4 section rejects as an unsupported
native variant; arbitrary machine bytes are never selected or executed.

## Manifest (kind 1, exactly 128 bytes)

| Offset | Width | Meaning |
| ---: | ---: | --- |
| 0 | 4 | manifest schema `1` |
| 4 | 4 | LIR schema `3` |
| 8 | 4 | signed entry function ID; nonnegative |
| 12 | 4 | reachable host capability count, at most 4,096 |
| 16 | 4 | required native-variant count; currently `0` |
| 20 | 4 | asset count |
| 24 | 4 | verified proof status; currently `0` |
| 28 | 4 | reserved; zero |
| 32 | 32 | program identity |
| 64 | 32 | asset identity |
| 96 | 32 | combined executable-content identity |

The program identity is SHA-256 of `ZX28-PROGRAM\0` followed by the exact kind-2
bytes. The asset identity is SHA-256 of `ZX28-ASSETS\0`, the u32 asset count,
then each sorted asset's u32 name length/name bytes and u32 data length/data
bytes. The combined identity is SHA-256 of `ZX28-CONTENT\0`, the little-endian
entry ID, required-feature u64, program identity and asset identity.

All three identities are checked before the executable is exposed. Required
program, entry and assets affect content identity. Optional debug, source and
unknown optional attachments intentionally do not. Proof status zero means no
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

The executable writer strips source names and origins, then calls the unchanged
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
The existing bounded portable engine then executes the verified program once
the embedder has bound every capability.

Outer validation uses the caller's `LirLimits` as well as the inner codec and
verifier. The reader rejects a container larger than `limits.bytes` before
framing or hashing and charges `limits.work` for container bytes, program and
asset hashing, symbol-name bytes, hash-table initialization/probes, reachability
and temporary copies. Temporary asset, symbol and reachability allocations are
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
API validates and hashes them during the read; its current execution surface
reports the asset count but does not yet expose a persistent asset table.

## APIs and failure behavior

`ExecutableEncode` measures when output is empty. A short output returns its
required size and changes no byte. `ExecutableDecode` frames sections and assets
without interpreting LIR. `ZxExecWrite` produces a stripped deterministic
container; `ZxExecRead`, `ZxExecReadProgram`, and `ZxExecReadFree` provide the
bounded owned verified program. Empty and repeated frees are safe.

`zi2asm` keeps generated-output replacement semantics: it writes a sibling
temporary and replaces the requested path only after a complete successful
write. `-o -` streams revision 28 to a pipe. A malformed input cannot replace a
previous output. Diagnostics distinguish legacy/unknown versions, required
features or sections, flags, ordering, alignment, overlap, truncation, trailing
bytes, manifest fields, assets, content identity, LIR codec/verifier rejection,
entry mismatch, capability, variant, external and symbol failures.

## Deliberate remaining step-05 gaps

This foundation is complete for one already-linked LIR program in the currently
verified portable subset. It does not claim the full step-05 exit gate yet:

- separate revision-28 library/add-on containers do not yet have import/export
  symbol tables, structural cross-container type identities, relocation tables,
  callback roots, dead stripping or partial-module composition;
- the existing versions 26/27 library and partial composition APIs remain usable
  and unchanged, but cannot be mixed into revision 28;
- host-call ABI/capability tables are not implemented, so any reachable
  `host.call` and any nonzero manifest capability count reject before execution;
- native target/CPU/OS/ABI variant metadata and template verification are not
  implemented; a required variant rejects, and no arbitrary machine blob is
  accepted;
- debug/source attachment codecs, optimization remarks, law witnesses and proof
  revalidation are not implemented; optional bytes remain non-executable and
  proof status remains unverified;
- assets are validated and identified but are not yet plumbed into the portable
  instruction engine's host API;
- default `ziran`/`zi2zx`, package, provider, cache and installed-runtime cutover
  waits for the later full-plan integration gates.

The retained focused tests cover deterministic source/saved output, stripped
portable execution, owned input lifetime, legacy and feature rejection, unknown
required/optional sections, identity corruption, overlap, truncation, trailing
bytes, binary/NUL assets, pipe output and failed-output preservation.
