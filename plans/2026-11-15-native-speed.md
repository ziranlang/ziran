# Native speed: measured gaps and implementation plan (2026-11-15)

Analysis only. No compiler or test source changed. `native` below means the
zx2native ELF; `zx` in the receipt means the instruction engine. These are
separate targets. The initial task table accidentally quoted engine times as
native times. “Calls is close to C” also needs qualification: the measured native
path constant-folds Fib(20), so it does not measure recursive calls.

## Baseline and provenance

Source: `bench/results/wasm-tier-alpha-2026-11-15-native.json`, revision
`c1e3f085056608f12bb5598a9f38cdccfac5ac97`, compiler_dirty=false. Physical
Alpha, Intel i7-8700, virtualization=none, timestamp 2026-11-15T18:54:29Z.
Load averages were **8.51/7.49/6.82 before and 8.43/7.51/6.84 after**, not quiet.
The Omega receipt is a separate host/load observation, not a controlled before
or after comparison. Use Alpha alone for the targets below.

| Kernel | C ms | Engine ms | Native ELF ms | Wasm/V8 ms | Native/C |
| --- | ---: | ---: | ---: | ---: | ---: |
| Sieve | .043014 | .256932 | .236575 | .076771 | 5.50 |
| Records | .013878 | 4.136167 | 4.124353 | .051456 | 297.19 |
| Hash | .036158 | .173276 | .132773 | .063507 | 3.67 |
| Vectors | .043509 | .285839 | .994235 | .086227 | 22.85 |
| Calls | .011414 | .043455 | .006434 | .000146 | .56* |
| Printing | .025817 | .094008 | .334501 | .009722 | 12.96 |

Eleven raw timing samples, symmetric inclusive-interpolated 1.5×IQR filtering.
Native exclusions: Vectors [4,6], Calls [6], Printing [8,10]; the other three
exclude none. The receipt retains every raw sample. Native samples include a
process executing 32 calls, divided by 32; C, engine and Wasm time warm
in-process calls. Compilation is excluded. Native startup remains material for
Calls. Output/checksum parity was checked by the benchmark, not re-established
by this document. The earlier “10–60× faster” claim is not substantiated by a
controlled before/after run here.

The six local `build/wasm-tier/*.native` SHA-256 values match the Alpha receipt:

| Kernel | SHA-256 |
| --- | --- |
| Sieve | 1eb1de3b6088f7860a7cf67015fd0527fa74cbe2e09e288c1498b06e683e2680 |
| Records | 6de9331701d8aa888db68ecc0b1b453c726a0dda863eb00b043c4e6f8d1d4f89 |
| Hash | ba718fdfa8ed1301b4c4e421be6793e776353cccaed6751f3291d008ce40b33c |
| Vectors | 8c07b777f7fec57f11e9e689f81d41cded370e60bea01684b35b9715e8bff3a1 |
| Calls | 6c7aba944044d73292726f391b8516c94b6222c4e91047d73e0fa3fd7e069035 |
| Printing | 09ac7babc51e9c27ac528d8b83e2f867d9d987a8d380e48322c37cb8a76c1e8e |

Alpha's existing on-disk native binaries had changed by inspection time. Do not
attribute their code to this receipt. Sampling uses the matching local bytes,
transferred in memory and executed from memfd on Alpha, without replacing its
artifacts. No source rebuild is necessary.

## Static instruction evidence

Receipt ELFs have no usable symbol/debug map: `nm -n` reports no symbols;
`addr2line` cannot recover source lines. `objdump -d` alone prints no code for
these sectionless images. Decode the executable segment as a binary, rather
than counting ELF headers or data as instructions:

```sh
readelf -h -l build/wasm-tier/Records.native
nm -n build/wasm-tier/Records.native
objdump -D -b binary -m i386:x86-64 -Mintel \
  --start-address=28238 --stop-address=65546 build/wasm-tier/Records.native
```

Coordinates below are **file offsets**, not virtual PCs; the mapped ELF base is
0x400000. Disassemble from a known instruction boundary (text starts at 0x1000).
Counts exclude objdump byte-continuation lines. Loop envelopes include alternate
branch arms, failure-only trap calls and cold capacity-growth arms: these are
static code counts, never instructions executed per iteration or time shares.
`rbp` loads/stores include aggregate object traffic and ABI saves; they cannot
all be called allocator spills. No `mov r,r` self-move occurs in these envelopes.
Register transfers listed below are candidates for coalescing, not proof that
every transfer is redundant.

