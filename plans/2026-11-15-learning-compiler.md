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

## Current baseline (where machine learning can help)

Alpha physical host, `bench/results/wasm-tier-alpha-2026-11-15-records.json`,
revision `77a7ef04`, 2026-11-15T23:27Z, median of 11 samples after 1.5×IQR.

| Kernel | C ms | Native ELF ms | Native/C |
| --- | ---: | ---: | ---: |
| Sieve | .0336 | .1432 | 4.3 |
| Records | .0130 | .0611 | 4.7 |
| Hash | .0282 | .1041 | 3.7 |
| Vectors | .0376 | .9677 | 25.8 |
| Calls | .0111 | .0067 | 0.6 (constant-folded) |
| Printing | .0098 | .3294 | 33.8 |

Records went from 297× C to 4.7× today. The open native-speed lanes own the
remaining Vectors and Printing gaps, which are structural compiler work. The
learner is a parallel track for the remaining 5–20% and for code size, not a
replacement for that plan. Always re-read the newest receipt before comparing.

## Hardware

- Alpha: NVIDIA RTX 5060 Ti, 16 GB, driver 595.99.02, idle. The CUDA toolkit
  (`nvcc`) is not installed. Alpha has 15 GB RAM, often about 5 GB free, and it
  is also the benchmark host: training must not run while a benchmark is
  measuring, or it adds noise to the receipts.
- Omega: 32 cores, 30 GB RAM. Corpus processing and CPU search run here.
- Ziran has no GPU backend yet (`#parallel_gpu` is a named gap on every target).

## Steps

Recording already exists: `ziran learn on|off|status` (`cmd/package_learn.zi`,
documented in `docs/ZX.md`), off by default, deduplicated by SHA-256 and capped.

### 1. Record more of real use

- `ziran run FILE.zi` on the native backend (runs through `zi2zx run` without a
  saved ELF today).
- Direct `zx2native` calls from `bench/` harnesses. Benchmark receipts already
  name each artifact by `artifact_sha256`, so timings join to corpus records by
  digest without changing the receipt format.
- A Harmony settings switch that runs `ziran learn on|off` on Omega and Alpha,
  inline in the existing settings modal (owner rule: no new screens).

### 2. Instruction-window search (CPU, Omega, on demand)

A Ziran tool reads the corpus's machine maps, takes short instruction windows
that occur often, and searches for shorter equivalent sequences. Candidates are
tested on random inputs, then checked with the proof kernel
(`cmd/compiler_proof_kernel.zi`) or differential tests. Accepted windows become
a report of proposed rewrite rules for review, never automatic compiler edits.
It runs only when started, through build-queue.

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
