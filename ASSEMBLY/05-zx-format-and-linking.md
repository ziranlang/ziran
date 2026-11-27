# Step 05 — Make ZX a versioned portable executable container

Status: in progress; the [revision-29 executable container](zx-executable-format.md) is the one `.zx` format. It stores verified LIR, runs stripped through the portable engine, and names its host capabilities, module globals and declarations in the host interface section; `ziran bundle`, `ziran run` and the embedding API use it. Library containers ([`ziran pkg zx pack-ir`](zx-library-format.md)) and module-linked bundles (`--require-module`/`--import`) exist; cross-container partial composition remains open. Delivery order: [milestones](milestones.md) M1 and M3. Depends on [steps 02](02-lowered-ir-and-verifier.md)–[04](04-portable-instruction-engine.md). Next: [native code generation](06-optimization-and-native-backend.md).

## Objective

Replace embedded executable `.zir` with the lowered portable instruction program. Keep `.zx` responsible for executable code, linkage, capabilities, libraries, assets, variants, and inspection metadata. A stripped bundle must run without embedded source or semantic IR.

## Existing integration points

- [Container records and codec](../cmd/compiler_bundle.zi).
- [Bundle linker](../cmd/compiler_link.zi), [bundle implementation](../cmd/compiler_bundle.zi), and [asset handling](../tests/bundle_assets.sh).
- [Bundle command](../cmd/zi2zx.zi), [package bundle execution](../cmd/package_bundle_run.zi), and [format documentation](../docs/ZX.md).
- [Law checking](../cmd/compiler_law.zi) and [proof checking](../cmd/compiler_proof.zi).

The executable codec and linker live in `cmd/compiler_exec_serial.zi` and `cmd/compiler_exec_link.zi`. Variant records use the same function identities and executable section as the portable instructions.

## Implementation work

- [x] Define a new incompatible ZX revision with explicit section kinds, lengths, alignment, feature requirements, entry IDs, and deterministic ordering. Choose the revision from the live format history at implementation time. (The current [ZX revision 29](zx-executable-format.md) includes native variant metadata. Legacy rejection is retained in [zx_executable.sh](../tests/zx_executable.sh) and [executable_bundle_cases.zi](../tests/executable_bundle_cases.zi).)
- [ ] Encode functions, blocks, values, instructions, types, relocatable symbolic constants, imports, exports, global initializers, and data without host pointer values or compiler struct layouts.
  Revision 28 already serializes ordered initializer IDs and readonly byte data;
  imports/exports, relocatable symbols and the public linked representation keep
  this container-wide item open.