| Native envelope (inclusive) | Instructions | Pointer resolves (`shr 0x2c`) | Fuel sites | rbp loads/stores | Register movs | Successful runtime calls |
| --- | ---: | ---: | ---: | --- | ---: | --- |
| Records update 0x6e4e–0x10005 | 5412 | 0 | 1 | 2602/2612 | 22 | 0 |
| Records initialization 0x33b9–0x5209 | 1143 | 0 | 1 | 1074 rbp references total | 14 | 0 |
| Vectors push 0x1189–0x1584 | 250 | 3 | 1 | 27/25 | 38 | 1 conditional resize |
| Vectors sum 0x1589–0x1671 | 61 | 1 | 1 | 2/0 | 15 | 0 |
| Sieve mark 0x1695f–0x16a4a | 61 | 1 | 1 | 0/0 | 15 | 0 |
| Hash inner envelope 0x115d–0x12a4 | 83 | 1 | 1 | 0/0 | 22 | 0 |
| Printing line 0x10f0–0x138d | 178 | 3 | 1 | 2/2 | 50 | 6 output calls |
| Calls wrapper 0x103d–0x1080 | 18 | 0 | 1 | 0/0 | 6 | 0 |

Records has 5226 rbp-relative instructions in that envelope, including address
calculations beyond the loads/stores. Failure-only trap calls number 13 there,
13 in Vectors push, 6 in Vectors sum, 7 in Sieve mark, 7 in Hash and 17 in
Printing. Counting these as runtime work on successful runs would be wrong.
Whole-image resolve counts, including cold/runtime code: Records 0, Vectors 8,
Sieve 3, Hash 1, Calls 0, Printing 3. The single `rep movsb` per image is in a
shared helper, not evidence that Records aggregate copies already use it.

### Records: whole-array value copies, not allocation or checked pointers

The source updates 64 particles for 200 steps. Each particle is 32 bytes; the
array is 2048 bytes. At 0x6e9e the native code extracts four words into a new
record object. At 0x6eda it copies that record again before changing x. At
0x6f19 it starts copying **the entire 2048-byte array**, word by word through
r11, into a separate stack object before inserting the changed record. Each
2048-byte `CopyMemory` expands into 256 loads plus 256 stores. Loop-edge
aggregate transfers add more such sequences. Both y updates and conditional
velocity changes repeat this pattern; branch arms are included in the table.

The mechanism is explicit in `cmd/compiler_x86_64.zi`: `AggregateInsert`
unconditionally calls `CopyMemory(destination, aggregate, size)` before changing
the member; `CopyMemory` emits a load/store pair per eight bytes. In
`cmd/compiler_regalloc.zi`, `EmitAggregateMove` skips copies only if source and
destination already name the same stack object. Scalar register allocation
alone cannot make a 2048-byte value fit into scalar registers.

C's hot particle update envelope 0x1860–0x18a3 is 19 instructions, with eight
memory operands (including an alignment nop); its generated source updates
fields directly. Wasm has the missing mechanism already:
`cmd/zx2wasm/zx_wasm_lower.c::coalesce_aggregates` shares storage for
non-overlapping aggregate lifetimes and insert results; `plan_views` reads
extracted members in place and elides the insert-back when justified. That
explains a credible structural route toward its .051 ms result. Allocation,
free and per-field PointerResolve are ruled out on the native hot path.

### Vectors: dynamic checks, aggregate header traffic and growth

The source pushes 20,000 elements, reads 20,000, then frees 20 vectors.
A successful push may grow storage through the resize veneer at 0x1af0; the
linear envelope contains the growth arm, not one resize on every push. There
are three such call sites across the outer kernel envelope, including cleanup.
Resolve/check machinery is substantial even when capacity suffices; the sum
loop has one complete resolve per element and no runtime call. The 27 loads
and 25 stores in the push envelope also show Vec header and saved-value traffic.
Do not collapse these into a claim that allocation dominates without samples.
C's sum loop is six instructions at 0x1a00–0x1a17, one memory operand; it calls
realloc on growth and free on cleanup. Wasm's range/check elimination and
aggregate storage reuse avoid part of the native work. Dynamic vector tokens
may change after resize: Tier-0 facts cannot be carried across it unchecked.

### Sieve and Hash: check and instruction-selection overhead

Sieve's inner marking envelope has one resolve, one fuel decrement and no rbp
spill traffic. C's marking envelope is five instructions at 0x1800–0x1811;
there is a direct byte store. Native also emits a large unrolled initialization
(the image is 94,336 bytes), so zeroing and code size deserve separate attention.

