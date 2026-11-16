# A compiler that learns from Harmony builds (2026-11-15)

Owner, 2026-11-15: "i would really like to have some kind of artificial or
machine learning stuff that learns with me as i use harmony, especially related
to ziran how we can improve ziran. it doesnt really need to be good at english,
but good at machine code maybe even at assembly."

Owner, 2026-11-15, on the first plan: "recording sounds pretty good we dont want
to waste too many resources so we should have an enable disable feature ... we
already improved it a lot more and we have a 5060 ti gpu available on alpha we
can utilize".

The idea: the verifier and tests decide whether a change is correct, Alpha's
benchmarks decide whether it is faster, and the byte count decides whether it is
smaller. A learner proposes changes and keeps what passes; it never needs to
understand English. Nothing learned reaches the compiler without passing the
verifier and the tests.

Owner, verbatim: "analyze the assembly c genrates leran form it to improve natiev assembyl to we have some machine lerlanig or leraning stuff maybe we can enhance it to leran to improve everytihng".

## Open work

### Remaining C teacher work (2026-11-16)

- Keep future teacher comparisons on a fixed compiler/source epoch. Guard
  launcher, zi2c, zi2zx, zx2native, zx2wasm, libziran and harness digests before
  and after each finite measurement. Preserve raw disassembly, native map
  identity, C parity, SIGPROF IP counts and zero-sample uncertainty. Compare
  the kernel and warm-widget receipts separately from sampled startup costs.
- Verify the actual compiler selected by each project before collecting widget
  evidence. Resolve `ziran pkg path ziran`, hash that selected toolchain, and
  reject a mismatch with report metadata. A current launcher can dispatch to an
  older project pin; use explicit local overrides for controlled comparisons.
  Regenerate checked IR in a fresh output directory and preserve prior output
  for diagnosis. Add the selected-toolchain check to the shared site harness,
  using the Alpha checkpoint as a regression fixture.
- Turn ranked candidates into general transformations. Keep allocation,
  aggregate work with the records lane; keep SSA, loop rotation, LICM,
  induction variables and strength reduction with the loop lane, vector push
  with the records lane, and SIMD reduction with the vectorizer lane, following
  the parent's updated assignment. Keep widget/host calls with the
  widget lane. Use the teacher's selected scalar idioms and bounded leaf
  inliner as the starting point for further measured, parity-covered work.
- Extend `ziran learn` mining beyond constant multiplication. Mine bounded native
  machine/LIR windows paired with C idioms from current-format corpus records,
  grouped by exact compiler and target. A shorter sequence is a proposal,
  never proof. Match widths, signedness, live flags, memory effects, traps and
  register constraints; reject unknown semantics and every disagreement.
  Acceptance requires proof or deterministic edge/random input differential
  execution against generated C plus the retained parity fixtures. The closed
  multiply families already have this gate; widen the vocabulary only with
  explicit semantics and new parity fixtures. Bind every future candidate to
  the corpus ZX, ELF, map and exact compiler identities.
- Harden promotion of the versioned Ziran source table. Record each rule's
  semantic preconditions and verifier/test identity. Unknown table versions,
  targets or unsupported rule IDs fail closed. Compilation reads only the
  checked-in table, with no corpus, clock, random source or online training;
  `ZIRAN_LEARN=off` disables recording; explicit offline verify/train commands
  remain available and cannot alter the compiler already running.
- Extend measured selection beyond the first bounded leaf calibration: unroll
  choices, block layout and allocation policies. The six-kernel leaf comparison
  emitted identical artifacts at thresholds zero and one, so the learned table
  retains threshold zero. Recalibrate after SSA exposes eligible source-level
  leaves. Join corpus artifacts to
  checksum-verified timing receipts by digest; compare on one host with the
  same input and compiler, retain raw samples and symmetric 1.5×IQR exclusions,
  and break ties deterministically by size then policy ID. Treat identical
  native artifacts as one experiment so timing noise cannot select a policy.
  `ziran learn train` already validates corpus identities and writes deterministic
  source plus an exclusion receipt; add more measured legal strategies. Keep correctness
  verification independent of speed scores. Serialize the selected choices
  into the same reviewed source table; never load a mutable user policy during
  ordinary compilation. Scalar leaf inlining and block layout have consumers;
  measure both settings before changing the baseline. Reject unroll factors
  other than one and allocation policies other than zero until their owning
  passes expose alternate, parity-covered legal strategies. The current
  allocator has exactly one verified strategy; never revive its removed retry
  path as a tuning choice. Use integer-nanosecond samples and exact interpolated
  quartiles so filtering does not depend on floating-point rounding.
