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
