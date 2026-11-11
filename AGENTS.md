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
- Test with `make check` (every check, run in parallel on `BUILD_QUEUE_JOBS`)
  and `make check-bootstrap`; `make check CHECK_FILTER=text` reruns only the
  checks whose name contains `text`. Full logs are in `build/check-logs/`.

- Module roots are ordinary repeatable search folders. Keep package manifests,
  CLI forwarding, editor paths and loader storage dynamically sized; do not add
  a folder-count cap or library-specific compiler handling. Preserve resolution
  ordering and use normal input-size, allocation and operating-system bounds.

- Deploy the website only through `.github/workflows/site.yml` on GitHub
  Actions, which builds and validates the site before publishing to Cloudflare
  Pages. Never invoke Wrangler directly from a local agent or terminal,
  including authentication commands. The workflow's Cloudflare action owns
  uploads.

- Do not create a `backup/` directory in this repository. Keep reusable
  benchmark scripts in Ziran under `bench/`, and retained benchmark reports
  and logs under tracked `bench/results/` paths. Temporary compiler and test
  outputs belong in the existing `build/` layout.
