# Ziran repository rules

Ziran owns the language, compiler tools, `.zir`, portable `.zx` output,
standard modules, and runtime. Keep this repository usable for programs that
do not import a UI library.

## Layout

- `cmd/` compiler and tools (`ziran`, `zi2c`, `zi2go`, `zi2zx`, `zx2native`,
  `zx2wasm`, `zi-edit`, `zi-fmt`, `zi-api`, `zi-inspect`, `zi2zir`).
- `bootstrap/` generated C seeds; refresh with `make update-bootstrap`.
- `std/` standard modules; `tests/` retained checks; `proofs/` Lean model.
- `bench/` benchmark drivers; retained receipts in `bench/results/`.
- `docs/` user and contributor documents; `plans/` open work;
  `ASSEMBLY/` machine-IR and ZX format specs; `site/` generated website.
- `scripts/` site and release helpers; `templates/` project templates.

## Commands

- Build: `make`; install: `make install-user` (into `~/.local/bin`).
- Checks: `make check` and `make check-bootstrap`. `make check
  CHECK_FILTER=text` runs every check whose name contains `text`. Full logs
  land in `build/check-logs/`.
- Run checks with `DISPLAY` and `WAYLAND_DISPLAY` removed; any test that needs
  a display uses a private Xvfb/Xephyr.
- Proof model: `make proof-model LEAN=/path/to/lean` (see docs/LAWS.md).

## Question -> document

| Question | Document |
| --- | --- |
| How do I compile, run, or package a program? | docs/USAGE.md |
| Which targets and features are supported today? | docs/IMPLEMENTATION_STATUS.md, docs/FEATURES.md |
| How do packages, projects, and locks work? | docs/PACKAGES.md |
| How do native TypeScript and TSX sources work? | docs/NATIVE_TYPESCRIPT.md |
| What does the compiler pipeline do? | docs/ARCHITECTURE.md |
| How do I write Ziran (syntax by example, habits from other languages, std signatures)? | docs/REFERENCE.md (site/llms.txt is generated from it) |
| What is the language syntax (directives, enums, imports)? | docs/LANGUAGE_DIRECTION.md |
| How do owned values and recoverable failures behave? | docs/OWNED_VALUES.md |
| What do diagnostic codes and JSON diagnostics look like? | docs/DIAGNOSTICS.md |
| How do tools edit and query source programmatically? | docs/AUTHORING.md, docs/API_DISCOVERY.md, docs/SEMANTIC_EDIT.md |
| How do laws and proof certificates work? | docs/LAWS.md |
| What is `.zir`? | docs/ZIR.md |
| What is `.zx` and how is it linked? | docs/ZX.md, ASSEMBLY/zx-executable-format.md, ASSEMBLY/zx-library-format.md |
| How does direct native code generation work? | ASSEMBLY/README.md, ASSEMBLY/machine-ir.md, ASSEMBLY/lowered-format.md |
| How is Unicode handled? | docs/UNICODE.md |
| How do benchmarks run? | bench/README.md, bench/standard/README.md |
| What work is still open? | plans/open-work.md |

## Rules

- Scope: no Inbe application code (`../inbe`); no Kryon widgets or
  UI-specific compiler cases (`../kryon` is an ordinary library). Add a
  capability here only when non-graphical programs benefit. The exception
  is ZX Runner and the platform layer it hosts (window, drawing, input,
  media) on Linux and Android, which belong here even though they are
  visual (owner, 2026-11-23); widgets stay in UI libraries.
- No backward compatibility: use current Ziran syntax only. Remove legacy
  spellings, flags, binary formats, and entry points; keep the rejection
  tests that enforce current rules.
- Keep source, checked `.zir`, native targets, and instruction-engine
  behavior aligned. Generated output carries only the code a program reaches;
  shrink output by removing what is not needed.
- Module roots are ordinary repeatable search folders. Keep package manifests,
  CLI forwarding, editor paths, and loader storage dynamically sized; no
  folder-count caps or library-specific compiler handling.
- Write retained test logic and helpers in Ziran. Remove redundant tests and
  unused helpers instead of growing a parallel suite. Run every retained
  check; never substitute sharding or skipped tests.
- Keep private transfer and recovery state under the ignored `backup/`.
  Keep benchmark reports under tracked `bench/results/`; temporary compiler
  and test outputs under `build/`.
- Benchmarks run directly on Alpha's physical host, never virtualized. If
  Alpha is unavailable, wait instead of measuring elsewhere. Record the host
  in every report; collect both sides of a comparison on the same host.
  Exclude anomalies with the symmetric 1.5xIQR rule (inclusive interpolated
  quartiles) for every target and platform; keep raw samples and excluded
  indices.
- Shared checkout: commit on master through the shared index. `git add` only
  your own paths, then `git commit -- <those paths>`, so other lanes' staged
  paths stay out. Never `git add -A`, a private `GIT_INDEX_FILE`, plumbing
  that writes commits or moves HEAD, a rebase, reset or stash; never touch
  files another lane is editing.
- Deploy the website only through `.github/workflows/site.yml` on CI. Never
  invoke Wrangler directly from a local terminal.
