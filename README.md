# Ziran

Ziran is a general-purpose language with generated C and Go targets and a
portable executable format. The toolchain checks source, saves checked
modules, and builds portable programs you can run without a platform compiler.

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
build/bin/ziran run examples/hello.zi
```

This prints `Hello, World!`. To install the `ziran` command for your user, run
`make install-user` and add `~/.local/bin` to your `PATH`. Then start a
project from a template and run it:

```sh
ziran new hello          # or --template lib, or a package's template
cd hello
ziran run -- Ada         # Hello, Ada!
```

See [packages and projects](docs/PACKAGES.md) for templates, dependencies,
and `ziran build`/`install`.

## Tools and formats

Use `build/bin/ziran` from the checkout, or `ziran` after installation.

| Command | Purpose |
| --- | --- |
| `check` | Validate source and imports. |
| `build` | Generate C or Go output. |
| `ir` / `inspect` | Save checked modules and read their contents. |
| `bundle` / `run` | Build and run portable programs; `run --target=c FILE.zi -- ARGS` runs C-generated scripts. |
| `fmt` | Format source files. |
| `guide` / `features` / `capabilities` | Explore syntax, support, and target limits. |
| `api` | Query public declarations as JSON. |

Source files use `.zi`, checked modules use [`.zir`](docs/ZIR.md), and portable
programs use [`.zx`](docs/ZX.md). Direct-native targets and the portable runner
have different limits.

The experimental direct pipeline is source → `.zir` → linked LIR →
verification → LIR optimization → portable `.zx`. Those verified ZX bytes run
in the portable interpreter
or, on an explicitly supported direct target, lower to machine code and a
platform binary. That direct-native route is experimental and incomplete; its
current gaps are tracked in [the assembly plan](ASSEMBLY/milestones.md).
Generated C and generated Go are separate routes that also produce platform
binaries after their own downstream toolchains run. “Native” in the assembly
documentation means verified ZX instructions lowered directly to machine code,
not a C- or Go-generated binary.
Default `ziran`/`zi2zx` semantic bundles and the semantic-ZIR Wasm route have
not yet cut over to this instruction pipeline; see [ZX status](docs/ZX.md).

The opt-in Linux x86-64 `zi2asm` path now has a first console veneer: its static
ELF owns `_start`, retries partial or interrupted `write` syscalls, and exits
the process with `exit_group`. It links no libc or CRT. Literal `print` formats
cover byte strings/text views, bools, every fixed signed and unsigned integer
width, and `%%`; floats and unavailable platform services are named hard gaps.
This implemented slice does not make the wider direct-native target matrix
complete.

Host capabilities describe portable, typed services a program requires.
Platform bindings implement those services for a host; they are separate from
CPU instruction lowering and from each target's calling convention or syscall
veneer.

Run source as a C-generated script with `ziran task.zi arguments`. Executable
Unix scripts can start with `#!/usr/bin/env ziran`; after `chmod +x task.zi`,
run them as `./task.zi arguments`. See
[C-generated scripts](docs/USAGE.md#c-generated-scripts).

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

Run `make check` for the complete lean language, backend, and portable runtime
suite. Pruning removed 263 redundant check scripts and their unused helpers.
Workers follow the build queue's allocation; set `CHECK_JOBS=1` to run serially.
The check controller and its regression tests are written in Ziran.

The native command and formatter are written in Ziran. Compiler scanners,
declaration, foreign binding, and `using` modifier syntax, enum validation and
evaluation, type spelling and field parsing, expression and nested initializer
grammar, statement expression extraction, loop headers, optional `then`, and
compile-time branch syntax are also written in Ziran. Ziran also parses multiple
results and bindings, normalizes procedure-type parameters, expands formatted
builder statements, and rewrites
symmetric operators, procedure-name expressions, and local procedure names.
The rest of the compiler is still being migrated from C.
A fresh build uses the generated modules in `bootstrap/` to build a bootstrap
compiler, then compiles their maintained `cmd/compiler_*.zi` sources for the
ordinary tools. Run
`make check-bootstrap` to verify that the generated seed matches the source.
After changing a compiler module, use `make update-bootstrap` to refresh its seed.
