# Ziran intermediate representation (`.zir`)

ZIR is the checked, versioned module format. `ziran ir --root src -o out
src/main.zi` writes it after semantic checking; `ziran inspect` reads it.
C and Go exporters consume ZIR directly. The ZX linker lowers checked modules
to LIR, verifies and optimizes LIR, then emits executable ZX.

## Checked module data

Modules retain import and export identities, resolved types, constants,
procedure signatures and bodies, checked expression graphs, source spans,
foreign declarations and host capability requirements. Named and default
arguments retain source evaluation order and their checked parameter indices.
Generic declarations retain template and specialization identities.
Owned callables retain their erased ABI, concrete environment construction,
invoke and cleanup procedure values, and invocation-local borrow provenance.
Consumers recheck capture types and provenance along with ordinary moves.

Architecture-specific `#asm` metadata accompanies the checked portable body.
C and Go can select supported variants; ZX uses the portable body. See
[assembly](USAGE.md#assembly-with-a-portable-body).

Law records carry structured evidence and proof graphs. Consumers validate
proof certificates against checked bodies; cached evidence alone cannot
prove an obligation. See [laws](LAWS.md).

## Validation and lifetime

The reader validates the format, lengths, references and expression graphs.
Builds recheck saved modules. Use the same toolchain to write and consume a
module; ZIR is a compiler interchange format. Linked [ZX](ZX.md) executables
are the portable distribution format.

Process pointers, context identities and scratch storage are never serialized.
Source spans remain available for diagnostics. See [compiler embedding](COMPILER_API.md)
for context ownership and [current support](IMPLEMENTATION_STATUS.md) for target limits.
