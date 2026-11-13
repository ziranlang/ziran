# Ziran repository rules

Ziran owns the language, compiler tools, `.zir`, portable `.zx` output,
standard modules, and runtime. Keep this repository usable for programs that
do not import a UI library.

- Do not put Inbe application code, screens, assets, workflows, or platform
  policy in Ziran. Those belong in `../inbe`.
- Do not put Kryon widgets or UI-specific compiler cases in Ziran. Kryon is an
  ordinary library in `../kryon`, imported by applications that need it.
- Add a capability here only when it is useful to non-graphical programs as a
  language or standard-library feature. Keep the source, checked `.zir`,
  native targets, and portable VM behavior aligned.
- Use the current Ziran syntax directly. Do not add legacy Kryon/KIR/KRB
  compatibility paths.
- Do not preserve old source spellings, command flags, binary formats, or
  runtime entry points for backward compatibility. Remove executable legacy
  paths when replacing them; keep rejection tests that enforce the current
  language rules.
- Run relevant checks with `DISPLAY` and `WAYLAND_DISPLAY` removed. Any test
  that needs a display must use a private Xvfb/Xephyr display.
- Test with `make check` and `make check-bootstrap`. The owner requested a
  physically pruned suite of the most useful checks, about half the former
  entry points. Run all retained checks locally and in CI; do not substitute
  sharding or skipped tests for pruning. `make check CHECK_FILTER=text` runs every
  check whose name contains `text`. Full logs are in `build/check-logs/`.
- Write retained test logic and test helpers in Ziran. Remove redundant tests
  and their unused helpers instead of growing another parallel test suite.

- Module roots are ordinary repeatable search folders. Keep package manifests,
  CLI forwarding, editor paths and loader storage dynamically sized; do not add
  a folder-count cap or library-specific compiler handling. Preserve resolution
  ordering and use normal input-size, allocation and operating-system bounds.

- Deploy the website only through `.github/workflows/site.yml` on GitHub
  Actions, which builds and validates the site before publishing to Cloudflare
  Pages. Never invoke Wrangler directly from a local agent or terminal,
  including authentication commands. The workflow's Cloudflare action owns
  uploads.

- Keep private transfer and recovery state under the ignored `backup/`
  directory. Keep reusable benchmark scripts in Ziran under `bench/`, and
  retained benchmark reports and logs under tracked `bench/results/` paths.
  Temporary compiler and test outputs belong in the existing `build/` layout.

- Owner instruction, 2026-11-11: run all benchmarks directly on Alpha's
  physical host and reject virtualized execution.
  Keep benchmark compilation and measurement off the current desktop; if
  Alpha is unavailable, report that instead of falling back locally. Record
  the actual execution host and keep before/after comparisons on that host.

- Exclude statistical anomalies from benchmark results using the same symmetric
  1.5×IQR rule for all targets and platforms (inclusive interpolated quartiles).
  Keep raw samples and excluded indices; never silently discard measurements.
