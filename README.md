# Ziran

Ziran 0.6.0 is a general-purpose language with direct native, WebAssembly,
portable ZX, C and Go output. Master includes Unreleased work.

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

```zi
main :: () { print("Hello, World!\n") }
```

## Tools and formats

Source → ZIR → LIR → optimized LIR → ZX → native (ELF/PE, x86-64/AArch64/ARMv7). WebAssembly lowers from verified ZX through `zx2wasm`. C and Go export from ZIR through `zi2c` and `zi2go`.

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

- [Compiler and standard library guide](docs/USAGE.md)
- [Packages and projects](docs/PACKAGES.md)
- [Architecture](docs/ARCHITECTURE.md)
- [Language reference](docs/LANGUAGE_DIRECTION.md)
- [Standard library and package APIs](https://ziran-lang.org/modules.html)
- [Examples](https://ziran-lang.org/examples.html)

Kryon is a separate UI library imported by applications. Console programs have
no graphics dependency. Release history lives in [CHANGELOG.md](CHANGELOG.md).

## Development

Run `make check` for the retained local suite and `make check-bootstrap` for
bootstrap reproducibility. `CHECK_FILTER=text` selects matching tests.
The compiler uses maintained Ziran and C sources; bootstrap seeds live in
`bootstrap/`. Refresh affected seeds with `make update-bootstrap` after compiler
changes.
