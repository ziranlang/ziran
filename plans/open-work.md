# Open work

One line per undone item, with its location and how to test it. Delete the
line when the work lands. Keep detail in the referenced plan.

## Native assembly target (ASSEMBLY/)

- M4 ARM rows: ARMv7 unchecked encoder forms, linear image relocations on
  ARM, and standalone AArch64/ARMv7 ELFs (`zx2native --target`); test with
  `make android-parity` on the Moto (armv7) and Pixel 4a (aarch64). Detail:
  plans/2026-11-15-m4-native-arm.md, rows 5-7.
- M5 objects and linking: x64 unwind data (`.pdata`/`.xdata`), Mach-O and
  archives, the external C ABI with foreign libraries, a console line mode,
  files and clock platform services, a real Windows host run, and an
  in-process loader; test with `tests/zx_windows_parity.sh` plus per-row
  tests. Landed slice: `zx2native --platform windows` writes PE32+.
- Optional, owner-deferred: the loader refuses an unchecked `.zx` bundle the
  user marks untrusted (`cmd/compiler_exec_link.zi`); test with a
  marked-bundle load case.
- M6 inspection and cutover: unwind, debug and profiler mapping, then the
  owner's full before/after ZX comparison on Alpha's physical host.
- Native code quality: warm native ZX remains slower than the M0 TinyCC tier
  and the engine on some kernels. Active work: plans/2026-11-15-native-speed.md,
  measured against `bench/results/` receipts.
- Explicit gaps kept as named diagnostics: resizable-collection ownership,
  reentrant callbacks, general native string and arbitrary-host instructions
  (ASSEMBLY/gap-policy.md).
- Deferred metaprogramming until self-hosting reaches S5: `#modify` notes,
  `type_info`, the compiler message loop, build-as-Ziran-program,
  `#bake_arguments`, SOA layout.

## Compiler and tools

- Self-hosting S16-S18: port the remaining compiler C to Ziran (portable
  linking, the ZX command runner, the embedding and execution APIs, the Wasm
  provider, inline assembly, the editor service and the tool drivers), then
  prove the stage-0 to stage-2 fixed point; gate every step with
  `make check-bootstrap`. Detail: plans/2026-11-14-self-hosting.md (active
  lane).
- Digest-keyed frontend check cache: reuse parse and check results keyed by
  toolchain format version, module and import digests, and defines, reusing
  only at module/declaration boundaries; precedents
  `cmd/compiler_backend_cache.zi` and `cmd/compiler_native_cache.zi`;
  validate identical diagnostics and IR with and without the cache.
- `ziran test` driver plus unused-name lints (locals and imports,
  warnings-only): dispatch in `cmd/package.zi`, checks in
  `cmd/zir/zir_check_program.c`; validate with formatter-style cases and run
  the lints over `cmd/` before enabling.
- Vec growth reuses storage when nothing borrows it: clear stale-alias host
  twins and keep the Vectors checksum 94905000 green; measure Vectors on
  Alpha before and after.
- Unique-use Insert-in-place and borrowed call arguments shared across the
  engine, Wasm, zi2c and machine IR; measure the Records kernel against C
  (receipt wasm-tier-alpha-2026-11-15-native.json).
- Proof-carried bounds elision under `--checks`: extend `LirProvenBuild`
  (`cmd/compiler_lowered_proven.zi`) with dominance-scoped slice and heap
  facts plus multi-variable index guards through `ProofKernel::DecideOrder`;
  `MachineVerify::DirectProven` stays conservative; test with
  `tests/proven_pointer_cases.zi`, `tests/guarded_index_ranges.sh`,
  `tests/trust_boundary_cases.zi`.
- Language quick wins: reserve the `zi_` generated-name prefix, add
  statement-level parse recovery with a per-region error cap, and list
  ambiguity candidates in `module.ambiguous`/`package.ambiguous`; new codes
  enter the explain registry and `tests/explain.sh` must pass; clean-file
  `.zir` output stays byte-identical.
