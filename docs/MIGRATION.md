# Two-repository migration

This is the intended end state and remaining migration work. The current state is recorded
in [Implementation status](IMPLEMENTATION_STATUS.md).

## End state

| Concern | Repository after cutover |
| --- | --- |
| `.zi` language, checker, laws, standard library, `.zir`, native backends, `.zx`, portable runtime | Ziran |
| Reusable widgets, layout, styling, accessibility, interaction, renderers, host adapters implemented in `.zi` | Kryon |
| Product behavior and application UI | Each application repository |

Ziran must build, check, and run non-graphical programs independently. Kryon
is a normal importable Ziran library. No language file, IR node, compiler
branch, linker rule, or loader rule recognizes Kryon widgets by name. UI
behavior is expressed by Kryon's `.zi` modules. External C libraries and OS
services enter through declared FFI or host capabilities; they do not hold
Kryon's widget or runtime policy.

## Current state and remaining work

The old Kryon `.kry` compiler and `.krb` runtime have been removed. All
maintained Kryon UI modules are `.zi` sources using current Ziran syntax. The
complete Kryon `make all` build passes checked `.zir`, C, and Go generation
and native backend compilation with the current Ziran compiler. Platform host
integration and application behavior gates remain incomplete. Inbe has moved
its application sources to `.zi` and is migrating its native services, but its
full source gate and app build do not pass yet. Inbe now pins the current
verified local Kryon and Ziran revisions while its source migration proceeds.

1. Finish generic Ziran imports, typed blocks and slots, laws, saved `.zir`,
   and the `.zx` capability contract without importing Kryon.
2. Complete Kryon's native, browser, and portable host effects through explicit
   interfaces and verify widget behavior in applications.
3. Port representative applications to Kryon imports and run their behavior
   and supported target gates.
4. Migrate downstream source and dependency pointers to `.zi`, `.zir`, and
   `.zx`. Existing `.krb` bytes are not `.zx` and require recompilation.

All Kryon changes are made in the upstream Kryon repository, then consumed by
applications through clean dependency updates. The Ziran repository owns only
language and portable execution work. The two repositories may progress
independently while the downstream application cutover is completed.

## Completion gates

- Non-UI `.zi` and `.zir` builds work with no Kryon checkout or graphics
  dependency; equivalent `.zx` programs run without a display.
- A saved `.zir` can be read by every supported backend; equivalent source and
  saved-IR builds behave the same. The `.zx` loader rejects malformed bundles
  and missing capabilities before execution.
- Kryon builds from maintained `.zi` source and is linked only when imported.
  Widget calls are ordinary typed calls, and supported UI examples have no
  compiler or IR widget exceptions.
- Supported C, Go, and portable targets, renderer paths, and downstream
  applications pass their relevant behavior and build gates before changing
  their public input and output extensions.