Hash similarly has one resolve and one fuel check inside its byte loop, zero
rbp spills and no successful calls. C's byte loop is six instructions at
0x1920–0x1931. The native 83-instruction envelope includes loop-control arms;
C's xor/multiply dependence still limits the gain even after removing checks.
The global text token and loop index range are strong candidates for the
verifier-backed facts in trust-tiers Step 1. Compare/boolean materialization
(`cmp; setcc; movzx; test; branch`) is another avoidable cost where flags can
flow directly to a branch.

### Calls: folded workload, not recursion evidence

Native wrapper instruction 0x106a loads immediate 0x1a6d (6765); 32 iterations
sum that constant. The kernel at 0x10c4 returns the same immediate. A recursive
Fib body remains in the image at 0x10cb, but the timed path never calls it.
Its two recursive call sites, fuel and stack checks are therefore irrelevant
to the reported .0064 ms. Wasm's .000146 ms also requires workload inspection
before interpreting it as call throughput. C still has a call-containing Fib
implementation. Use runtime-varying input before evaluating a call-ABI fix.

### Printing: syscall granularity before register tuning

One source line becomes six successful output calls: three string-write calls
at 0x13c5, two integer-format calls at 0x142f, one bool-format call at 0x1488.
Each ultimately reaches the write veneer, so 100 lines means at least 600
write operations per kernel (partial writes/EINTR can add more). There are
three constant-text pointer resolves per line. C's 14-instruction loop at
0x1ee0–0x1f11 makes one printf call per line with libc buffering. Wasm's host
output path likewise differs from native's direct syscalls. The time ratio
therefore includes output buffering policy; it is not solely compiler quality.

## SIGPROF sampling (local diagnostic test on Omega)

The Alpha queued sampler was cancelled with SIGTERM while still queued; its
local SSH wrapper finished with status 143. It did not run later as a duplicate.
The coordinator then selected Omega for diagnostic sampling only. Alpha remains
the timing baseline; these are not cross-host speed measurements. No build was
needed: the sampler used existing receipt-matching ELF bytes from
`build/wasm-tier/`, executed via memfd. No shared build outputs were changed.

Omega: AMD Ryzen 9 9950X, physical host (`systemd-detect-virt`: none), load
11.72/11.98/9.00 both before and after. Invocation:
`env -u DISPLAY -u WAYLAND_DISPLAY build-queue -- python3 -u -c SAMPLER`.
The sampler forks a traced child, injects setitimer(ITIMER_PROF, 1 ms) at entry
using ptrace, restores the original instruction/registers, and counts RIP on
SIGPROF stops while suppressing delivery. Each process executes the original
32-call benchmark wrapper; stdout is /dev/null. Cap: 150 samples or 30 runs per
kernel. It completed with status 0; all 162 child processes exited 42.

| Kernel | Runs | Samples | Attribution |
| --- | ---: | ---: | --- |
| Records | 12 | 150 | 150/150 in 0x406e4e–0x410005 aggregate update envelope |
| Vectors | 30 | 62 | 42 resize veneer; 14 push envelope; 3 sum envelope; 3 outer growth/control |
| Sieve | 30 | 0 | process shorter than effective profiling tick; no sampled attribution |
| Hash | 30 | 0 | same resolution limitation |
| Calls | 30 | 0 | constant-folded path and startup; no sampled attribution |
| Printing | 30 | 0 | short process; no sampled attribution |

Records samples cover the word-copy sequences throughout the update region,
for example PCs 0x406f04, 0x407d8d (2), 0x408c79 (2), 0x40f021 (2) and
0x40fdc7 (2). There are no allocation or resolve sites in that region. Samples
confirm the region dominates; they do not mean every instruction there is a copy.

Vectors has **42/62 (67.7%)** samples in runtime resize, 14/62 (22.6%) in the
push envelope, 3/62 (4.8%) in summation, and 3/62 (4.8%) in outer growth/control.
Full raw Vectors histogram (virtual PC: count):

```text
401bd6:23 4019b7:1 401b8f:9 4015b0:1 40165a:1 4019c2:2
4011bd:1 401479:3 401574:1 401456:1 401b8a:7 40156c:1
401b45:2 40154e:1 401513:1 401191:1 401607:1 401b96:1
4014f1:1 401516:2 4011d7:1
```

At 0x401b88 is mmap; samples at 0x401b8a and 0x401b8f follow it.
At 0x401bd4 is munmap; 23 samples at 0x401bd6 follow it. A further two at
0x401b45 follow the free-only munmap. Timer signals charged during a syscall
can be delivered at its return; do not attribute those ticks to the following
`test` or metadata store. **The resize implementation's mmap/copy/munmap policy
is the leading sampled Vectors cost**, not an allocation on every push.
`cmd/compiler_machine_elf.zi::WriteRuntime` contains this veneer;
`cmd/compiler_x86_64.zi::Resize` calls it and then updates checked-pointer slots.

