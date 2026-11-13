# Delivery milestones and plan review

The ten step files define **what** 100% means. This file defines the **order**
in which their checklist items are closed, so that real execution and speed
evidence arrives in stages instead of only after every platform, object format
and profiler is finished. No milestone below changes a step's exit gate, and no
milestone is the completed upgrade; only [step 10](10-inspection-performance-and-cutover.md)
can declare that.

## Review of the ten-step plan (2026-11-12)

What the plan gets right:

- One verified portable instruction program (LIR) feeds the interpreter, the
  ZX container; native always reads verified ZX before encoding. Listings are views of encoder records,
  not a write-and-reparse stage.
- Every boundary is verified and bounded; unsupported constructs fail
  explicitly instead of silently falling back to C or TinyCC.
- The [gap policy](gap-policy.md) makes missing work observable: a gap has a
  stable error and an open matrix cell, never a substituted engine.
- Measurement rules (Alpha physical host, raw samples, symmetric 1.5×IQR) are
  strict enough to make the final comparison trustworthy.

What needed to change:

1. **Big-bang completion.** The owner's requested ZX speed comparison was gated
   behind step 10, which also requires AArch64/ARMv7/Windows/macOS/wasm
   backends, COFF/Mach-O linkers, signing and profiler integration. Nothing
   measurable would reach the owner until all of that existed. The milestones
   below deliver labeled interim comparisons on the first platform row.
2. **The baseline came last.** Step 10 says to preserve the old engines'
   "before" measurement before replacing them, but scheduled it at the end.
   The plan now requires a direct physical-host baseline at milestone M0 and
   rejects virtualized receipts.
3. **Breadth without a target workload.** Lowering grew construct by construct,
   while the actual comparison workload, [`bench/zx_compare.zi`](../bench/zx_compare.zi),
   still cannot lower: it needs `#foreign host_api` calls and string values.
   Step 03 work is now ordered by a measured corpus-coverage report, starting
   with the benchmark workloads.
4. **Two native code paths.** The direct LIR-to-x86-64 encoder kept gaining
   features while the [machine IR](machine-ir.md) boundary was being built
   beside it, so every native feature risked being implemented twice. From now
   on the direct encoder receives fixes only; new native features land on the
   machine-IR path, and the direct encoder is deleted once M2 passes.
5. **Speed-critical shortcuts were not tracked as debt.** Native procedure and
   pointer tokens are dispatched by a linear scan, native code is stack-slot
   only, and native `Fuel` emits no bytes, so native output does not yet satisfy
   the ZX budget contract. These are now explicit M2/M3 exit criteria.
6. **Stale status.** Step 05 was still "planned" after the revision-28
   container landed, and the README listed the container and machine IR as
   wholly outstanding. The statuses now match the code.
7. **Unbounded extras.** The textual-listing re-assembler, the GNU inline
   assembly template surface, 32-bit x86 and RISC-V are post-cutover
   extensions unless the step-01 matrix names them as maintained rows.

Recorded decision: the checked-IR-to-LIR adapter stays in C
([`zir_asm_lower.c`](../cmd/zir-asm/zir_asm_lower.c)) because the checker's
program records are C structures. Everything after LIR (verifier, codec,
engine, optimizer, machine IR, encoders, container, linker) stays in Ziran.
Step 03's proposed `compiler_lower*.zi` modules apply only if the checker
itself moves to Ziran.

## Rules that apply to every milestone

- A new language feature lands in LIR, the verifier, the codec and the portable
  engine first. Native support follows on the machine-IR path.
- Each row of the platform matrix is cut over separately. A row switches its
  default to the new format only after its own gate passes. Before that switch,
  the direct target is a named Gap and fails; an explicitly selected legacy
  command remains a comparison tool, never a fallback.
- Portable, native, Wasm, C and Go execution are separate targets. A request for
  one never routes to another. Missing CPU features, host services, `#asm`
  variants, parallel/GPU support, object formats or loaders are preflight errors.
- Every Gap has the stable diagnostic and side-effect-free negative fixture
  required by the [gap policy](gap-policy.md). A fallback, downgrade, skip,
  emulated run or legacy-engine result cannot close a milestone.
- Interim measurements follow every rule in step 10, are labeled *interim*,
  and never replace the final full comparison.
- Hostile-input hardening is mandatory at trust boundaries (ZX container and instruction
  decoders, loaders, host results). Internal stages rely on the verifier
  instead of duplicating its checks.

## Milestones

