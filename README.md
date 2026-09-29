# Ziran

Ziran is a general-purpose language that compiles to C, C++, Go, Rust, and
Python. The toolchain checks source, saves checked modules, and builds portable
programs you can run without a native compiler.

[Website & playground](https://ziran-lang.org/) · [Compiler guide](docs/USAGE.md) · [Implementation status](docs/IMPLEMENTATION_STATUS.md)

The compiler and portable runner are under active development. Language
coverage and target limits are listed in the implementation status.

## Try it

```jai
main :: () {
    print("Hello, World!\n");
}
```

Edit and run this example in the [browser playground](https://ziran-lang.org/).

## Quick start

You need Make, a C11 compiler, and GNU binutils. From the repository root:

```sh
make
build/bin/ziran bundle --root site/examples --entry hello:main \
  -o build/hello.zib site/examples/hello.zi
build/bin/ziran run build/hello.zib
```

This prints `Hello, World!`. To install the `ziran` command for your user, run
`make install-user` and add `~/.local/bin` to your `PATH`.

## Tools and formats

Use `build/bin/ziran` from the checkout, or `ziran` after installation.

| Command | Purpose |
| --- | --- |
| `check` | Validate source and imports. |
| `build` | Generate C, C++, Go, Rust, or Python output. |
| `ir` / `inspect` | Save checked modules and read their contents. |
| `bundle` / `run` | Build and run portable programs. |
| `fmt` | Format source files. |
| `guide` / `features` / `capabilities` | Explore syntax, support, and target limits. |
| `api` | Query public declarations as JSON. |

Source files use `.zi`, checked modules use [`.zir`](docs/ZIR.md), and portable
programs use [`.zib`](docs/ZIB.md). Native targets and the portable runner have
different limits; the Plan 9 C target is experimental.

## Standard library

The library includes text and UTF-8 helpers, generic values, sorting, queues,
JSON scanning, ZIP archives, and explicit HTTP and process capabilities.
Native Linux adapters provide file, socket, timer, and other system operations.

See the [standard library reference](https://ziran-lang.org/stdlib.html) and
[compiler guide](docs/USAGE.md#standard-library) for imports and platform limits.

## Documentation

| Start here | What it covers |
| --- | --- |
| [Compiler guide](docs/USAGE.md) | Commands, imports, foreign functions, and standard modules. |
| [Examples](https://ziran-lang.org/examples.html) | Small programs with source and output. |
| [Implementation status](docs/IMPLEMENTATION_STATUS.md) | Supported features and remaining work. |
| [Architecture](docs/ARCHITECTURE.md) | Compiler, runtime, and library boundaries. |
| [Language direction](docs/LANGUAGE_DIRECTION.md) | Language design and tooling goals. |
| [Jai parity roadmap](docs/JAI_PARITY_ROADMAP.md) | Planned language work. |
| [Benchmarks](bench/README.md) | Compiler and runtime measurements. |
| [Migration](docs/MIGRATION.md) | Repository migration details. |

Kryon is a separate UI library that applications import explicitly.

## Development

Run `make check` for the language, backend, and portable runtime checks.
Tests use four workers by default; set `CHECK_JOBS=1` to run them serially.
