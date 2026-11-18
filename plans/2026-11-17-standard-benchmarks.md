# Standard benchmarks: native against C

Owner goal: ZX native at or below `cc -O2` on `bench/standard/`. All timing
is on Alpha (i7-8700, jump-erratum microcode) with `bench/bench.zi
standard-ab`; profiles use `standard-profile` (SIGPROF, hot native code
disassembled). The native lane owns fib, mandelbrot and the register, call
and float side; this plan covers binary-trees, fasta and the rest.

## Where native still loses, ranked

Numbers are µs from the baseline matched A/B (C / native). Binary-trees now
runs at 46,346 against C's 98,101: the in-process heap fast path replaced
the host call, `calloc` and block list on every `New` and `free`.

1. **fasta** (111,974 / 135,515).
   - Loop-invariant `if phase == 1 { p = ... }` overrides inside the inner
     loop: gcc unswitches them into a second probability table. Native
     tests `phase` and four indices and rematerializes the float constants
     with `movabs` + `movq` on every iteration. Needs loop unswitching (or
     table folding) in the LIR optimizer.
   - One step check (`sub $1,%r15; jb`) per inner-loop iteration; gcc has
     none. A bounded loop (`index < 14`) could check its whole budget once
     at entry and charge the real count at exit, with a checked copy when
     the budget is short.
   - gcc unrolls the inner loop by two.
   - `hash*31` is `imul $0x1f` (3 cycles on the hash chain); gcc emits
     `shl $5; sub`.
   - `i % 287` with `i >= 0` takes the signed multiply-high and its sign
     fix-up; gcc's range analysis uses the unsigned form.
2. **matmul** (3,982 / 10,937). gcc's inner loop is six instructions:
   `mov (%rsi,%rax,8),%rcx; imul %rdi,%rcx; add %rcx,(%rdx,%rax,8); add
   $1,%rax; cmp %rax,%rbx; jg`. Native's loop:
   - Spills four values it never uses inside the loop on every iteration
     and closes through a block that reloads them (register allocation;
     evidence sent to the native lane).
   - Reloads `a[i*n+k]` each iteration: LICM keeps the load because its
     memory effect follows the store to `c`, and nothing proves the
     vectors' storage is disjoint. gcc hoists it to the `k` loop.
   - One step check per iteration.
3. **spectral-norm** (10,483 / 21,745). gcc's inner loop is about 16
   instructions with `1.0` kept in a register and `/ 2` as a bare `sar`.
   Native still:
   - Rebuilds `1.0` every iteration with `movabs` + `movq`; LICM leaves
     constants at their uses, and float constants have no register-free form
     (a RIP-relative `movsd` from the constant pool would be one load).
   - The machine selector now recognizes `(i+j)*(i+j+1)` as even, including
     after integer wrap. Signed division by two emits one arithmetic shift
     because truncation and the shift agree for every even value.
   - Shuffles four registers and jumps on the back edge (register
     allocation, native lane).
   - Checks the step limit every iteration.
4. **nqueens** (5,324 / 7,568). gcc's recursion keeps `cols`, `left`,
   `right` and the running total in callee-saved registers and computes
   `(left | bit) << 1` as `add %rdx,%rdx`. Native still spills two values
   across the recursive call (register allocation, native lane) and stores
   and reloads the step counter around every call.
5. **n-body** (32,288 / 64,742),
   **fannkuch-redux** (16,329 / 20,689), **quicksort** (12,780 / 14,372):
   not yet profiled. String-hash is at C (5,607 / 5,618).

The expanded Alpha cohort measured **k-nucleotide** at 1,971 µs in C,
71,099 µs in native and 46,958 µs in Wasm. The native profile puts 999 of
1,062 samples in a 128 KiB copy loop. The loop condition loaded
`KNucleotide`'s entire `counts: [16384]s64` before folding `.count` to a
constant. The body already loaded just the indexed element.

## Exact division by two on Alpha

The candidate and baseline used the same frozen ZX bundle and the C digest
for every invocation, with 11 matched rounds pinned to CPU 3.
[Raw samples and exclusions](../bench/results/standard/native-exact-half-alpha.json).

| Workload | C (ms) | Native before (ms) | Native after (ms) |
| --- | ---: | ---: | ---: |
| spectral-norm | 10.457 | 21.730 | 16.475 |
| fasta | 110.901 | 128.681 | 128.706 |
| nqueens | 5.274 | 7.464 | 7.464 |
| quicksort | 12.688 | 14.151 | 14.361 |

## Tail calls through a shared return block

The x86-64 trial converted a call followed by a jump to a shared return
into one tail jump. Disassembly confirmed it kept the other recursive call
in the same function. In 17 matched rounds on Alpha with exact-half in both
runners, quicksort took 13.916 ms before and 13.812 ms after, against 12.224 ms
in C. The median paired change was −0.29%. N-queens took 7.273 ms before and
7.456 ms after, against 5.150 ms in C. The median paired change was +0.09%.
Both matched the C digest in every round. The emitter trial was removed.

## Multiplication by 31 on Alpha

Native x86-64 now uses a shift and subtract for a constant factor of 31.
FASTA improved in 10 of 11 focused rounds and all seven rounds across the
original suite. The median paired gains were 0.79% and 0.70%, respectively.
The full run measured 107.192 ms in C, 124.622 ms before and 123.544 ms after.
All samples matched the C digest. The quicksort changes varied across runs.
[Raw samples](../bench/results/standard/native-multiply-31-alpha.json).

## Boolean argument specialization on Alpha

The general inlining threshold trial removed the four `Multiply` calls
and increased their caller's native size from 2,741 to 4,271 bytes.
Spectral-norm's paired median changed by +0.19% and the trial was removed.

