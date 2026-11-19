# Learning from Lean 4: Verification Proposals for Ziran (2026-11-18)

Ziran already mirrors part of Lean's split between small trusted checkers and untrusted optimization: manual safety by default (`docs/USAGE.md`), opt-in `--checks`/`#checks`, static pointer/index facts (`cmd/compiler_lowered_proven.zi::LirProvenBuild`), machine verifiers (`cmd/compiler_machine_verify.zi::MachineVerify`, `cmd/compiler_machine_verify_allocated.zi::MachineVerifyAllocated`), `#law`/`#proof` (`cmd/compiler_proof_kernel.zi::ProofKernel`), and an offline Lean soundness model (`proofs/Scalar.lean`, `proofs/Oracle.lean`). Below are ranked gaps and next steps.

## Ranked proposals

### 1. Proof-carried loop-stable pointer hoisting & bounds elision (`GetElem` / `get_elem_tactic`)
- **Lean:** `GetElem` decouples total proof-carrying `xs[i]` / `xs[i]'h` (discharged by `get_elem_tactic` from loop/branch context) from checked `xs[i]?` and trapping `xs[i]!` ([`Init/GetElem.lean`](https://github.com/leanprover/lean4/blob/master/src/Init/GetElem.lean), [Lean Reference](https://lean-lang.org/doc/reference/latest/)).
- **Ziran now:** `cmd/compiler_lowered_proven.zi` (`PointerStep`, `IndexFacts`, `LirProvenIndexBound`) proves only `Global`/`Frame` roots and constant-count views; `cmd/compiler_machine_lower.zi` (`DirectBase`, `LowerMemory`, `ProvenBound`) emits `PointerDirect` only on x86-64 and falls back to per-access `LowerResolve` on heap/slice/vector pointers.
- **Change:** Extend `LirProvenBuild` with dominance-scoped slice/heap pointer facts invalidated at `free`, resize, or unknown host/write calls; hoist one `PointerResolve` before a barrier-free loop, emit `PointerDirect` inside it, and verify dominance/invalidation in `cmd/compiler_machine_verify.zi::DirectProven` and `CheckedMemory`. Enable the same proven index elision in AArch64 and `cmd/compiler_zx_wasm_code.zi::LowerMemory`.
- **Win & proof:** Removes inner-loop `PointerResolve` overhead in `--checks`/`#checks` builds (trust-tiers Step 1, `plans/2026-11-15-native-speed.md`); shown by `tests/proven_pointer_cases.zi`, `tests/guarded_index_ranges.sh`, `bench/wasm_tier.py` (`Vectors`, `Hash`, `Sieve`), and Kryon `Button`/`Text`/`Input` widgets under `--checks`.
- **Risk:** Missing a resize/alias invalidation silently drops a trap; mitigated by keeping `MachineVerify::DirectProven` conservative across any call/store that may resize.

### 2. Feed `#law`/`#proof` and `ProofKernel` into bounds-check discharge (`Decidable` / `decide`)
- **Lean:** `Decidable` bridges propositions and computable witnesses, and `decide` closes decidable arithmetic/order goals inside the kernel ([`Init/Core.lean`](https://github.com/leanprover/lean4/blob/master/src/Init/Core.lean), [Lean Reference](https://lean-lang.org/doc/reference/latest/)).
- **Ziran now:** `cmd/compiler_proof_kernel.zi` (`CloseGoal`, `DecideOrder`) and `cmd/compiler_proof.zi::EvaluateTheorem` verify `#law`/`#proof` at `ziran check` time, disconnected from `LirProvenBuild`; `cmd/compiler_bundle.zi` (`ExecutablePayload`, `ExecutableDecodeInternal`) hardcodes `proof_status == 0`.
- **Change:** Add a bounded query from `IndexFacts` to `ProofKernel::DecideOrder` for multi-variable linear index guards (`i + stride < len`, `lo <= hi <= count`) that interval widening loses, and consume caller `#law` preconditions when lowered to LIR. Keep `proof_status` advisory only—recheck at load time.
- **Win & proof:** Drops remaining bounds checks on 2D/strided indexing in `tests/guarded_index_ranges.sh`, `tests/proven_index_ranges.sh`, and `bench/wasm_tier.py` (`Vectors`, `Hash`); covered by `tests/laws.sh` and `make proof-model`.
- **Risk:** `ProofBudget` (100,000) exhaustion must conservatively keep the runtime check and never cache unverified status.

### 3. Bounded translation validation for optimizer rewrites and machine encoders (small trusted kernel)
- **Lean:** Tactics and elaborators are untrusted; a small C++ kernel (`src/kernel/`) checks every resulting term independently ([`environment.cpp`](https://github.com/leanprover/lean4/blob/master/src/kernel/environment.cpp), [Lean Reference](https://lean-lang.org/doc/reference/latest/)).
- **Ziran now:** `cmd/compiler_opt.zi::VerifyPass` only re-runs `cmd/compiler_lowered_verify.zi::LirVerify` (structural/SSA/dominance, not semantic equivalence). `MachineVerifyAllocated` stops before `cmd/compiler_x86_64.zi`, `compiler_aarch64.zi`, and `compiler_armv7.zi` byte emission; `cmd/compiler_exec.zi::LirExecPlanBuild` validates decoded LIR only.
- **Change:** Add a `ProofKernel`-backed symbolic equivalence check for acyclic local/peephole rewrites (`compiler_opt_rotate.zi`, `compiler_opt_licm.zi`, and `plans/2026-11-15-learning-compiler.md` window search), plus a decode-back check in encoder tests comparing emitted bytes against `MachineInstruction` operands/relocations.
- **Win & proof:** Catches silent miscompilations in optimizer/encoder changes before benchmark regressions; shown by `tests/compiler_opt_rotate_cases.zi`, `tests/regalloc_cases.zi`, `tests/x86_encoder_cases.zi`, `tests/aarch64_encoder_cases.zi`, and `tests/armv7_encoder_cases.zi`.
- **Risk:** False rejections on side-effecting or floating-point ops; restrict translation validation to pure scalar/bitwise windows and keep encoder roundtrip checks in test/debug mode.

### 4. Explicit `partial` / `unsafe` tainting for untrusted `.zx` bundles and foreign boundaries
- **Lean:** `partial` and `unsafe` annotations isolate non-termination and kernel-bypassing code so safe definitions cannot silently depend on `unsafe` ([`environment.cpp`](https://github.com/leanprover/lean4/blob/master/src/kernel/environment.cpp), [Lean Reference](https://lean-lang.org/doc/reference/latest/)).
- **Ziran now:** `cmd/compiler_machine_verify_allocated.zi::MachineVerifyFuel` enforces fuel placement and `cmd/compiler_exec_link.zi::ValidateReachable` recomputes host capabilities, but `plans/2026-11-15-manual-safety.md` notes the loader still accepts unchecked `.zx` bundles even when marked untrusted, and `#law_waive` leaves no runtime load policy bit.
- **Change:** Record a bundle's checked/waived/foreign taint summary in `ExecutableDecodeInternal` (`cmd/compiler_bundle.zi`), and reject unchecked or waived bundles when a host opens them in untrusted mode—without altering default trusted execution.
- **Win & proof:** Closes the open untrusted-bundle gap in `plans/2026-11-15-manual-safety.md` with zero cost to default native/widget speed (`Records`, `Vectors`, Kryon widgets); verified by `tests/trust_boundary_cases.zi` and `tests/verified_loading_cases.zi`.
- **Risk:** None to default builds if untrusted-bundle rejection is opt-in per loader policy.

### 5. Co-located `#law` / `#assert` structural invariants beside hot compiler & std types (`example` / `theorem`)
- **Lean:** `example` and `theorem` live directly next to code and run in the checker with zero emitted runtime bytes ([Lean Reference](https://lean-lang.org/doc/reference/latest/)).
- **Ziran now:** `#law` (`type`, `bounds`, `size`, `effect`, `abi`, `theorem`) and `#assert` exist (`docs/LAWS.md`), while compiler/ABI layout invariants (e.g., `MachinePointer` bit shifts, `ProofKernel` constants) are mostly asserted in separate `tests/` scripts.
- **Change:** Place lightweight `#law size` / `#law effect` / `#assert` checks directly beside key compiler/std records (`cmd/compiler_machine_ir.zi`, `std/`) so `ziran check` validates ABI/layout assumptions immediately, keeping heavy `#proof` blocks in dedicated proof modules.
- **Win & proof:** Catches layout/effect regressions at `ziran check` before `make check` runs downstream tests; tracked by `tests/laws.sh` and `tests/api_assertions.zi`.
- **Risk:** Slowing incremental checks if heavy symbolic proofs sit in widely imported headers; limit inline laws to fast structural/constant checks.

## Coordinator questions
1. **Can static proofs drop provable checks?** Yes, selectively—Ziran already does this in `LirProvenBuild` + `MachineVerify::DirectProven` for global/frame roots and interval-bounded indices on x86-64/Wasm. Static proofs can drop more checks in `--checks`/`#checks` builds (Rank 1–2) once dominance/resize-barrier rules prove an alias stays live, while default builds already omit defensive checks (`docs/USAGE.md`).
2. **Translation validation of optimizer passes and encoders?** Viable for bounded local passes (peephole windows, `compiler_opt_rotate.zi`, `compiler_opt_licm.zi`, learned rewrites) via `ProofKernel` + `proofs/Scalar.lean`, and for machine encoders via test-time decode-and-match against `MachineProgram`. Not viable as a whole-program equivalence prover across loops, memory aliasing, and foreign calls.
3. **Lightweight contracts?** Yes, as optional `#law`-style pre/postconditions that either discharge `#checks` via `LirProvenBuild`/`ProofKernel` or compile to `--checks` entry traps, evaporating to zero overhead in default unchecked builds.

## Ideas that don't fit Ziran's model
- **Mandatory proof-carrying indexing (`xs[i]'h`) everywhere:** Conflicts with Ziran's C-like manual safety default (`docs/USAGE.md`).
- **Global termination proofs (`termination_by`) for all procedures:** Ziran uses bounded runtime fuel (`MachineVerifyFuel`) and unbounded native loops instead of requiring well-founded recursion proofs.
- **Full dependent types (`Type u`, proof terms in AST/LIR):** Breaks Ziran's small, deterministic C/Ziran compiler and minimal `.zir`/`.zx` representations.
- **Trusting cached `.zx` proof bits without re-checking:** Violates `docs/LAWS.md` and `ValidateReachable`; loaders must always re-verify (`LirVerify`).
- **Invoking Lean or an SMT solver inside `ziran` / `.zx` loaders:** Violates `docs/LAWS.md`: the compiler and runtime carry zero external solver dependency (`proofs/` is offline CI only).