A 1 ms requested timer has coarser effective scheduling granularity here.
Zero samples in four kernels is not zero runtime cost. Their analysis remains
static, with the Alpha receipt providing elapsed times. The bounded sampler
is diagnostic, perturbs execution, and is too sparse for fine differences or
cross-host extrapolation. Future same-host A/B runs should budget at least
±3–10% noise (more under contention), retain raw samples, and seek repeated
confidence before accepting small gains. No speedup was measured by this task.

## Ranked fixes and acceptance contracts

Expected gains below are hypotheses for the baseline Alpha workload, not
measured promises. Do not multiply them blindly: several remove the same work.
All implementation assignments use Claude/Codex. Each needs verified machine
IR, named diagnostics, parity and hostile counter-tests; no silent unchecked
native shortcut is acceptable.

1. **Aggregate storage coalescing and views (L, Claude/Codex high).**
   Files/functions: `cmd/compiler_regalloc.zi::EmitAggregateMove`, stack-object
   placement and liveness; `cmd/compiler_machine_lower.zi::LowerMemory`;
   `cmd/compiler_x86_64.zi::AggregateInsert`, `AggregateExtract`, `CopyMemory`.
   Reuse the Wasm `coalesce_aggregates`/`plan_views` analysis pattern, with
   verifier-visible ownership and liveness. Share slots only after the old value
   dies; preserve snapshots, aliasing, branch-edge parallel copies and cycles.
   Expected: 20–80× Records initially; 1.05–1.3× Vectors overall if header
   traffic shrinks; sampled allocator work limits the ceiling.
   Ziran test: extend retained regalloc/aggregate fixtures with a 64-record
   nested update, an old-array snapshot used after insertion, loop parameters,
   branches and copy cycles. Done: checksum 68788; no 2048-byte copy in the
   common x/y update path; old snapshots unchanged; static byte-traffic counters
   and allocation verifier agree; Records reaches <=15× C before further tuning.

2. **Reuse allocator storage instead of mmap/copy/munmap per growth (M/L,
   Claude/Codex high).** Files/functions:
   `cmd/compiler_machine_elf.zi::WriteRuntime` resize veneer;
   `cmd/compiler_x86_64.zi::Resize/ResizeSlots`; preserve the existing typed
   resize contract and checked slot/version rules. Introduce bounded reusable
   allocation storage or a capacity-aware native allocator. Do not assume that
   an address remaining unchanged keeps old aliases valid: successful resize
   must obey the existing invalidation semantics. Preserve zeroing, contents,
   overflow diagnostics and failure leaving the old allocation live.
   Expected: 1.5–3× Vectors, not an unlimited gain; eliminating all sampled
   resize cost would cap improvement near 3.1× on this sample distribution.
   Ziran test: extend retained native resize cases with repeated growth/shrink,
   freed/reused storage, stale aliases, alignment, allocation failure and content
   preservation; run the Vectors checksum 94905000. Done: materially fewer
   mmap/munmap calls under repeated growth, stale aliases trap, allocator state
   remains bounded, full native/parity suites pass and Alpha improves >=1.5×.

3. **Proven checks and stable-loop resolve reuse (M/L, Claude/Codex high).**
   Implement trust-tiers Step 1 in `cmd/compiler_opt.zi`,
   `cmd/compiler_machine_ir.zi`, `cmd/compiler_machine_verify.zi` and
   `cmd/compiler_machine_lower.zi::LowerResolve/LowerMemory`; consume facts in
   `cmd/compiler_x86_64.zi::PointerResolve`. Constant global text and bounded
   fixed-array indices can lose checks only with slot/version/liveness/extent/
   writability obligations proved. For vector summation, hoist only what remains
   valid until the next resize/free/unknown host call; this is beyond blanket
   constant-token elision. Expected: 1.5–3× Hash/Sieve, 1.2–2× Vectors after
   allocator work, little
   direct Records gain. Ziran test: proven constant/range cases paired with
   stale alias, freed/resized storage, read-only store and out-of-range indices.
   Done: expected resolve counters fall; hostile twins still trap; no surviving
   unchecked address without the machine verifier's fact.