The optimizer now emits separate callees for literal Boolean arguments
that control loop branches. The unchanged spectral-norm workload calls
two smaller copies of `Multiply`. Calls with a runtime Boolean and
indirect calls retain the original body. Each runner invocation matched
the C digest. [Raw samples](../bench/results/standard/native-bool-specialization-alpha.json).

| Cohort | C (ms) | Native before (ms) | Native after (ms) | Median paired change |
| --- | ---: | ---: | ---: | ---: |
| Four workloads, 11 rounds | 10.116 | 15.845 | 12.744 | −19.89% |
| Original 11 workloads, 7 rounds | 9.995 | 15.891 | 12.762 | −19.31% |

Spectral-norm improved in all 18 rounds. In the seven-round cohort the
other paired medians ranged from −1.15% to +1.84%. Fib and binary-trees
had the largest increases at +1.84% and +1.46%, respectively.

The first version slowed the Button and Input widgets about 3×. Its copy
`TreeLayerAt$1` folded `committed`, which removed the field read of the
first `trees[slot]` load but left the load, so both backends copied the
whole widget tree before loading it again. A copy is now made only when
folding leaves no record or array load without its use, and DeadPass
drops an unused load that its block repeats at the same address before
any effect. On Alpha, with stdout and result checked in every run, the
fix ran faster than master in all 7 rounds of each widget and target
(µs; [raw samples](../bench/results/standard/bool-guard-alpha.json)):

| Widget | Before specialization | Master | Fixed |
| --- | ---: | ---: | ---: |
| Button native | 4.701 | 14.423 | 4.848 |
| Button Wasm | 2.641 | 8.576 | 2.563 |
| Input native | 6.768 | 18.813 | 6.470 |
| Input Wasm | 3.944 | 10.830 | 3.792 |

Spectral-norm still uses both `Multiply` copies: 12.944 ms before and
12.954 ms after, against 10.076 ms in C, over 7 matched rounds.

## Loop unswitching trial on Alpha

A trial pass ran the smallest loop with a branch on a Boolean computed
before it as two copies, tested once in the preheader. FASTA's `phase == 1`
overrides moved out of its middle loop and all 29 probes matched C, but
FASTA took 138.594 ms against 132.032 ms before (C 110.704 ms) and native
Input took 6.630 µs against 6.415 µs in 7/7 rounds; the other workloads
stayed within noise. FASTA is bound by the `seed` and `hash` modulo
chains and the float divide, not by the branches, and its native
modulo-by-constant sequences already match GCC's. The pass was not kept.
[Raw samples](../bench/results/standard/unswitch-trial-alpha.json).

## Loads in rotated tests and fixed member hoisting on Alpha

Rotation now copies a loop test that loads from memory, such as
quicksort's `while a[i] < pivot`. The latch's copy runs where the header's
test ran, so fuel, loads and traps keep their order. LICM now also hoists
a fixed member read such as a slice's data or count, which spectral-norm
reloaded from its argument slot every iteration. Over 7 matched rounds
of all 29 workloads, both changes together took 0.67% less time by
geometric mean (µs; [raw samples](../bench/results/standard/rotate-licm-alpha.json)):

| Workload | C | Before | After |
| --- | ---: | ---: | ---: |
| json-roundtrip | 3,783 | 7,035 | 6,353 |
| radix-sort | 8,196 | 9,360 | 8,725 |
| heat-grid | 1,421 | 2,273 | 2,123 |
| dijkstra | 4,081 | 9,673 | 9,138 |
| fasta | 107,100 | 128,996 | 123,688 |
| binary-trees | 91,725 | 41,300 | 39,852 |
| spectral-norm | 10,104 | 12,943 | 12,744 |
| fannkuch-redux | 15,865 | 17,975 | 18,610 |

Bytecode compiles to the same LIR in all four trial bundles yet took
6,807, 8,227, 8,248 and 6,957 µs. Each runner given the other bundle
follows the bundle, and its unchanged native code moved by 0x60 bytes:
32-byte boundaries stay put, so this is branch-prediction aliasing in its
compare-chain dispatch. Treat single-workload changes within ±20% on
branchy code as placement until a cross run says otherwise.

Spectral-norm's inner loop is still about 23 instructions in-process
against C's 16. The rest is native: 1.0 is rebuilt with `movabs` and
`movq` every iteration, the back edge is `je exit; add; jmp` rather than
one bottom `jne`, and the inlined `A` and the latch each check fuel.

## Fixed-array count on Alpha

ZX lowering now folds the count of a plain local fixed array before
emitting a load. Receiver expressions that can perform work, such as
calls returning arrays, still evaluate. The k-nucleotide source and
input stayed the same.

| Runner | Before (ms) | After (ms) | Median paired change |
| --- | ---: | ---: | ---: |
| C (`cc -O2`) | 1.948 | 1.948 | |
| Native ZX | 65.623 | 3.216 | −95.14% |
| Wasm | 45.084 | 3.779 | −91.35% |

Both ZX variants improved in all 11 matched k-nucleotide rounds. The
five other workloads had paired changes from −1.85% to +1.85%.
All 29 suite probes matched C in Go, native and Wasm, and every timed
invocation matched the C digest.
[Raw samples and exclusions](../bench/results/standard/fixed-array-count-alpha.json).

## Every in-process native function

- Step metering (`fuel`) is always on for in-process code: one check per
  loop iteration and call, and it turns off the vectorizer and frameless
  leaves.
- Recursive calls carry a stack check and a frame; fib's recursive path
  also takes a `jge` and a `jmp` back per call (native lane).