| Milestone | Scope | Exit evidence |
| --- | --- | --- |
| **M0** Integrate and baseline | Land the in-flight slices behind one passing local `make check` and `make check-bootstrap`. On Alpha's physical host, measure the old portable VM and old TinyCC native ZX with the M1 workloads, hashing and preserving the exact old binaries. | Green local suites; baseline receipt with raw samples under `bench/results/`. |
| **M1** Workload-complete portable path | Typed host capability calls (`s64 -> s64` first, then scalar/string/record signatures), in-bundle `--bind` providers, immutable strings (literal, copy, `.count`, checked index), runtime global initializers. The revision-28 container lists capabilities recomputed from reachable `HostCall` instructions. Add a C embedding API for the new engine (open, list capabilities, bind, run with fuel) that the existing comparison harness can drive. Unsupported operations are Gap diagnostics, not calls into the old VM. | Corpus-coverage report shows every benchmark workload lowering; source, saved IR and new ZX agree with the oracle; negative fixtures account for every remaining Gap; **interim comparison 1**: explicitly selected new portable engine vs explicitly selected old VM. |
| **M2** Register-allocated x86-64 | Liveness, linear-scan allocation with splitting and spilling, a post-allocation verifier, and machine-IR-to-x86-64 encoding that reuses the existing byte encoders. Fuel at loop back-edges and calls. Constant-time token dispatch. | Every x86-64 fixture passes on the machine-IR path; the direct LIR encoder is removed. |
| **M3** Native ZX on Linux x86-64 (first cutover row) | In-memory loading with W^X pages, relocation of the same encoder output, host trampolines, stack and fuel checks, and a content/CPU/backend-keyed cache. `ziran run app.zx` selects revision 28 on linux-x86_64; unsupported native features fail preflight without starting the portable engine, and revisions 26/27 get the rebuild diagnostic. | Full local suite on the new default for that row, including no-fallback negative fixtures; installed tool verified; **interim comparison 2**: explicitly selected new native ZX vs explicitly selected TinyCC native ZX, plus AOT and C controls. |
| **M4** ARM rows and direct wasm | AArch64 and ARMv7 (USB Moto) machine targets, and direct wasm lowering from LIR. Each row is cut over separately. | Per-row execution evidence; Moto comparison on the device. |
| **M5** Objects, linker and foreign libraries | Relocatable ELF/COFF/Mach-O, archive reading, external C ABI and foreign libraries, Windows/macOS ABIs and required signing. | Step 07/08 rows with real launches. |
| **M6** Inspection closure and final cutover | Unwind/debug/profiler mapping, removal of the TinyCC/generated-C ZX path, and the owner's **full before/after ZX comparison**. | The step-10 completion receipt. |

## Current position

The experimental `zi2asm` pipeline now optimizes verified LIR before producing
ZX, and native source compilation serializes and reads those same ZX bytes.
The standalone instruction artifact/CLI is removed; source LIR remains an
inspection view. Direct Linux x86-64 hello has short constants, fallthrough
edges, frame-free literal writes and compact adjacent read-only file data.
Retained tests require text below 200 bytes, ELF below 1.5 KB and valid retained
function extents, with inlined platform service declarations omitted rather than trap
aliases. Pointer token runs lower with bounds-checked base/index arithmetic.
These fixes do not close M2/M3: machine-IR service lowering, native fuel,
source debug origins and broader ABI coverage remain gaps. Wasm still lowers
semantic ZIR, not revision-28 executable instructions.

M0 integration is in progress. M4's first slice, the explicit
[target registry and preflight](07-architecture-and-platform-coverage.md#target-registry-and-preflight),
lists every CPU/OS row with its gap-policy state; no row is Implemented yet,
so every general registry selection fails preflight with
`assembly.gap.target-unimplemented`. The separate opt-in `zi2asm` prototype
accepts only `x86_64-linux`; other target rows fail with the stable native
platform-service gap before input or output. The revision-28 container, frame-local
`Allocate`, local scalar address-taking and the pre-allocation machine-IR
boundary are implemented. On the final synchronized physical-Alpha cohort, the
four required focused executor scripts and `make check-bootstrap` passed. The
full `make check CHECK_JOBS=2` result was 275/276: its only failure was the
unrelated stale generated site-page check for `guide/usage.html`,
`guide/packages.html` and `guide/language-direction.html`. The earlier 274-check
result remains historical, not the current full-suite claim. M0's Alpha
physical-host baseline and M1's interim comparisons exist; see the
[baseline](../bench/zx_compare.md#interim-comparison-1-new-portable-engine-2026-11-12)
and [final pointer-dispatch cohort](../bench/zx_compare.md#final-pointer-dispatch-cohort-2026-11-13).

`bench/zx_compare.zi` runs through scalar host capabilities and strings on the
new portable engine with oracle-correct results. Word-backed scalar slots,
cached plan facts and frame bases, flattened immutable instructions,
pointer-based instruction/edge dispatch, proven-safe direct edge copies and
dead-source aggregate `Insert` aliasing reduced the lowered scalar median from
5.820 to 2.339 ms (59.81%) and structured median from 46.52 to 8.773 ms
(81.14%). Focused tests cover optimized and still-live aggregate cases and
non-coalescible loop inputs. In this receipt the new engine is 1.024× slower
than the old VM on scalar and 1.079× slower on structured work. The remaining
native speedup requires completion of M2/M3 integration and runtime work.

Parts of M2 are now implemented for Linux x86-64 SysV: deterministic liveness
and linear-scan register allocation with splitting/spilling, an independent
allocated-program verifier, 16-byte-aligned frame layout, and direct encoding
of verified allocated machine IR into an in-memory image with relocations. The
opt-in `zi2asm --machine --emit elf` pipeline now writes and executes that image
without external compilation tools. This does not close M2. Completing the
promised call and instruction surface, removing the direct LIR encoder,
and passing every M2 exit fixture remain required.

The M0/M1 implementation also covers dependency-ordered once-only runtime
initializers, local aggregate/procedure address-taking, checked portable slices
and string comparison, native readonly data, the first no-libc Linux x86-64
console veneer and validated typed host values
with instance-owned returned bytes. Resizable collection ownership, reentrant
callbacks and general native string/arbitrary-host instructions remain explicit gaps;
none of the M2–M6 exit criteria is claimed by this subset.