- [x] Separate required execution sections from optional source maps, origins, optimization remarks, and original source attachments. Optional executable `.zir` must never be a hidden execution dependency. (The optional debug attachment (section `0x80000001`) is identity-bound, refuses foreign or damaged bytes and restores stripped origins ([`compiler_exec_debug.zi`](../cmd/compiler_exec_debug.zi), [executable_bundle_cases.zi](../tests/executable_bundle_cases.zi)); [zx_executable.sh](../tests/zx_executable.sh) executes stripped bundles with no source paths. Optimization remarks have no producer yet, so nothing hidden remains executable.)
- [x] Preserve asset streaming and binary bytes, deterministic output, write-to-pipe behavior, and existing error semantics. Validate offset/length arithmetic, overlaps, truncation, unsupported required features, and trailing data rules. (Asset codec in [`compiler_zx_assets.zi`](../cmd/compiler_zx_assets.zi) with [bundle_assets.sh](../tests/bundle_assets.sh); deterministic source/saved bytes, pipe output, and rejection/preservation semantics retained in [zx_executable.sh](../tests/zx_executable.sh) and the overlap/truncation/trailing-bytes fixtures in [executable_bundle_cases.zi](../tests/executable_bundle_cases.zi).)
- [ ] Lower then link executable symbols and portable type identities. Handle direct/indirect call roots, callbacks, exported layouts, global initialization, function/data address identity, and dead stripping conservatively. Landed slice: reachability from direct/indirect roots, global initializers, address identity and conservative dead stripping link in [`compiler_zx_link.zi`](../cmd/compiler_zx_link.zi)/[`compiler_zx_link_reach.zi`](../cmd/compiler_zx_link_reach.zi)/[`compiler_zx_link_prune.zi`](../cmd/compiler_zx_link_prune.zi), retained by [bundle_global_reachability.sh](../tests/bundle_global_reachability.sh). Callback roots and exported ABI layouts keep the rest open.
- [ ] Preserve external libraries, thin/add-on bundles, partial-module composition, and their resolution order. Detect duplicate/conflicting definitions and ABI/layout mismatches before instantiation. Landed slice: module-linked bundles (`--require-module`, `--import`) bind checked module payloads with a combined verified identity ([zi2zx.zi](../cmd/zi2zx.zi), [zx_module_build.sh](../tests/zx_module_build.sh), [zx_module_compiler_cases.zi](../tests/zx_module_compiler_cases.zi), [zx_module_runtime_cases.zi](../tests/zx_module_runtime_cases.zi)). Generic thin/add-on bundle composition beyond required modules stays open.
- [x] Recompute capability requirements from verified reachable instructions. Cross-check declared tables, library bindings, and host signatures rather than trusting producer-supplied manifests. (The writer stores the recomputed capability count and the reader rejects a mismatched manifest (`capability-count`) ([zx-executable-format.md](zx-executable-format.md), [executable_bundle_cases.zi](../tests/executable_bundle_cases.zi)); host result shapes are checked before use in the same fixture.)
- [ ] Keep law evidence honest. Either revalidate attached proof material against an adequate non-executable witness or report producer evidence as unverified metadata. A checksum, previous checker flag, or matching law table is not a proof of lowering correctness.
- [x] Verify the executable itself independently of any optional proof/source section. Certificates for removed bounds checks or other privileged assumptions must be checkable by the consumer; otherwise retain the checks. (The owned reader verifies the stripped program regardless of optional bytes ([`compiler_zx_exec_bundle.zi`](../cmd/compiler_zx_exec_bundle.zi), [zx_executable.sh](../tests/zx_executable.sh) stripped execution); no privileged certificate codec exists, so removed checks are never authorized by uncheckable data.)
- [x] Encode the stable Gap identifiers needed for target preflight and inspection.
  Unknown required target features reject; they do not select another executable
  section, format revision, or engine.
  (`assembly.gap.*` codes in [`compiler_target.zi`](../cmd/compiler_target.zi),
  reported by `ziran capabilities`/`ziran explain` ([package_explain.zi](../cmd/package_explain.zi));
  every row and code checked in [target_diagnostics.sh](../tests/target_diagnostics.sh).)
- [x] Reject old incompatible bundles with an actionable rebuild diagnostic. Keep legacy-format rejection tests; do not maintain two executable formats indefinitely. (Legacy-revision rejection retained in [zx_executable.sh](../tests/zx_executable.sh); the old VM and TinyCC ZX route are removed per [step 09](09-native-runtime-and-integration.md).)


## Tests

Extend the retained bundle checks: [provider cases](../tests/bundle_provider_cases.zi), [executable bundles](../tests/executable_bundle_cases.zi), [global reachability](../tests/bundle_global_reachability.sh), and [step limits](../tests/bundle_step_limit.sh).

Add malformed fixtures for future sections, required feature mutations and proof-table tampering. Revision 29 variant records, native selection, feature masking and target-only rejection are covered by the [assembly regression](../tests/generic_assembly_cases.zi) and [executable bundles](../tests/executable_bundle_cases.zi).

## Exit gate

New bundles execute using step 04 without semantic `.zir` bodies. Libraries, assets, state, capabilities, law reporting, and diagnostics retain their defined behavior. Old bundles fail clearly. The writer and reader agree on a documented public instruction/container contract rather than private compiler serialization.

## Handoff

Deliver the binary/text format specification, opcode version policy, golden/mutation fixtures, variant contract, and the new bundler/loader path. Keep the installed default unchanged until step 10's integration gates pass.
