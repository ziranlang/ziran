# Record argument prototypes on Alpha

The final scalar projection prototype was rejected. It increased code size
without improving the widget geometric mean: native ZX was 1.08% slower and
Wasm 0.41% slower in the matched comparison. Every timed batch verified its
paint output. No record argument optimization from these trials is retained.

## Fresh scalar projection comparison

These measurements used a rebuilt `zi2zx` and changed artifacts for all three
widgets. Compilation and timing ran on physical Alpha with learning disabled.
Five shuffled rounds measured each baseline and candidate artifact with 11
samples per run. Medians use symmetric 1.5×IQR filtering. The host gate recorded
and rejected unstable clocks or busy hyperthread siblings. See the
[raw matched comparison](projected-call-fresh-widgets-matched.json) and
[compiler and artifact provenance](projected-call-fresh-widgets-after.json).

Microseconds per two-frame scenario, median of five run medians:

| Scenario | Native before | Native candidate | Wasm before | Wasm candidate |
|---|---:|---:|---:|---:|
| Button | 4.211 | 4.177 | 2.401 | 2.415 |
| Text | 5.728 | 5.837 | 3.618 | 3.642 |
| Input | 6.058 | 6.190 | 3.447 | 3.446 |

| Scenario | ZX bytes before | ZX candidate | Wasm bytes before | Wasm candidate |
|---|---:|---:|---:|---:|
| Button | 1,379,565 | 1,428,925 | 2,244,279 | 2,253,324 |
| Text | 1,151,529 | 1,201,905 | 2,092,196 | 2,101,361 |
| Input | 1,991,098 | 2,065,386 | 2,333,124 | 2,345,783 |

The prototype made a private callee variant replacing an aggregate formal
used only for constant numeric field reads with up to four scalar formals.
It captured those fields immediately after the caller's original aggregate
load, before any alias writes or effects, retaining by-value snapshot
semantics. Original functions and function addresses retained their ABI.
Fresh entry parameters were reordered into signature order for native code.
Focused local tests covered integer and float fields, alias writes, intervening
effects, variant reuse, checked accesses, escapes, fuel and resource limits.

## Earlier runs and a missing build dependency

Earlier source-only edits rebuilt the embedding engine but left the optimizer
inside `zi2zx` stale. `assembly.mk` lacked the existing specialization module
and the experimental module in its source prerequisites. The final trial
added both prerequisites before rebuilding. Only the existing specialization
module remains necessary after removing the prototype.

| Report prefix | Optimizer actually measured | Interpretation |
|---|---|---|
| `readonly-borrow` | Read-only pointer variant | Initial pointer trial |
| `readonly-borrow-scalar` | Same read-only pointer variant | Does not measure its proposed source revision |
| `readonly-borrow-cache` | Pointer variant plus copy cleanup | Second pointer trial |
| `projected-call` | Same pointer variant plus copy cleanup | Does not measure scalar projection |
| `projected-call-ordered` | Same pointer variant plus copy cleanup | Does not measure reordered scalar formals |
| `projected-call-fresh` | Scalar projection with ordered formals | Fresh final comparison above |

The first two rows share the exact `zi2zx` digest and widget artifact hashes.
The following three rows also share a compiler digest and artifacts, despite
changes in embedding-library digests. Those timing differences cannot support
claims about the proposed source revisions. Full digests and raw samples are
preserved in each `*-widgets-after.json` and `*-widgets-matched.json` report.

The fresh scalar compiler digest was
`e834b1601357b415ec0b11be58f86d4a526a307a0b1b6e4a3cc5929bff13a205`;
the stale pointer/copy-cleanup compiler digest was
`e9d276686b6c905d91a075a92b890ce6a7913f66ae8b067043b052495e8847e9`.

The pointer trials also failed to establish a useful overall gain. This rejects
these implementations; it does not settle whether stronger alias analysis
could remove snapshots in the remaining paint queue path.