4. **Coalesce one formatted line before output (M, Claude/Codex high).**
   Files: `cmd/compiler_machine_lower.zi` service lowering and native console
   emission in `cmd/compiler_machine_elf.zi::WriteRuntime` (with any required
   instruction encoding in `cmd/compiler_x86_64.zi`). Format a bounded line
   into owned scratch and
   send it through the existing partial-write/EINTR-safe veneer. Preserve exact
   text, integer widths, bools and trap behavior; do not buffer arbitrary output
   indefinitely. Expected: 3–8× Printing, negligible compute-kernel gains.
   Ziran test: retained console cases with the 100-line fixture, signed extrema,
   long fragments and injected partial writes/EINTR. Done: byte-identical
   stdout, common-case write count <=100 per kernel, <=4× C on Alpha.

5. **Copy/zero code-size selection (M, Claude/Codex high).**
   `cmd/compiler_x86_64.zi::CopyMemory/ZeroMemory`: after removing unnecessary
   copies, choose bounded inline or loop/rep sequences for the remaining large
   transfers; obey register clobbers, overlap semantics and the machine verifier.
   A compact copy alone still moves unnecessary kilobytes in Records, so rank
   it after storage reuse. Expected 1.1–2× affected kernels plus smaller images.
   Ziran test: aligned/unaligned copies, sizes around 1/2/4/8 and threshold,
   exact self-copy, zero-length and any supported overlap. Done: same bytes,
   reserved-register invariants, reduced image size and no Alpha regression.

6. **Scalar transfer and compare/branch selection (M, Claude/Codex high).**
   `cmd/compiler_regalloc.zi::BuildIntervals/SplitAt/Assign/EmitTransfer` and
   `cmd/compiler_x86_64.zi` compare/branch encoding, coordinated with
   `cmd/compiler_machine_lower.zi::LowerControl`. Prefer direct flags-to-branch
   where flags liveness proves validity; coalesce scalar moves without hiding
   fixed-register constraints. Expected 1.1–1.5× compute kernels, less Printing.
   Ziran test: flags live across intervening arithmetic, signed/unsigned boundary
   branches, call clobbers, pressure and copy cycles. Done: allocated verification
   passes; instruction counts fall; no check or branch changes semantics.
   Fuel remains enforced; batching/hoisting fuel is a separate semantic change,
   not authorized merely by a trusted pointer proof.

For compiler implementations run relevant retained Ziran cases plus
`env -u DISPLAY -u WAYLAND_DISPLAY build-queue -- make check` and
`env -u DISPLAY -u WAYLAND_DISPLAY build-queue -- make check-bootstrap`, one
queue job at a time. Re-run `build-queue -- python3 bench/wasm_tier.py --samples
11 --output <approved-result-path>` on physical Alpha with matching source,
record actual load, raw samples, exclusions and hashes. Tests are proposed here;
none of these compiler fixes has been implemented or tested by this task.

## Realistic targets after fixes 1–3

| Kernel | Baseline native/C | Target native/C | Reason/limit |
| --- | ---: | --- | --- |
| Records | 297.19 | 4–15 | remove whole-array traffic; remaining scalar/branch costs |
| Vectors | 22.85 | 3–8 | coalesce header storage, reuse stable facts; keep growth checks |
| Sieve | 5.50 | 2–4 | prove bounded accesses; initialization/fuel still present |
| Hash | 3.67 | 1.5–2.5 | constant global and range facts; serial hash dependence |
| Printing | 12.96 | 10–13 | top three retain per-fragment syscalls; fix 4 targets 2–4× C |
| Calls | .56* | no recursion target | fixed-input constant folding; repair experiment first |

These ranges require all stated proof coverage, especially stable vector
facts. A Step-1 implementation limited to constant pointers will not reach the
Vectors range by itself. Call throughput needs a new runtime-input experiment,
not a target inferred from this folded fixture.

## Status: fixes 1 and 5 for Records (2026-11-15, Omega)

Implemented in `cmd/compiler_machine_aggregate.zi` (planning) and
`cmd/compiler_machine_lower.zi`, `cmd/compiler_x86_64.zi`,
`cmd/compiler_x86_bytes.zi`. Owner direction for fix 1: speed first, keep only
the guardrails correctness needs, so no new verifier obligations were added.

- Storage: aggregate values with non-overlapping lifetimes share one frame
  object (the zx2wasm `coalesce_aggregates` rule), so an insert whose source
  dies writes only its member and block parameters reuse their arguments'
  object. Real copy cycles keep a temporary, now of the aggregate's own size.
- Views (x86-64): an extract read only by extracts in its block is read in
  place through an address computed by `StackAddress` and
  `FieldAddress`/`ElementAddress`; a dominating view with the same storage
  and member shares that address. A changed view is stored in place and its
  put-back emits nothing. Dynamic indices use the shared LIR facts' proven
  bound (no check) or keep one check per address.
