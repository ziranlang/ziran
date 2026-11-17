# Standard benchmarks: native against C

Owner goal: ZX native at or below `cc -O2` on `bench/standard/`. All timing
is on Alpha (i7-8700, jump-erratum microcode) with `bench/bench.zi
standard-ab`; profiles use `standard-profile` (SIGPROF, hot native code
disassembled). The native lane owns fib, mandelbrot and the register, call
and float side; this plan covers binary-trees, fasta and the rest.

## Where native still loses, ranked

Numbers are µs from the latest matched A/B (C / native). Binary-trees now
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
3. **spectral-norm** (10,539 / 24,374): `A(i, j)` and the transpose select
   are duplicated into two copies of the loop body that jump back into each
   other; no vectorization under step metering.
4. **nqueens** (5,324 / 7,568). gcc's recursion keeps `cols`, `left`,
   `right` and the running total in callee-saved registers and computes
   `(left | bit) << 1` as `add %rdx,%rdx`. Native still spills two values
   across the recursive call (register allocation, native lane) and stores
   and reloads the step counter around every call.
5. **n-body** (32,288 / 64,742),
   **fannkuch-redux** (16,329 / 20,689), **quicksort** (12,780 / 14,372):
   not yet profiled. String-hash is at C (5,607 / 5,618).

## Every in-process native function

- Step metering (`fuel`) is always on for in-process code: one check per
  loop iteration and call, and it turns off the vectorizer and frameless
  leaves.
- Recursive calls carry a stack check and a frame; fib's recursive path
  also takes a `jge` and a `jmp` back per call (native lane).
