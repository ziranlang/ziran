# Records signed interval lowering on Alpha

The signed interval fold is implemented and locally validated in
[the compiler change](https://github.com/ziranlang/ziran/commit/617dd0ede20588877f808affb3a3abc085e0871e).
It replaces `x > U || x < 0`, in either order, with one same-width unsigned
comparison when both operands read the same stable local, record member or
fixed-array element. Calls, pointer reads and expressions with relevant second
evaluations retain normal short-circuit behavior.

The fresh Alpha cohorts measured Records native at 67.343 µs before and
46.358 µs after; C measured 16.427 and 16.456 µs. The after native result is
still **2.82 times C**. These sequential measurements do not establish a clean
causal speedup: the unchanged Hash native executable also became 21% faster,
and the identical Records baseline previously measured 43.637 µs in the
[earlier matched report](loop-passes-alpha-2026-11-16.md). No C parity is claimed.

| Kernel | C before, µs | C after, µs | Native before, µs | Native after, µs | Native image changed |
| --- | ---: | ---: | ---: | ---: | --- |
| Sieve | 34.238 | 34.199 | 56.232 | 56.427 | No |
| Records | 16.427 | 16.456 | 67.343 | 46.358 | Yes |
| Hash | 27.714 | 27.468 | 46.002 | 36.319 | No |
| Vectors | 34.648 | 33.883 | 88.841 | 89.479 | No |
| Printing | 9.603 | 9.438 | 11.800 | 11.534 | No |

Records native image size falls from 2,816 to 2,752 bytes. Both arms preserve
the result 68,788 and the native 32-call checksum 2,201,216. The C executable
is byte-identical between arms. This experiment isolates the bounds fold;
it does not include the separately measured LICM profitability fix.

Both cohorts compiled fresh source and ran on physical Alpha, an Intel Core
i7-8700. They used the same unchanged registered `wasm-tier` entry point,
11 samples per series, learning disabled, and symmetric 1.5×IQR filtering
with inclusive interpolated quartiles. C, Go, instruction-engine, native and
Wasm results and output were checked. Native wall times include one process
startup divided by 32; C and the other targets use the existing in-process
timers. CPU affinity and governor were not changed. Frequency observations
are untimed snapshots for all logical CPUs, not proof of the child's clock
frequency or migration.

The experiment kept Alpha's canonical source checkout at its original
revision. It temporarily replaced only the generated lowering C module and
its object, rebuilt the aggregate object and relinked the two compiler tools
at their stable paths. Every replaced file was restored. Source, harness,
compiler object, tool, default index and immutable standard-benchmark guards
all matched after restoration. Alpha was released to the standard lane as
soon as the run finished.

The [before samples](records-bounds-alpha-2026-11-16-before.json) and
[after samples](records-bounds-alpha-2026-11-16-after.json) retain all raw
measurements, exclusions, artifact identities and host observations.
[Provenance](records-bounds-alpha-2026-11-16-provenance.json) records source and
tool identities, the complete runner and restoration verdicts.
[Verification](records-bounds-alpha-2026-11-16-verification.json) independently
recomputes the quartiles, exclusions and medians of all 50 series.

Compiler optimization, vectorization and lowered C/native/interpreter/Wasm
parity passed locally. The final shared retained suite passed 284/284 local
tests and bootstrap comparison matched. The compiler change is pushed;
GitHub CI has not been checked, and no installation or release is claimed.
An interleaved comparison is still needed to separate the timing effect from
host variation. The native gap against C remains open.