- Copies and zeroing of 64 bytes or more run a 16-byte `movups` loop with an
  indexed operand; Records keeps no copy that large, Sieve's image falls from
  94,336 to 8,320 bytes.

Receipts: `bench/results/wasm-tier-omega-2026-11-15-records-before.json`
(revision 7ac6643d, load 16.2/16.8) and `...-records-after.json` (revision
c0efdfd5 plus this change, load 11.5/11.9), 11 samples, both on Omega with
other lanes' uncommitted work in the tree.

| Records | C µs | zx engine µs | native µs | Wasm µs | native image |
| --- | ---: | ---: | ---: | ---: | ---: |
| before | 9.38 | 1858.32 | 1951.25 | 18.48 | 69,760 B |
| after | 8.66 | 29.20 | 32.18 | 17.00 | 12,416 B |

Direct repeated runs after the receipt measured native 25.4 µs (32 calls per
process, start included) and the zx engine 28.9 µs, about 2.9× and 3.4× C.
The update loop is now C-shaped for memory: one proven address per particle,
then load/add/store of x and y. The rest of the gap to Wasm is scalar code
outside fix 1: each compare becomes `cmp; setcc; movzx; test; jcc`, constants
are moved into registers before use, jumps to the next block remain, and the
per-iteration fuel decrement in memory stays enforced. Those belong to fix 6.

## Status: fix 6, step 1 — compare, branch and immediate selection (2026-11-15, Omega)

`cmd/compiler_machine_select.zi` plans x86-64 selection per LIR function
before lowering: constants fitting a sign-extended 32-bit field become the
right operand of integer compares and add/subtract/and/or/xor/multiply (a
commutative left constant swaps places) and are not loaded when every use
folds; a compare whose only use is the next branch branches on its flags; a
jump into a block that only branches on its one parameter does that branch
itself (constant: straight to the chosen edge; compare: on its flags), and a
block every edge skips is lowered as unreachable. The encoder elides a jump
when only lone jumps to the target lie between, and scales 1/2/4/8-byte
element indices in the address instead of multiplying. MachineVerify now
also rejects a branch that tests neither an operand nor flags.

Receipt `bench/results/wasm-tier-omega-2026-11-15-scalar-select.json` (load
16.0/15.6): native Sieve 65.9 → 57.8 µs, Records 32.2 → 20.1 µs, Hash 38.9
→ 32.1 µs; zx engine Sieve 66.2 → 51.3, Records 29.2 → 23.1, Hash 40.4 → 31.3
(C 10.8, 8.9, 22.4). Sieve's inner loop is 12 instructions (C: 5). Measured
with fuel checks removed in a private build, fuel costs about 25% of Sieve and
22% of Records: the per-iteration `sub qword [rip+fuel], 1` is a store-to-load
chain. Next: keep a looping function's budget in a register (exact, synced
at calls, returns and traps), then the direct-pointer store sequence, store
immediates and loop-invariant address work in Hash.

### Fix 6, step 2 findings — fuel and trap layout (not landed)

A register step budget (r15 in looping x86-64 functions: loaded after the
prologue, stored before internal/host calls, returns and every trap,
reloaded after calls; patch kept outside the tree) gave no speedup alone on
Omega: Sieve 52/53 µs either way. The cost is not the memory chain but the
`jae` every inline check takes to skip its trap sequence: each loop pays two
taken branches (that and the back edge). Private experiment, 3×15 runs:

| Kernel | inline trap | register fuel + one out-of-line fuel stub | no fuel at all |
| --- | ---: | ---: | ---: |
| Sieve | 50.5–53.7 | 39.4–42.3 | 37.5–39.8 |
| Hash | 26.6–28.9 | 27.1–28.3 | 25.9–27.6 |
| Records | 17.9–19.2 | 17.3–18.9 | 16.8–17.3 |

So the next landing is: fuel checks that fall through (`sub r15, n; jb stub`)
with the stub placed just before the loop header's label and covered by the
fuel instruction's own range, so trap reports keep their instruction and
line without host changes; a predecessor that would fall into the header
jumps over the stub instead. The first attempt in the shared tree made
Wurm's native SessionOpen trap with code 1 (a jump landing inside a trap
sequence past its `mov edi`) and was reverted; it must pass that path and the
full suite before landing.

Remaining per kernel after that, from objdump of the current build:
- Sieve inner loop (12 instructions, C 5): the direct frame store goes
  `lea token; mov r9d, 1; mov r11d, tok; mov rsi, base; add rsi, r11; mov [rsi],
  r9b`. `mov byte [base + j], 1` needs PointerDirect to fold a proven element
  index into the address and Store to take an immediate value, both of which
  MachineVerify's CheckedMemory must then accept.
