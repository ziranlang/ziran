# Native field-copy checkpoint on physical Alpha

The compiler fix is [committed and pushed](https://github.com/ziranlang/ziran/commit/1fd89e81cdb66a1c020382f6d7a67251c31c7918).
Native widget calls improve 121–185× against the same-host baseline. They
remain 1.78–2.33× slower than Go; the owner's Go-performance target remains open.

## Same-host measurements

Physical Alpha: Intel Core i7-8700, `systemd-detect-virt` reports `none`.
Both runs use eleven samples per target, checks off, learning off, explicit
Native policy for widgets and explicit Engine policy for kernel engine cells.
Medians use the same symmetric 1.5×IQR rule with inclusive interpolated quartiles.
Raw samples, exclusions, checksums, compiler revisions, tool hashes and harness
hashes remain in the JSON receipts. Every measured batch verifies its result.
Initialization, compilation and loading are outside widget timing; sessions
persist across calls.

Baseline compiler: [pushed master before this checkpoint](https://github.com/ziranlang/ziran/commit/888f39a992cc0a18d1f0fa0e41f0070dc0002a6c).
After compiler: the linked field-copy checkpoint, including the separately
[accepted zero-index prerequisite](https://github.com/ziranlang/ziran/commit/8a3b727cfc372ecf78be7e978957968336698707).
Both have clean compiler sources. The three-file benchmark harness overlay is
identical between runs and recorded by hash. The ordinary upstream Kryon
fixture revision is recorded separately; no widget-specific compiler behavior
was added.

Nanoseconds per warm two-frame scenario:

| Scenario | C after | Go after | Native before | Native after | Speedup | Native / Go |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Button | 2825.8 | 4826.1 | 1590729.4 | 8620.4 | 184.5× | 1.79× |
| Text | 3101.9 | 4120.8 | 1165004.4 | 9607.1 | 121.3× | 2.33× |
| Input | 4125.7 | 7385.0 | 1904851.1 | 13139.3 | 145.0× | 1.78× |

Receipts:

- `widget-alpha-2026-11-16-baseline.json`
- `widget-alpha-2026-11-16-fields-after.json`
- `wasm-tier-alpha-2026-11-16-explicit-engine-baseline.json`
- `wasm-tier-alpha-2026-11-16-fields-after.json`

The published widget matrix and downloadable C, Go, ZX, source and checked-IR
artifacts come from the after run. Kernel results stay separate from widgets:
the native kernel column executes 32 calls per ELF process and includes startup
divided by 32. The Calls kernel can be constant-folded. Historical Auto-policy
receipts cannot support instruction-engine claims; replacement engine receipts
select Engine explicitly.

## Implementation and semantics

Unchecked aggregate loads used only by member extraction within their defining
block can use the original address. All eventual nested reads must precede
every possible write, allocation, resize, insert or call. Escapes, cross-block
uses and checked programs retain their snapshot. Address sharing is recomputed
for the resulting memory views, including changed nested views.

Native entry reuses per-thread stack bounds only while the current frame lies
inside the cached range and `RLIMIT_STACK` remains unchanged. A switched stack,
changed limit or failed query takes the established query/fallback path. The
per-entry depth cap and stack reserve still apply.

Earlier explicitly requested Omega diagnostics, retained in
`widget-native-omega-2026-11-16.md`, locate the original 132,523-byte
TreeIsBuilding snapshot and show it replaced by a direct byte read with a
32-byte frame. Those cumulative shared-tree measurements include other lanes'
compiler work. The clean Alpha comparison above measures the pushed checkpoint
cohort and its zero-index prerequisite.

## Local validation

The isolated ten-file field/cache closure on clean master built and passed
`tests/machine_ir.sh` and `tests/native_load.sh`. Its retained tests exercise
large and nested records, intervening stores and calls, cross-block uses,
escaping snapshots, distinct pointer bases, changed views and checked builds.
Runtime tests exercise changed stack limits, multiple threads, alternate stacks
and cross-instance reentry.

Omega then ran the current canonical integrated source through `build-queue`:

```sh
env -u DISPLAY -u WAYLAND_DISPLAY ZIRAN_LEARN=off build-queue -- sh
make -j4 all build/bin/native-load-test
make check
make check-bootstrap
```

Build passed. The full suite ran all 281 retained tests without reuse:
279 passed, including machine IR, ELF, native loading and both site suites.
Bootstrap matched for every retained Linux and Darwin module. Compiler, runtime,
test and build-rule fingerprints across 3,404 files were identical before and
after the pipeline.

Two existing failures remain separately reported: `explain.sh` finds a
diagnostic registry count mismatch, and `zstd_go.sh` requires Go 1.25 while the
test environment has 1.24.4 with automatic toolchain download disabled. The
registry mismatch also reproduces on clean pushed master. The clean Alpha
baseline additionally exposed its missing ffmpeg and package-inventory setup;
these are not reported as passing tests.

Build/test command receipts and complete logs are retained privately under
ignored `backup/`; reusable benchmark drivers and public result receipts remain
in their established tracked folders. GitHub CI and actual site deployment are
separate from these local tests.
