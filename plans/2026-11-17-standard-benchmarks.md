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
1,062 samples in a 128 KiB copy loop. Check whether iteration over
`KNucleotide`'s `counts: [16384]s64` materializes a fresh copy on each pass.

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

## Every in-process native function

- Step metering (`fuel`) is always on for in-process code: one check per
  loop iteration and call, and it turns off the vectorizer and frameless
  leaves.
- Recursive calls carry a stack check and a frame; fib's recursive path
  also takes a `jge` and a `jmp` back per call (native lane).