- Hash byte loop: the text token, a zero-index element, the global address
  and the 64-bit prime are recomputed every iteration (loop-invariant code
  motion), plus a dead count constant the proven element no longer uses.
- Records per particle (~33 instructions, C 19): reloads of x and y just
  stored (the changed view's value can be forwarded), jumps through
  forwarding blocks the layout puts out of order, and outer-loop copy moves
  that tie hints would remove.

## Status: fix 2 for Vectors (2026-11-15, Omega)

Implemented in the new `cmd/compiler_x86_runtime.zi` (the ELF's runtime
routines, emitted with the x86 encoder; the resize part of the old byte blob
is gone), `cmd/compiler_machine_elf.zi` (layout: the routines a program calls,
the heap state after the frame arena) and `cmd/compiler_x86_64.zi`
(`PointerResolve`).

- Resize: blocks carry a 32-byte header (total, free-list link, count,
  element size). Totals up to 1 MiB are power-of-two classes carved from
  2 MiB chunks and recycled through one free list per class; a block that
  keeps its class stays in place. Larger blocks are their own mappings and
  grow or shrink with `mremap`. Bytes past the kept elements read zero: chunk
  and mapping bytes are fresh, recycled ones are cleared (`New(T)` relies on
  it). Refusal (overflow, no memory) still leaves the old block live, and the
  slot sequence in `Resize`/`ResizeSlots` is unchanged, so every successful
  resize retires the old token even when the address stays.
- Resolve: the slot bound check is gone (the ELF and the in-process loader map
  all `MachinePointerCapacity + 1` entries and `CreateSlot` never makes the
  last one live); version, liveness and writability are one xor and test.
  This also speeds the zx engine, which runs the same code.

Receipts: `bench/results/wasm-tier-omega-2026-11-15-speed-before.json` (HEAD
c9b088cb, load 8.94/12.03) and `...-speed-vectors.json` (this change, load
8.83/11.70), 11 samples on Omega. µs per call:

| Vectors | C | zx engine | native | Wasm |
| --- | ---: | ---: | ---: | ---: |
| before | 13.14 | 81.43 | 379.87 | 27.71 |
| after | 13.72 | 64.46 | 69.27 | 28.64 |

Startup-free (1024 calls per process) the kernel went from 353 to 58 µs. The
1.5× C target is not met. SIGPROF samples of the 58 µs build: 49% push loop,
37% sum loop, 12% resize and the runtime. Per element the push path still runs
about 55 instructions against C's five: the checked resolve of the element
address (19), two `cmp; setcc; movzx; test; jcc` compare materializations
(fix 6, the records-aggregate lane), the index check `count < count + 1`, the
Vec header kept in its frame slot (load data and count, store count back) and
the fuel decrement. The sum loop resolves every element too. What remains in
this lane's reach: resolving a loop-invariant vector view once before a loop
with no call or resize (needs a verifier-visible view instruction) and
dropping fuel sites when an ELF's budget is unbounded (measured 1-12% per
kernel by patching them out).

## Status: fix 4 for Printing (2026-11-15, Omega)

`cmd/compiler_x86_runtime.zi` now also emits the console: `write_all`, the
integer and bool formatters, `stdout_bytes` for the float formatter (the one
blob left, minus its private write loop) and `Exit`, which `_start` and the
trap stub both jump to. Standard output collects in an 8 KiB buffer, written
when full, after each line when it is a terminal (one `TCGETS` on the first
write), before any write to another descriptor, before a trap's exit and at
exit. A failed write still exits 125 without flushing again. 100 lines are one
write; `tests/print.sh` checks the bytes, an injected EINTR and short write,
the write count, terminal output and output before a trap.

Receipt `bench/results/wasm-tier-omega-2026-11-15-speed-after.json` (this
change on top of fix 2, load 8.53/11.55), µs per call:

| Printing | C | zx engine | native | Wasm |
| --- | ---: | ---: | ---: | ---: |
| before | 3.86 | 24.38 | 41.72 | 2.89 |
| after | 4.35 | 25.39 | 4.52 | 2.67 |

Startup-free the kernel is 1.57 µs (34 before), 2.5× faster than C. The bench
row includes the native process start its Calls row shows (3.9-5.5 µs per
call over 32 calls), which is why native and C meet there. The zx engine is
unchanged: its prints already reach libc's buffered stdout, and its cost is
one generic host call per fragment in `cmd/zir/zir_host_native.c`
(`NativeHostInvoke` allocates the argument array, then `snprintf` and
`fputs`), which belongs to the loader lane.

