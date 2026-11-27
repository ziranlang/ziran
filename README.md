# Ziran

Ziran is a general-purpose language with direct native, WebAssembly, portable
ZX, C and Go output. Master includes Unreleased work; the current version is
in [VERSION](VERSION) and [CHANGELOG.md](CHANGELOG.md).

[Website and playground](https://ziran-lang.org/) · [Compiler guide](docs/USAGE.md) · [Current support](docs/IMPLEMENTATION_STATUS.md)

## Quick start

Build with Make, a C11 compiler and GNU binutils:

```sh
make
build/bin/ziran check examples/hello.zi
build/bin/ziran run examples/hello.zi
build/bin/ziran build examples/hello.zi
```

`run` checks and executes `main`; `build` defaults to a direct native binary.
`make install-user` installs the tools in `~/.local/bin`.
The same `check`, `ir`, `build` and `run` workflow handles `.zi`, `.ts` and
`.tsx` sources together; `ziran ir --project -o build/ir` saves their common
checked IR. See the [native TypeScript subset](docs/NATIVE_TYPESCRIPT.md).

```zi
main :: () { print("Hello, World!\n") }
```

## Tools

Source → ZIR → LIR → optimized LIR → ZX → native (ELF/PE, x86-64/AArch64/ARMv7).
WebAssembly lowers from verified ZX through `zx2wasm`; C and Go export from ZIR
through `zi2c` and `zi2go`.

| Command | Purpose |
| --- | --- |
| `check` | Validate source and imports. |
| `build` | Build native output, or select `--target=c`, `go`, `wasm` or `zx`. |
| `ir` / `inspect` | Save checked `.zir` modules and inspect artifacts. |
| `bundle` / `run` | Link `.zx` executables and run source or ZX. |
| `learn` | Control native-build corpus recording and offline C-teacher verification/training. |
| `fmt` | Format source. |
| `guide` / `features` / `capabilities` | Query syntax and target limits. |
| `api` | Query checked public declarations. |

Safety is manual by default, as in C. `--checks` opts a build into runtime
checks; `#checks` keeps bounds checks in selected source regions. See
[checks](docs/USAGE.md#checks) for target-specific behavior.

## Documentation

| Read | When you need |
| --- | --- |
| [docs/USAGE.md](docs/USAGE.md) | The compiler and standard library guide |
| [docs/IMPLEMENTATION_STATUS.md](docs/IMPLEMENTATION_STATUS.md) | Supported targets and limits |
| [docs/PACKAGES.md](docs/PACKAGES.md) | Packages, projects, and lock files |
| [docs/NATIVE_TYPESCRIPT.md](docs/NATIVE_TYPESCRIPT.md) | Native `.ts` / `.tsx` syntax, imports and current limits |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | The compiler pipeline |
| [docs/LANGUAGE_DIRECTION.md](docs/LANGUAGE_DIRECTION.md) | Language syntax and directives |
| [docs/OWNED_VALUES.md](docs/OWNED_VALUES.md) | Owned values and recoverable failures |
| [docs/DIAGNOSTICS.md](docs/DIAGNOSTICS.md) | Diagnostic codes and JSON output |
| [docs/AUTHORING.md](docs/AUTHORING.md) | Tool-driven editing workflows |
| [docs/LAWS.md](docs/LAWS.md) | Laws and proof certificates |
| [docs/ZX.md](docs/ZX.md) | The portable ZX format |
| [bench/README.md](bench/README.md) | Running benchmarks |
| [plans/open-work.md](plans/open-work.md) | What is still open |

Standard library and package APIs: [ziran-lang.org/modules.html](https://ziran-lang.org/modules.html).

Kryon is a separate UI library imported by applications; console programs have
no graphics dependency. Repository rules for agents live in
[AGENTS.md](AGENTS.md).

## Development

Run `make check` for the retained local suite and `make check-bootstrap` for
bootstrap reproducibility. `CHECK_FILTER=text` selects matching tests. The
compiler uses maintained Ziran and C sources; bootstrap seeds live in
`bootstrap/`. Refresh affected seeds with `make update-bootstrap` after
compiler changes.
