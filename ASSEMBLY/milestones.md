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
  ZX container and the native encoder. Listings are views of encoder records,
  not a write-and-reparse stage.
- Every boundary is verified and bounded; unsupported constructs fail
  explicitly instead of silently falling back to C or TinyCC.
- Measurement rules (Alpha physical host, raw samples, symmetric 1.5×IQR) are
  strict enough to make the final comparison trustworthy.

What needed to change:

1. **Big-bang completion.** The owner's requested ZX speed comparison was gated
   behind step 10, which also requires AArch64/ARMv7/Windows/macOS/wasm
   backends, COFF/Mach-O linkers, signing and profiler integration. Nothing
   measurable would reach the owner until all of that existed. The milestones
   below deliver labeled interim comparisons on the first platform row.
2. **The baseline came last.** Step 10 says to preserve the old engines'
   "before" measurement before replacing them, but scheduled it at the end. The
   existing `bench/results/zx-compare-alpha-embedded.json` was measured inside
   Alpha's personal KVM guest, which the 2026-11-11 benchmark rule excludes, so
   no valid "before" exists today. Capturing it is now milestone M0.
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
  default to the new format only after its own gate passes; other rows keep the
  old engine until then. There is never a silent fallback between formats.
- Interim measurements follow every rule in step 10, are labeled *interim*,
  and never replace the final full comparison.
- Hostile-input hardening is mandatory at trust boundaries (`.lirb`/`.zx`
  decoders, loaders, host results). Internal stages rely on the verifier
  instead of duplicating its checks.

## Milestones

| Milestone | Scope | Exit evidence |
| --- | --- | --- |
| **M0** Integrate and baseline | Land the in-flight slices behind one passing local `make check` and `make check-bootstrap`. On Alpha's physical host, measure the old portable VM and old TinyCC native ZX with the M1 workloads, hashing and preserving the exact old binaries. | Green local suites; baseline receipt with raw samples under `bench/results/`. |
| **M1** Workload-complete portable path | Typed host capability calls (`s64 -> s64` first, then scalar/string/record signatures), in-bundle `--bind` providers, immutable strings (literal, copy, `.count`, checked index), runtime global initializers. The revision-28 container lists capabilities recomputed from reachable `HostCall` instructions. Add a C embedding API for the new engine (open, list capabilities, bind, run with fuel) that the existing comparison harness can drive. | Corpus-coverage report shows every benchmark workload lowering; source, saved IR and new ZX agree with the oracle; **interim comparison 1**: new portable engine vs old VM. |
| **M2** Register-allocated x86-64 | Liveness, linear-scan allocation with splitting and spilling, a post-allocation verifier, and machine-IR-to-x86-64 encoding that reuses the existing byte encoders. Fuel at loop back-edges and calls. Constant-time token dispatch. | Every x86-64 fixture passes on the machine-IR path; the direct LIR encoder is removed. |
| **M3** Native ZX on Linux x86-64 (first cutover row) | In-memory loading with W^X pages, relocation of the same encoder output, host trampolines, stack and fuel checks, and a content/CPU/backend-keyed cache. `ziran run app.zx` selects revision 28 on linux-x86_64, and revisions 26/27 get the rebuild diagnostic there. | Full local suite on the new default for that row; installed tool verified; **interim comparison 2**: new native ZX vs TinyCC native ZX, plus AOT and C controls. |
| **M4** ARM rows and direct wasm | AArch64 and ARMv7 (USB Moto) machine targets, and direct wasm lowering from LIR. Each row is cut over separately. | Per-row execution evidence; Moto comparison on the device. |
| **M5** Objects, linker and foreign libraries | Relocatable ELF/COFF/Mach-O, archive reading, external C ABI and foreign libraries, Windows/macOS ABIs and required signing. | Step 07/08 rows with real launches. |
| **M6** Inspection closure and final cutover | Unwind/debug/profiler mapping, removal of the TinyCC/generated-C ZX path, and the owner's **full before/after ZX comparison**. | The step-10 completion receipt. |

## Current position

M0 integration is in progress. The revision-28 container, frame-local
`Allocate`, local scalar address-taking and the pre-allocation machine-IR
boundary are implemented with focused local tests. The full local suite passes
except for the site-page checks owned by the separate website-layout task. The
Alpha baseline has not been captured yet. M1 starts with the two constructs
blocking `bench/zx_compare.zi`: host capability calls and string values.
