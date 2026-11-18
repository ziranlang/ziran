# Rejected call-load forwarding experiment

The experimental shared optimizer pass did not improve the widget geometric
mean. It was removed; no compiler change from this experiment is retained.

Five shuffled, alternating rounds on physical Alpha found native performance
unchanged overall (after/before geometric mean 1.00018) and Wasm 1.55% slower
(1.01555), while increasing generated code size. Every timed batch verified
its paint output. An initial apparent Wasm Input speedup did not reproduce.

## Matched widget results

Microseconds per two-frame scenario; each value is the median of five run
medians, each calculated from 11 samples with the repository's symmetric
1.5×IQR filtering. The host gate rejected jobs with unstable clocks or a busy
hyperthread sibling. Raw samples, excluded indices, rejected attempts and
artifact hashes are in [the matched report](call-borrow-widgets-matched.json).

| Scenario | Native before | Native candidate | Wasm before | Wasm candidate |
|---|---:|---:|---:|---:|
| Button | 4.061 | 3.972 | 2.296 | 2.348 |
| Text | 5.479 | 5.411 | 3.479 | 3.519 |
| Input | 5.827 | 6.036 | 3.317 | 3.360 |

Native Input was slower in all five paired rounds. Wasm Button was slower
in all five. Small improvements in native Button and Text did not compensate.

The restored baseline's widget bundles, Wasm modules and native runtime
were byte-identical to the original baseline. Its Wasm Input time changed
from 5.494 µs in the first isolated measurement to 3.317 µs in the alternating
comparison. Consequently, the initial candidate's 3.472 µs result is not
evidence of a speedup.

| Scenario | ZX bytes before | ZX candidate | Wasm bytes before | Wasm candidate |
|---|---:|---:|---:|---:|
| Button | 1,379,565 | 1,407,565 | 2,244,279 | 2,250,232 |
| Text | 1,151,529 | 1,180,161 | 2,092,196 | 2,099,171 |
| Input | 1,991,098 | 2,026,274 | 2,333,124 | 2,340,130 |

## What was tried

For an unchecked direct call, the pass found a single-use record or array
load in the same block, with no intervening effect, check or possibly trapping
operation. It replaced the by-value argument with the load's source pointer
and made a bounded, shared callee variant with an entry snapshot load.
Backend view analysis could then avoid copies when safe, while retaining the
snapshot when another alias writes the source. Other calls and function
addresses kept the original callee.

This moved a snapshot across the call boundary; it did not establish a
general interprocedural read-only/no-alias contract or remove all callee
parameter storage. Remaining paint-path calls include source pointers received
as parameters and callees with writes and host calls. Eliminating their
snapshots needs a stronger alias proof. This result rejects this prototype,
not every possible implementation of read-only parameter borrowing.

The prototype's focused tests covered snapshot preservation across alias
writes, intervening effects, multiple uses, checked code, work/memory/function
limits and fuel parity. An end-to-end recursive alias case agreed across C,
the interpreter, native code and Wasm. The candidate passed all 271 local tests
and bootstrap before removal.

## Standard suite and kernels

The 29-workload standard suite passed every independent C result comparison.
All five generated artifacts (`c`, `go`, `native`, `suite.zx`, `suite.wasm`)
were byte-identical before and after. Their timing differences therefore do
not demonstrate any code improvement or regression from this pass.

The five kernels also verified every result. Records contains no call that
this pass could change: its isolated native measurements were 31.108 and
31.944 µs; Wasm was 40.985 and 41.868 µs. This experiment does not address the
Records gap relative to C.

Raw retained runs:

- [Standard before](standard/call-borrow-before.json) and
  [candidate](standard/call-borrow-after.json).
- [Kernels before](call-borrow-kernels-before.json) and
  [candidate](call-borrow-kernels-after.json).
- [Widgets initial baseline](call-borrow-widgets-before.json),
  [candidate](call-borrow-widgets-after.json), and
  [restored baseline](call-borrow-widgets-restored.json).

Benchmark compilation and timing ran only on physical Alpha, one suite at a
time through its build queue, with learning disabled. The baseline source
revision and exact tool, runtime, workload and generated artifact hashes are
retained in the JSON reports. The candidate was an uncommitted experiment.
Published site artifacts were restored after the trial.

After removing the prototype, the restored checkout passed all 271 local
tests again, with none reused, and `make check-bootstrap`. The restored
current-tree toolchain was installed on Omega and the original Alpha
toolchain was reinstalled; all 11 installed binaries matched their respective
build outputs. No website deployment was made for this experiment.
