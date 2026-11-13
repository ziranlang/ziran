# Step 05 — Make ZX a versioned portable executable container

Status: in progress; the [revision-28 executable container](zx-executable-format.md) stores verified LIR, runs stripped through the portable engine, and rejects revisions 26/27 with a rebuild diagnostic. It is opt-in through `zi2asm --emit zx`/`--input zx`; host capabilities, libraries/partial composition, native variants and the default cutover remain pending. Delivery order: [milestones](milestones.md) M1 and M3. Depends on [steps 02](02-lowered-ir-and-verifier.md)–[04](04-portable-instruction-engine.md). Next: [native code generation](06-optimization-and-native-backend.md).

## Objective

Replace embedded executable checked `.zir` with the lowered portable instruction program. Keep `.zx` responsible for executable code, linkage, capabilities, libraries, assets, variants, and inspection metadata. A stripped bundle must run without embedded source or semantic IR.

## Existing integration points

- [Container records and codec](../cmd/compiler_bundle.zi).
- [Bundle linker](../cmd/compiler_link.zi), [bundle implementation](../cmd/zir/zir_bundle.c), and [asset handling](../cmd/zir/zir_bundle_assets.c).
- [Bundle command](../cmd/zir-zx/main.c), [package bundle execution](../cmd/package_bundle_run.zi), and [format documentation](../docs/ZX.md).
- [Law checking](../cmd/compiler_law.zi) and [proof checking](../cmd/compiler_proof.zi).

Proposed modules: `cmd/compiler_exec_serial.zi`, `cmd/compiler_exec_link.zi`, and `cmd/compiler_exec_variants.zi`. Evolve the existing container owner instead of adding a competing bundle wrapper.

## Implementation work

- [ ] Define a new incompatible ZX revision with explicit section kinds, lengths, alignment, feature requirements, entry IDs, and deterministic ordering. Choose the revision from the live format history at implementation time.
- [ ] Encode functions, blocks, values, instructions, types, relocatable symbolic constants, imports, exports, global initializers, and data without host pointer values or compiler struct layouts.
- [ ] Separate required execution sections from optional source maps, origins, optimization remarks, and original source attachments. Optional executable `.zir` must never be a hidden execution dependency.
- [ ] Preserve asset streaming and binary bytes, deterministic output, write-to-pipe behavior, and existing error semantics. Validate offset/length arithmetic, overlaps, truncation, unsupported required features, and trailing data rules.
- [ ] Lower then link executable symbols and portable type identities. Handle direct/indirect call roots, callbacks, exported layouts, global initialization, function/data address identity, and dead stripping conservatively.
- [ ] Preserve external libraries, thin/add-on bundles, partial-module composition, and their resolution order. Detect duplicate/conflicting definitions and ABI/layout mismatches before instantiation.
- [ ] Recompute capability requirements from verified reachable instructions. Cross-check declared tables, library bindings, and host signatures rather than trusting producer-supplied manifests.
- [ ] Keep law evidence honest. Either revalidate attached proof material against an adequate non-executable witness or report producer evidence as unverified metadata. A checksum, previous checker flag, or matching law table is not a proof of lowering correctness.
- [ ] Verify the executable itself independently of any optional proof/source section. Certificates for removed bounds checks or other privileged assumptions must be checkable by the consumer; otherwise retain the checks.
- [ ] Define native variant records with target CPU, OS/ABI where relevant, required feature masks, common signature, and deterministic selection precedence. If no declared implementation matches the selected target/CPU contract, reject before execution; do not run portable code as a native fallback.
- [ ] Keep generic executable code mandatory for portable bundles. Store native variants as verifiable target-operation/template metadata, not automatically trusted arbitrary machine-code blobs.
- [ ] Validate variant operands, effects, register clobbers, and boundaries before code generation. Keep feature detection baseline-safe and check OS-enabled register state where required.
- [ ] Define explicit target-only artifacts separately when a producer intentionally requires an architecture. The CLI must report that reduced portability and reject unsupported hosts before executing code.
- [ ] Encode the stable Gap identifiers needed for target preflight and inspection.
  Unknown required target features reject; they do not select another executable
  section, format revision, or engine.
- [ ] Reject old incompatible bundles with an actionable rebuild diagnostic. Keep legacy-format rejection tests; do not maintain two executable formats indefinitely.
- [ ] Include every executable and variant section in cache/content identity while keeping optional debug-only changes from unnecessarily invalidating machine code where a correct split is possible.

## Tests

Extend [bundle verification](../tests/bundle_verification.sh), [compiler bundle tests](../tests/compiler_bundle.sh), [library composition](../tests/bundle_libraries.sh), [partial modules](../tests/bundle_partial_modules.sh), [global reachability](../tests/bundle_global_reachability.sh), and [portable streaming](../tests/zx_portable_write.sh).

Add malformed new-section fixtures, required/optional feature mutations, variant incompatibility tests, stripped-bundle execution, deterministic source/saved-IR bytes, and proof-table tampering. Verify functionally identical portable behavior with supported variants selected, masked out, and unavailable.

## Exit gate

New bundles execute using step 04 without semantic `.zir` bodies. Libraries, assets, state, capabilities, law reporting, and diagnostics retain their defined behavior. Old bundles fail clearly. The writer and reader agree on a documented public instruction/container contract rather than private compiler serialization.

## Handoff

Deliver the binary/text format specification, opcode version policy, golden/mutation fixtures, variant contract, and the new bundler/loader path. Keep the installed default unchanged until step 10's integration gates pass.
