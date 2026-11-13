# Compile-time targets and public re-export identity

Implementation contract for 2026-11-13.

## Language target model

- `OS` is the selected target operating-system enum, exactly, with members
  `.LINUX`, `.WINDOWS`, `.MACOS`, `.ANDROID`, `.WEB`, and `.NONE`.
- `CPU` is the selected target CPU enum, exactly. Its supported members include
  `.X64`, `.ARM64`, `.WASM32`, and `.NONE` as appropriate to the selected
  output.
- `TARGET` is the selected output-backend enum, exactly, with members `.C`,
  `.GO`, `.WASM`, and `.ZX`.
- `OS`, `CPU`, and `TARGET` are ordinary typed compile-time values available to
  `#if`, `#ifx`, `#assert`, and `#run`. Enum-member shorthand resolves from the
  value on the other side of a comparison, so expressions such as
  `#if TARGET == .C && OS == .LINUX` work directly.
- An inactive `#if` branch remains undiscovered: imports in it are not loaded
  and cannot contribute declarations, errors, or transitive dependencies.
- `#assert false, "message"` reports the supplied message clearly at the
  assertion source location.

Language `OS` and `CPU` are target facts, never compiler-host facts. The
`Compiler` metaprogram API exposes the running compiler as unambiguous
`Compiler.HostOS` and `Compiler.HostCPU`, plus the selected target and compiler
options. Cross-compilation can observe host and target as different values
without activating an import for the host.

Portable `.zir` behavior is explicit: checked portable source that has not
selected a concrete native platform uses `OS == .NONE` and `CPU == .NONE`, while
`TARGET == .ZX`. A concrete frontend/backend invocation supplies its selected
target facts before discovery. Portable code branches on language target facts,
not host facts. Host metadata is for metaprogram tooling decisions only and
never selects source imports.

Retire `POSIX_THREADS` and `PLATFORM_WEB` only where they proxy target identity.
Replace those checks with `OS`/`TARGET` comparisons. Preserve genuine feature
and runtime capability flags when they describe availability rather than the
selected platform.

## Public using-import identity

A public `using` re-export preserves the original declaration and type identity
through every facade. If `button.zi` publicly imports `button_props.zi` as
`PropsModule`, its public namespace exposes that original type as `Button.Props`;
`Button.Props.{...}` and arguments declared as `PropsModule.Props` inside
`Button` refer to the same canonical type even when a consumer has another
local `PropsModule` or another `Props` type.

Implement the smallest general resolver/use-site qualification correction. Do
not add a Kryon-specific case, compatibility alias, copied facade type, or
implicit structural conversion. Ambiguous public re-exports remain errors and
private imports remain private.

## Nested package exports

Quoted `[exports]` keys may be slash-separated identifier namespaces. A direct
dependency export such as `"host/desktop"` resolves as
`#import "kryon/host/desktop"`, with the dependency alias as the first segment.
The same exact mapping must survive source project loading and saved `.zir`;
slashes organize the public package namespace and do not expose private files.

## Implementation surface

- Frontend target environment and typed compile-time evaluation:
  `cmd/zir/zir_parse_typed.c`, `cmd/zir/zir_parse_condition.c`,
  `cmd/zir/zir_parse_internal.h`, `cmd/zir/zir_check_value.c`,
  `cmd/zir/zir_check_expr.c`, `cmd/zir/zir.c`, `cmd/zir/zir.h`.
- Backend selection and propagation: `cmd/zir-c/main.c`, `cmd/zir-go/main.c`,
  `cmd/zir-ir/main.c`, `cmd/zir-wasm/main.c`, `cmd/zir-zx/main.c`, and
  `Makefile` only where needed.
- Portable declarations and target-proxy cleanup: affected files under `std/`.
- Nested export validation and loading: `cmd/compiler_declaration.zi`,
  `cmd/package_manifest.zi`, `cmd/zir/zir_load.c`, and the generated declaration
  bootstrap artifact.
- Regression coverage: `tests/type_exports_cases.zi`,
  `tests/type_exports_test.c`, and focused target/package fixtures and runners
  under `tests/`.
- Documentation: `docs/USAGE.md`, `docs/LANGUAGE_DIRECTION.md`, and
  `docs/JAI_PARITY_ROADMAP.md`.

## Required local tests

- All backends: `.C`, `.GO`, `.WASM`, `.ZX`.
- Every `OS` member and supported `CPU` member, including `.NONE` behavior.
- Cross-target host OS/CPU differing from target OS/CPU.
- `OS`/`CPU`/`TARGET` in `#if`, `#ifx`, `#assert`, and `#run`, including
  `#if TARGET == .C && OS == .LINUX`.
- Clear `#assert false, "..."` diagnostics and unloaded inactive imports.
- Portable `.zir` uses `.ZX`/`.NONE`/`.NONE` unless a concrete target is
  selected.
- Facade construction and argument passing preserve the canonical public
  re-exported type even with a consumer-local same-named type.
- Quoted nested package exports resolve in project source and saved `.zir`
  builds, including the lowercase `kryon/host/{desktop,web,android,terminal}`
  surface.

Run focused retained checks and then both full applicable suites, with display
variables removed and every build routed through the shared queue:

```sh
env -u DISPLAY -u WAYLAND_DISPLAY build-queue -- make check CHECK_FILTER=<focused-test-name>
env -u DISPLAY -u WAYLAND_DISPLAY build-queue -- make check
env -u DISPLAY -u WAYLAND_DISPLAY build-queue -- make check-bootstrap
```

Preserve unrelated dirty work. Do not commit or push this implementation.