## Document verification and publication

Only this document is in scope for writes. Static inspection used receipt hash
comparison, readelf/nm, bounded objdump output and the named source functions;
no compiler rebuild or new timing result is claimed. SIGPROF diagnostic sampling completed as recorded above; four kernels remain
below timer resolution. No compiler change needs a compiler regression suite
for this document. The coordinator accepted the completed analysis and requested publication of
only this document using a private index; preserve the shared checkout and index.


Local document validation ran through `build-queue -- python3 -` and passed:
six ELF hashes and six native/C ratios match the Alpha receipt, Vectors sample
categories sum to 62, ranked contracts and Markdown fences are complete, and
`git diff --check -- plans/2026-11-15-native-speed.md` reports no whitespace
errors. The first validation assertion compared `.56` against the string
`0.56`; numeric comparison corrected that validation-only formatting mismatch.
No compiler or runtime test suite was rerun for this analysis-only change.

## Reproduce the diagnostic sampler

From the canonical Ziran checkout, using the hash-matching existing artifacts:

```sh
env -u DISPLAY -u WAYLAND_DISPLAY build-queue -- python3 -u - <<'PY'
import ctypes as C,os,signal,struct,sys,zipfile,io,collections,time,json,hashlib
lib=C.CDLL(None,use_errno=True);lib.ptrace.restype=C.c_long
class Reg(C.Structure):_fields_=[(n,C.c_ulonglong) for n in 'r15 r14 r13 r12 rbp rbx r11 r10 r9 r8 rax rcx rdx rsi rdi orig_rax rip cs eflags rsp ss fs_base gs_base ds es fs gs'.split()]
def pt(req,pid,a=0,d=0):
 r=lib.ptrace(req,pid,C.c_void_p(a),C.cast(d,C.c_void_p) if not isinstance(d,int) else C.c_void_p(d))
 if r==-1 and C.get_errno():raise OSError(C.get_errno(),str(req))
 return r

print('HOST',os.uname().nodename,'VIRT',os.popen('systemd-detect-virt').read().strip(),'LOAD',open('/proc/loadavg').read().strip(),flush=True)
for name in ['Records','Vectors','Sieve','Hash','Calls','Printing']:
 data=open('build/wasm-tier/'+name+'.native','rb').read();fd=os.memfd_create(name,0);os.write(fd,data); os.fchmod(fd,0o700)
 hist=collections.Counter(); exits=collections.Counter();start=time.monotonic();runs=0
 while sum(hist.values())<150 and runs<30:
  pid=os.fork()
  if pid==0:
   out=os.open('/dev/null',os.O_WRONLY);os.dup2(out,1);os.dup2(out,2);pt(0,0);os.execv('/proc/self/fd/'+str(fd),[name])
  os.waitpid(pid,0);r=Reg();pt(12,pid,0,C.byref(r));saved=bytes(r);rip=r.rip
  C.set_errno(0);old=pt(1,pid,rip)
  pt(4,pid,rip,(old & ~65535)|0x050f)
  timer=r.rsp-128
  for i,val in enumerate([0,1000,0,1000]):pt(5,pid,timer+i*8,val)
  r.rax=38;r.rdi=2;r.rsi=timer;pt(13,pid,0,C.byref(r));pt(9,pid);os.waitpid(pid,0)
  after=Reg();pt(12,pid,0,C.byref(after))
  if after.rax!=0:raise RuntimeError('setitimer '+str(after.rax))
  pt(4,pid,rip,old);r=Reg.from_buffer_copy(saved);pt(13,pid,0,C.byref(r));pt(7,pid)
  while True:
   _,status=os.waitpid(pid,0)
   if os.WIFEXITED(status):exits[os.WEXITSTATUS(status)]+=1;break
   if os.WIFSIGNALED(status):raise RuntimeError('signal '+str(os.WTERMSIG(status)))
   sig=os.WSTOPSIG(status)
   if sig==signal.SIGPROF:
    r=Reg();pt(12,pid,0,C.byref(r));hist[r.rip]+=1;pt(7,pid)
   else:pt(7,pid,0,sig)
  runs+=1
 print(json.dumps(dict(kernel=name,sha256=hashlib.sha256(data).hexdigest(),runs=runs,exits=dict(exits),samples=sum(hist.values()),seconds=round(time.monotonic()-start,2),ips={hex(k):v for k,v in hist.items()})),flush=True)
 os.close(fd)
print('LOAD_END',open('/proc/loadavg').read().strip(),flush=True)
PY
```
