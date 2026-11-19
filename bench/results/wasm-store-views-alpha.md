# Wasm stores from borrowed loads

Wasm can use a loaded record directly as a store source when every use stays
in the same block before any possible memory effect. This removes a temporary
record snapshot and its extra copy. Any later read after a store, call,
allocation or other possible write retains the original snapshot.

Stores through runtime pointers use `memory.copy` for borrowed sources, so
partially overlapping ranges preserve their original bytes in both directions.
Frame and fresh-allocation copies keep their existing short-copy lowering.
The rule remains disabled for builds with runtime checks.

Local tests on physical Alpha passed the complete lowered parity test across
C, the instruction engine, native bundles, native ELFs and Wasm. The existing
Wasm value test now includes typed host pointers that overlap by one word,
self-copy, an intervening write and a later snapshot read. All expected-value
checks live in Ziran; the JavaScript fixture only supplies host addresses.

## Matched widget comparison

Compilation and timing ran on physical Alpha with learning and runtime checks
disabled. Five shuffled rounds measured baseline and candidate with 11 samples
per run. Every timed batch verified its paint output. Symmetric 1.5x IQR
filtering and the existing host gate were applied to all variants.

Microseconds per two-frame scenario, median of five run medians:

| Scenario | Wasm before | Wasm after | Wasm bytes removed |
|---|---:|---:|---:|
| Button | 2.395 | 2.367 | 461 |
| Text | 3.594 | 3.586 | 509 |
| Input | 3.434 | 3.401 | 461 |

The geometric mean timing ratio was 0.9922. This is a small observed change;
individual rounds varied in both directions, so it does not establish a large
or statistically reliable speedup. The supported gains are fewer record copies
and smaller generated modules, with preserved output and no overall measured
slowdown in this comparison.

The three baseline and candidate ZX artifacts were byte-identical. Their native
control had a geometric mean ratio of 1.0074; the harness executable was
relinked, so its bytes differed even though its native compiler sources did not
change for this trial. The Wasm artifacts all changed. The baseline already
contained the integrated loop-inlining and devirtualization fixes, isolating
the Wasm backend change from those optimizer changes.

The [matched raw measurements](store-view-widgets-matched.json) retain samples,
excluded indices and artifact identities. The [generation report](store-view-widgets-after.json)
records compiler tool digests, source hashes and all four execution targets.
The final shared-tree local run executed all 273 retained tests without reuse:
272 passed and the audio test stopped on a cached compiler path removed with
the old command wrapper. Refreshing that disposable CMake configuration made
the retained audio test pass on its focused rerun, without source changes.
Linux, Darwin and SIMD bootstrap seeds matched. The current tree was installed
on Omega; all 11 installed tool binaries matched their build outputs, and the
installed compiler ran Hello World successfully. This validation is separate
from the Alpha timing comparison above.