- Several parse errors per file: a recovery point around each top-level declaration, added where the parser port puts recovery (today ParserWithRecovery in cmd/zir/zir_parse.c covers a whole parse).
- Exact caret columns inside a declaration's value: re-land the PartSpan span change (reverted in 527c3d0b) once the saved-IR codec stores literal values instead of reparsing statement-text slices.
- `ziran init` keeps a project's floating `[toolchain] ref` rather than adopting a template's exact pin (as `kryon init` does): the merge rule keeps the project's own values, it already prints "kept ref = ...; the template has ...", and `ziran.lock` pins the commit either way.
- Checks: merge the two Go assembly wrappers into `go_codegen` once the
  feature evidence names their `_cases.zi` programs, make
  `make check-incremental` cheaper than `make check`, and turn the twelve
  one-line shell launchers into `check_programs` rows; test with
  `make check CHECK_FILTER=go_`. Detail and the removed-check mapping:
  plans/2026-11-23-go-check-groups.md.
- Owner-gated: a persistent `ziran serve` session (owner-started, explicit
  Exit, never a daemon), only after the check cache lands.
- Deferred Lean-study proposals: polymorphic-default postponement,
  `using ... in` scopes, zi2c reuse of optimizer facts, bounded translation
  validation, `partial`/`unsafe` tainting, co-located structural laws
  (rejected alternatives: see Rejected research below).

## Rejected research (do not adopt)

From the Lean 4 study (2026-11-18), measured-only rule: runtime reference
counting or RC headers; tagged scalars and boxed value words; closure
unboxing; Lean's Task thread pool or any permanent background service
(`std/thread_linux.zi` pthreads with explicit joins suffice); lambda_RC's
formal borrow calculus as a source contract; Perceus-style reuse checks for
immutable structures; syntax extension or notation commands; type classes
and instance-implicit search; coercion class chains; auto-bound implicits;
dependent types or proof terms in the checked language; mandatory
proof-carrying indexing; `termination_by` proofs for all procedures;
invoking Lean or an SMT solver inside `ziran` or `.zx` loaders; trusting
cached proof bits without re-check; for now multi-process worker isolation,
intra-command incremental elaboration, and remote artifact caches.

## Benchmarks

- Publish the Kryon visual-workload table (mean and p95 per scene and
  resolution) once a clean pushed Kryon revision is measured; validate frame
  hashes, sample counts and the Ziran pin; entry point
  `bench/bench.zi -- wasm-tier --standard --host-gate --samples 11`.
- Trace the k-nucleotide 128 KiB fixed-array value-copy loop in the native
  and Wasm lowerings before changing the benchmark source; preserve value
  semantics for arrays that may escape or alias.
- `arrays.zi` and `loops.zi` example programs trail the Wasm tier by 25-40%;
  profile each print-heavy loop before optimizing.

- Strict five-core ASSEMBLY before/after campaign: the report processor
  (`bench/assembly_compare.zi`) and collector (`bench/assembly_collect.zi`)
  wait for the new-native ZX cell; run collection only on Alpha's physical
  host with fresh attestation; test: `--validate` accepts the plan and the
  generated report lists no missing core cells.
- Document `bench/results/zx-compare-alpha-physical-native-2026-11-16-release.json`
  in `bench/zx_compare.md` (extract medians and context from the receipt);
  test: quoted numbers match the receipt JSON.

## Learn

- Record more real builds, instruction-window search on CPU, a learned cost
  model, Alpha GPU experiments, and learned compiler decisions
  (plans/2026-11-15-learning-compiler.md); measure each on Alpha through
  `bench/bench.zi`.

## Proof and semantics

- Law-model induction over arbitrary sequences and sorting (docs/LAWS.md,
  proofs/); test with `make proof-model` on the pinned toolchain.
- Precise alias representation for owned values through globals and defined
  allocation-failure behavior (docs/OWNED_VALUES.md); extend the borrow and
  owned-field tests when landed.

## Environment

- `zstd_go` needs Go 1.25; Omega defaults to 1.24.4, so that check fails
  locally until the toolchain updates.

## Unstarted design (awaiting owner decision)

- Shared canonical Ziran/Kryon checkouts with a cross-project
  content-addressed output cache, leases, telemetry and a Projects manifest
  index (plans/2026-11-12-shared-ziran-kryon-builds.md).