- Keep future before/after measurements on physical Alpha, following the
  parent's updated host instruction, through `build-queue` and `bench/bench.zi`
  with learning off. Serialize Alpha benchmarks across lanes and never reset
  its checkout during another lane's run. Record the measured pushed revision,
  source/artifact digests and raw samples. Builds and local tests stay on Omega.
  Revalidate integrated changes with parity, `make check` and
  `make check-bootstrap`; a source change during a run requires a new
  checkpoint for that change. Commit only each lane's hunks after local
  validation, then push; preserve the shared index and other lanes' edits.

Keep future corpus work on current `zx<R>-lir<S>` records and exact compiler
identities. A format upgrade must not reinterpret older records; use their
source/entry metadata to rebuild them. Extend the per-opcode execution contract
before widening the learner's semantic vocabulary. Every new recording hook
must honor `ZIRAN_LEARN=off`; benchmark timing must never perform recording.
Update the owner's installed launcher when changes to recording/training are
accepted, and verify that installation independently of builds and pushes.

### 1. Record more of real use

- `ziran run FILE.zi` on the native backend (runs through `zi2zx run` without a
  saved ELF today).
- Direct `zx2native` and `zi2zx run` calls outside benchmarks (`make check`,
  scripts). Benchmark receipts already name each artifact by `artifact_sha256`,
  so timings join to corpus records by digest without recording during a run.
- Move driver-specific host gates (for example `bench/zx_compare.zi`'s
  `HARMONY_ALPHA_*` attestation) onto `bench/bench.zi`'s Ziran host check, so
  no benchmark depends on Harmony variables.
- A Harmony settings switch that runs `ziran learn on|off` on Omega and Alpha,
  inline in the existing settings modal (owner rule: no new screens).

### 2. Instruction-window search (CPU, Omega, on demand)

A Ziran tool reads the corpus's machine maps, takes short instruction windows
that occur often, and searches for shorter equivalent sequences. Candidates are
tested on random inputs, then checked with the proof kernel
(`cmd/compiler_proof_kernel.zi`) or differential tests. Accepted windows become
a report of proposed rewrite rules for review, never automatic compiler edits.
It runs only when started, through build-queue.

Owner, 2026-11-16: "maybe even make proper mathematical proofs that allow us to
self write improvement eventually when we have a way to verify result pixel by
pixel mechanic by mechanic". The acceptance ladder, strongest first:

- Proven rewrite rules: a window and its replacement are equal for every input,
  proved by `cmd/compiler_proof_kernel.zi` (itself checked in `proofs/*.lean`).
  A proven rule may enter the compiler without review of its logic; only its
  speed still needs an Alpha receipt. This is the self-improving loop.
- Learned decisions (inline, unroll, spill): every option is legal and the
  verifier checks the result, so a policy can only change speed.
- Whole-program replay for what cannot be proved: run the old and new build of
  a real program with recorded input, fixed seeds and fixed time, and compare
  every frame's pixels and every step's state. Equality on those inputs, not a
  proof; it needs deterministic replay and Kryon off-screen frame capture.

Proofs scale to rules, blocks and single functions, not to the whole compiler;
per-program translation validation (`cmd/compiler_machine_verify_allocated.zi`
is a first step) is the route past that. The model never has the final say.

### 3. Learned cost model

A small model that predicts a block's cycles on Alpha from its instructions,
trained on corpus records joined with benchmark timings. The search and
instruction selection use it to rank candidates without running each one. Start
on the CPU; the corpus will be small at first.

### 4. Using Alpha's GPU

Needed once models outgrow the CPU (sequence models over instruction streams,
then a small code model fine-tuned on Ziran ↔ LIR ↔ machine-code pairs). Two
routes, both in Ziran per the owner's language rule:

- CUDA through declared FFI from Ziran. This needs the CUDA toolkit installed on
  Alpha (root, through the admin popup).
- Implement `#parallel_gpu` for CUDA in the compiler. This is larger, but the
  learner then also tests Ziran's own GPU support.

Training pauses while an Alpha benchmark is measuring.

### 5. Learned compiler decisions

Replace fixed heuristics (inline or not, unroll, which value to spill) with a
small policy rewarded for size and speed, behind the verifier and `make check`,
as LLVM's MLGO does for inlining and register allocation.
