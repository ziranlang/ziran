# Step 08 — Encode instructions, write objects, and link executables directly

Status: in progress; [direct scalar ELF only](README.md), not relocatable objects or a linker. Depends on machine/target contracts from [steps 06](06-optimization-and-native-backend.md) and [07](07-architecture-and-platform-coverage.md).

## Objective

Complete the path from allocated machine instructions to native binaries without generated C, an external assembler, or an external linker on required targets. Keep optional external-tool comparison modes for validation, not as an undeclared dependency of the promised direct build.

## Existing integration points

- [Current C executable driver](../cmd/zir-c/main.c) and [build orchestration](../cmd/zir-c/zir_c_build.c) provide entry, diagnostics, cache, and failure-preservation behavior to retain.
- [Native entry tests](../tests/native_entry.sh) and [link-flag tests](../tests/native_link_flags_cases.zi) provide existing executable/FFI expectations.
- [Build rules](../Makefile) and [native runtime build selection](../native-zx.mk).

Proposed modules: `cmd/compiler_encode_x86.zi`, `cmd/compiler_encode_arm.zi`, `cmd/compiler_encode_aarch64.zi`, `cmd/compiler_object.zi`, `cmd/compiler_elf.zi`, `cmd/compiler_coff.zi`, `cmd/compiler_macho.zi`, `cmd/compiler_link_native.zi`, and `cmd/compiler_disassemble.zi`.

## Implementation work

- [ ] Implement instruction encoding for every emitted opcode and addressing form, including immediate widths, extension prefixes, branch displacement, vector encodings, and target feature validation.
- [ ] Define symbolic fixups independently of object format. Resolve local labels, literal pools, branch relaxation, long-branch veneers, alignment, and padding to a stable final layout.
- [ ] Print assembly directly from the same allocated/finalized instruction records used by the encoder. Annotate function, source origin, bytes, CPU requirements, stack offsets, and generated instructions.
- [ ] Provide a reader for the emitted textual listing that reconstructs its supported instruction/directive subset. This enables round-trip inspection without requiring text parsing during ordinary compilation. Do not silently imply support for every external assembler dialect.
- [ ] Keep existing user `#asm` syntax support a separate compatibility task: parse and bind the supported templates and constraints, including labels/directives where allowed; preserve their fallback and diagnostic contract.
- [ ] Write relocatable ELF, COFF, and Mach-O objects for required rows. Include text/data/readonly/BSS/TLS sections, symbols, visibility, alignment, relocations, unwind records, and debug references.
- [ ] Implement archive reading and symbol extraction, COMDAT/coalescing rules where required, weak symbols, unresolved references, duplicate definitions, section garbage collection, and retained exports/callback roots.
- [ ] Link supported executable/shared-object forms: ELF program headers and dynamic records, PE imports/exports and relocation tables, and Mach-O load commands/fixups as required by the maintained targets.
- [ ] Implement startup/entry handling and runtime helper linkage for console programs and libraries. Preserve command arguments, exit status, global initialization, thread/runtime setup, and foreign callbacks.
- [ ] Resolve ordinary OS and third-party libraries through explicit target search paths and format-aware symbol handling. Existing compiled foreign libraries are valid inputs; compiling their C sources is outside Ziran code generation.
- [ ] Inventory C-compiler-driver conveniences previously supplied by `CC`: CRT selection, sysroot, libc, dynamic loader, TLS, compiler support routines, and default libraries. Replace each with explicit native driver configuration and diagnostics.
- [ ] Define the supported native link-option surface. Preserve C-target pass-through options on the C backend; reject options that have no new-native equivalent instead of ignoring them.
- [ ] Implement platform-required executable authentication/signing integration where needed for a runnable artifact. Use installed OS signing facilities and explicit credentials; do not claim that a written Mach-O/PE file alone proves launchability.
- [ ] Write outputs atomically, preserving the last valid executable on failure. Cache object contributions using target, ABI, backend, flags, dependencies, and runtime identities.
- [ ] Produce relocation/link maps, section sizes, resolved symbol addresses, inserted stubs, padding, and a source-linked disassembly from actual final bytes.
- [ ] Expose the same encoder and relocation primitives to the in-memory loader in step 09; do not create an unrelated JIT instruction encoder.

## Tests

Add Ziran encoding/object/linker test programs. Compare individual instruction bytes against an independent assembler/disassembler; test boundary immediates, all register forms, branch expansion, relocation overflow, and malformed input objects.

Round-trip the supported listing form through parsing/encoding and compare resulting sections and fixups. Link new-native objects with independently compiled C callers/callees and, where useful, the system linker as a reference. Then run the fully internal link path with external C compiler, assembler, and linker commands unavailable.

Exercise executable launch, dynamic imports, TLS, callbacks, shared libraries, debug/unwind loading, missing symbols, archive ordering, failed writes, and preservation of installed outputs on each required OS. Run all builds through `build-queue --`; use private display fixtures where needed.

## Exit gate

Required standalone builds produce runnable binaries using the internal code generator, encoder, object writer, and linker. Listings match the encoded instruction stream; final disassembly accounts for linker-added code. No external-tool fallback is counted as completion of the direct path.

## Handoff

Deliver encoded-image APIs, relocations, link maps, independent byte/ABI fixtures, platform signing requirements, and final byte-to-source mappings to steps 09–10.
