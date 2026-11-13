Status: plan, 2026-11-12. Planning only; this document commits to no code,
build, lock, vendor or downstream change.

## Goal

All Projects repositories that use Ziran and Kryon should share the latest
canonical checkouts during development and share compiled outputs across
projects, so a Kryon or Ziran change quickly surfaces every application that
must update, and repeated compilation of pinned dependency versions stops.
Development reads shared latest sources; committed `ziran.lock` files and CI
releases stay exactly reproducible. The plan does not promise automatic mass
edits or releases: app updates remain separate, reviewed, upstream-first
commits.

## Current state (measured, read-only)

- Ziran already reuses native objects per output directory (`.objects`),
  keyed by source/header contents, compiler executables and reported
  subtools, command flags, environment, and include roots, under a writer
  lock; it always relinks. See `docs/USAGE.md` ("Generate native code") and
  `docs/IMPLEMENTATION_STATUS.md` (GCC object cache, native build parallel).
- Ziran's dependency-owned artifact engine already maintains a shared,
  content-addressed cache under `$XDG_CACHE_HOME/ziran/artifacts` with
  SHA-256 descriptor digests, atomic publication by directory rename,
  digest verification on hits, and dirty-checkout reuse refusal
  (`docs/PACKAGES.md`, "Dependency-owned native artifacts"). The immutable
  source cache lives in `$XDG_CACHE_HOME/ziran/sources`; native object and
  toolchain state in `$XDG_CACHE_HOME/ziran/native`.
- Kryon declares provider recipes (`renderer-raylib`, `renderer-desktop`,
  `renderer-web`, `native-sdl3`) in `ziran.toml` `[artifacts.*]` and ships
  `scripts/renderer_artifact.py` for consumers; its `ziran.local.toml`
  already overrides `ziran` and the codec packages to canonical checkouts.
- Applications resolve packages through committed locks (`ziran.lock`,
  format 1, full commits) and develop against latest via ignored
  `ziran.local.toml` `[overrides]`; overridden checkouts record a
  `development`/`unborn:` identity when they have no commits. `--locked`
  ignores overrides. CI builds the pinned toolchain through the setup
  action, reusing the published toolchain tree for that commit.
- Projects inventory (manifest scan, 2026-11-12): `kryonlabs/taskmgr` pins
  `kryon` and `ziran` at `master`; `kryonlabs/explorer` pins exact commits
  for both; `kryonlabs/daochi` pins Ziran/Kryon commits and still vendors a
  third-party source dependency (`liboqs`); `ziranlang/packages/*` are
  upstream packages consumed by lock. No app currently edits Kryon or Ziran
  source in place.

## Design

### 1. Two modes, one mechanism

- Development mode: an app's ignored `ziran.local.toml` maps `kryon` and
  `ziran` (and codec packages as needed) to the canonical
  `../../<org>/<repo>` checkouts. Overrides are the only way local latest
  sources participate; no tool or script hard-codes repository paths.
- Release mode: committed `ziran.lock` plus `--locked` reproduce the exact
  graph and toolchain; CI never sees overrides. Upstream commits to Kryon
  or Ziran `master` first; apps follow with their own `ziran lock` bump
  commits after the upstream push.
- Identity already derives from repository + commit (or override checkout
  HEAD), so latest-checkout builds and locked builds share the stores below
  only when the resolved revisions match, and dirty checkouts never collide
  with clean results (existing engine rule, kept unchanged).

### 2. Shared content-addressed module/object store (Ziran)

Promote the existing per-project `.objects` reuse to an optional shared
store keyed by the complete input set:

1. compiler identity: launcher/compiler bytes, version banners, reported
   subtools, installed specs (as today's probe fingerprints do);
2. target: OS-arch-ABI plus backend (`c`, `go`, `wasm`) and codegen flags
   (`CFLAGS`/`LDFLAGS`/`LDLIBS`, `--release`, feature defines);
3. environment: only variables a compiler reads, include roots, and the
   selected tool prefixes;
4. package graph: locked package IDs and commits (or override HEADs),
   carried through the existing package identity scheme;
5. module inputs: the module's own `.zi`/`.zir` bytes and, for the API
   boundary, the emitted public header bytes of every imported module.

The API-ABI input is the full emitted public header digest, not a
signature-only digest: Ziran's headers carry `static inline` copies of
small public procedures, so a body edit to an inline procedure must
rebuild callers, while an out-of-line body edit legitimately reuses them.
This matches the documented header-inline behavior and prevents stale
reuse by construction. Store entries publish atomically (temporary
directory, then one rename, the artifact engine's existing pattern) into
`$XDG_CACHE_HOME/ziran/modules/<key>`; every read verifies the recorded
digest and falls back to a normal rebuild on any mismatch, so torn or
damaged entries can never serve stale bytes. Linking stays always-fresh,
as today, so changed libraries always reach executables.

### 3. Leases and cache safety

- Per-content-key exclusive leases: `O_EXCL` lease files recording owner
  PID, boot ID, and timestamp; concurrent writers to one key retry
  briefly, then take over a lease only after the recorded owner is provably
  dead or the lease is idle past a fixed timeout. Readers never block; they
  verify digests instead.
- The existing per-output-directory writer lock stays for project-local
  link/replace steps.
- No global pruning. Idle cleanup remains limited to each project's own
  scratch outputs. Shared stores grow until an explicit, user-invoked,
  lease-respecting maintenance command runs; failed or damaged entries are
  never auto-deleted (matching the source cache's repair-not-delete rule).

### 4. Telemetry and receipts

- Extend the existing compiler profiles: per build, record store hits,
  misses, reused-object count, compile seconds saved versus a no-store
  rebuild estimate, and the resolved key components. Versioned schema;
  checked by a focused test.
- Each project build writes a receipt JSON beside its outputs recording
  resolved graph commits/HEADs, key hash, hit/miss counts, and the verified
  digest list. The artifact engine's `artifact.tsv` stays the shared-cache
  receipt for provider outputs.
- Retained benchmark reports stay under tracked `bench/results/` per
  repository rules; raw receipts of disposable builds stay in each
  project's `build/`.

### 5. Projects manifest index and affected-app graph (Harmony orchestration)

- A generator script, owned by `harness/harmony`, scans canonical
  `/mnt/storage/Projects` repositories for `ziran.toml`/`ziran.lock` and
  writes an index (repository, package name, toolchain ref, dependency
  name to ref kind: `master` | pinned commit | override present, `tool =
  "kryon"`), plus the resolved lock commits. Index data is generated
  state; nothing in Ziran or Kryon hard-codes it.
- The affected-app graph is derived, not stored: edges `kryon -> ziran`
  (toolchain), `app -> kryon`, `app -> ziran`. After a Ziran or Kryon
  commit, the orchestrator queries the index and, for each dependent app,
  runs under `build-queue` with `DISPLAY`/`WAYLAND_DISPLAY` removed:
  (a) override-mode `ziran check` against latest canonical checkouts to
  detect breakage early, and (b) locked-mode `ziran check --locked` to
  confirm the committed state still builds.
- The sweep emits a report listing apps whose locks are behind upstream
  and which fail override-mode checks (the compatibility worklist). It
  never edits app repositories; lock bumps remain per-app commits.
- Ziran gains the generic query surface this needs (read-only): report,
  for a given manifest set, which resolved revisions pin which packages;
  Kryon and apps gain nothing app-specific here.

### 6. Upstream-first migration inventory

Inventory method (repeatable, read-only): the index generator's dependency
table flags (i) any dependency whose checkout under the app is a vendored
directory rather than a locked package, (ii) any `ziran.local.toml`
override pointing outside canonical `ziranlang/` or `kryonlabs/` checkouts,
and (iii) apps missing lock entries for packages their manifests declare.
Migration is upstream-first and per-app: Kryon fixes land in
`kryonlabs/kryon` and Ziran fixes in `ziranlang/ziran` on `master`; an app
then updates its `ziran.lock` (or retires a `vendor/kryon` pointer by
replacing it with a dependency). Pinned third-party C sources
(`source = true`, e.g. raylib, SDL3, libavif) are sanctioned and are not
vendor copies of Kryon/Ziran code; they stay provider-owned. Kryon's
existing override of `raylib` to a cache vendor path is development-only
and must resolve back to the locked commit for `--locked` runs.

### 7. Failure recovery

- Failed provider or object builds remove only their own staging and
  preserve previously published results (existing engine rule).
- Digest-verification failure on a shared-store read rebuilds locally and
  republishes atomically; the damaged entry is recorded in the receipt and
  left in place until explicit maintenance.
- Source-cache corruption keeps today's behavior: restore from existing
  Git objects, refuse modified checkouts with a `preserved at` diagnostic.
- A crashed sweep leaves per-app reports marked incomplete with the
  failing command; the next run recomputes only that app.

### 8. Ownership split

| Surface | Owns | Must not own |
| --- | --- | --- |
| Ziran | Shared module/object store, keys, leases, receipts schema, dependents/revision query, lock/override semantics, setup-action toolchain caching | App lists, Kryon names, Projects index data, automatic lock bumps |
| Kryon | Complete provider recipes (`renderer-*`, `native-sdl3`) whose declared inputs/outputs fully describe them, its laws and tests; consume the store via the artifact path | Store implementation, app manifests, orchestration |
| Harmony orchestration | Manifest index generation, affected-app sweep scheduling and reports, build-queue routing, cross-repo report storage | Editing app sources/locks, mass automation, pixel capture |
| Downstream apps | `ziran.lock` bumps after upstream pushes, retiring vendored Kryon copies, declaring system `links` | Shared-store code, upstream source edits, vendor edits of Ziran/Kryon |

### 9. Rollout and gates

1. Ziran: add the shared store behind an environment opt-in, with
   receipts. Gate: extended reuse/staleness tests pass and per-project
   default behavior is byte-identical when disabled.
2. Ziran: default the store on. Gate: full `make check` and
   `make check-bootstrap` pass, plus the cross-project sharing fixture.
3. Kryon: verify recipes declare complete inputs/outputs and consume the
   store through the artifact path. Gate: `make test`, the focused
   renderer artifact fixture, and `make push-check` before any push.
4. Harmony: index generator plus sweep reports. Gate: index matches a
   hand-checked sample of three repositories; sweep reports are read-only.
5. Apps: per-app `ziran lock` bump commits after upstream pushes,
   reviewed individually; no batch automation.

### 10. Tests and benchmarks

- Ziran: extend `tests/native_object_reuse.sh` and
  `tests/native_build_cache.sh` with a cross-project fixture: two projects
  sharing one resolved graph under different roots; the second must reuse
  the first's objects and pass its behavior tests. Add a staleness matrix
  that mutates each key input (compiler bytes, one flag, one environment
  variable, a dependency's public header, an inline procedure body, an
  out-of-line body) and asserts miss/miss/miss/miss/miss/hit respectively.
  Run `sh tests/packages.sh` for the artifact path and `make sanitize` on
  changed compiler code. All checks with `DISPLAY`/`WAYLAND_DISPLAY`
  removed; builds through `build-queue`.
- Kryon: `make test`; the renderer recipes through the existing
  package-artifact fixture runner; `make push-check` before pushing
  shared-backend changes.
- Benchmarks: `bench/native_builds.py` and `bench/compiler_scaling.py`
  (with real package entries enabled) on Alpha's physical host per the
  standing benchmark instruction, paired preserved-compiler runs, 1.5x IQR
  exclusion, raw samples and excluded indices retained.

### 11. Acceptance thresholds

Measured on Alpha, warm caches, median of retained samples after IQR
exclusion:

- Rebuild sharing: after a Kryon `master` commit, an unchanged app
  consuming it via overrides rebuilds with at least 50% fewer compiled
  translation units and at least 40% lower wall time than the same app
  before the store exists; unchanged Kryon modules report store hits.
- Cross-project reuse: a second app resolving the identical graph and
  environment achieves at least 90% object hits on its first build.
- No stale reuse: every staleness-matrix mutation forces a miss and the
  rebuilt output passes the corresponding behavior test; digest checks run
  on every hit (asserted in receipts).
- Correctness gates: Ziran `make check` plus `make check-bootstrap`, Kryon
  `make test` and `push-check` green on the adopting revisions before any
  default-on rollout step.

## Non-goals

No global cache pruning, no automatic mass app edits or lock bumps, no
release or version bump tied to this work, no compatibility paths for old
spellings or formats, and no Kryon- or app-specific branches in the Ziran
compiler.
