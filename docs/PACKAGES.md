# Packages and project imports

Ziran packages are Git repositories. There is no central registry: a
dependency is the URL of the repository that holds it, pinned to a full commit
in a committed `ziran.lock`. A Ziran application is itself a package with a
`ziran.toml`. `make install-user` installs the native `ziran` command; project
commands build and use the compiler pinned by the application lock, and hand
the command to that toolchain's own launcher, so an installed `ziran` that is
older or newer than the lock behaves exactly like the pinned one.

## Starting a project

`ziran new DIR` writes a project from a template and locks it:

```sh
ziran new hello                        # a command-line program (template cli)
ziran new geometry --template lib      # a library with one exported module
ziran new calc --template kryonlabs/kryon          # a package's first template
ziran new calc --template kryonlabs/kryon:tui      # one of its named templates
ziran new calc --template ../my-templates:service  # a local package directory
```

`cli` and `lib` ship with Ziran. Any package can offer templates by listing
directories in its `ziran.toml`; the first one is the default:

```toml
[templates]
app = "templates/app"
tui = "templates/tui"
```

A template directory holds a `ziran.toml` and whatever files a project starts
with. In file contents and paths, `{{name}}` becomes the package name and
`{{module}}` the same name as an identifier (`my-tool` becomes `my_tool`).
`--name NAME` sets the name when the directory's name is not one, and
`--ref REF` fetches the template from another branch, tag, or commit.

`ziran init --template SOURCE` applies a template to the project in the
current directory. It never overwrites a file. When the project already has a
`ziran.toml`, the template's manifest is merged into it: missing tables and
keys are added, string arrays such as `module_roots` gain the template's
entries, and every other value the project already sets is kept and reported.
That is how an existing command-line program becomes, for example, a Kryon
application: the template adds the dependency, `tool = "kryon"`, and the
`[tool.kryon]` settings, and keeps the program's own entry.

## Building and running a program

In a project directory, `run`, `build`, `check`, and `install` without a file
work on the project's program:

```sh
ziran run                 # build, then run the entry module's main
ziran run -- one two      # arguments after -- go to the program
ziran build               # build/NAME
ziran check               # check the entry, or a library's exports
ziran install             # copy the program to ~/.local/bin/NAME
```

The program is the `main` procedure of `[package] entry`, which takes nothing
or `(argc: s32, argv: **u8)` and returns nothing or its exit status. Ziran
writes C and compiles it with `CC` (default `cc`), honoring `CFLAGS`,
`LDFLAGS`, and `LDLIBS`. `[package] links` names system libraries to link:

```toml
[package]
name = "fetcher"
entry = "src/main.zi"
links = ["curl"]
```

`ziran install --prefix DIR` installs under `DIR/bin`; `[install] bin` changes
the command name. The copy replaces an installed program in one step.

A project whose `[package] tool` names a dependency hands these four commands
to that dependency's project tool instead (see below). Naming a file, as in
`ziran check --project src/cell.zi`, or passing `--target`, keeps the command
on that file or target.

## Adding a dependency

Pass any spelling of the repository to `ziran add`:

```sh
ziran add https://github.com/kryonlabs/plot.git
ziran add git@github.com:kryonlabs/plot.git
ziran add ssh://git@codeberg.org/owner/repo.git
ziran add codeberg.org/owner/repo
ziran add kryonlabs/plot            # OWNER/REPO means GitHub
```

The dependency is named after the repository (`plot`; `kryon-ui` becomes
`kryon_ui`). Use `--name NAME` for another name, `--ref REF` for a branch, tag,
or full commit instead of `master`, and `--source` for a repository that is not
a Ziran package. `ziran add` writes the declaration, locks it, and prints the
modules you can import:

```toml
[dependencies.plot]
git = "git@github.com:kryonlabs/plot.git"
ref = "master"
```

HTTPS and SSH name the same package. Ziran identifies a repository by its
canonical HTTPS spelling, so switching a manifest between the two keeps the
lock valid, and two packages that reach one repository over different
transports share a single locked copy. Ziran fetches over the transport the
manifest names; when that fails, for example SSH without a key in CI or HTTPS
without credentials for a private repository, it retries once over the other
transport. It never prompts for a password. URLs with embedded credentials are
rejected.

## Importing modules

A dependency's exported modules are imported by the dependency name and the
module name, so every import says where it comes from:

```zi
#import "plot/Plot"
using Widgets :: #import "kryon/Widgets";
#import "std/text";
```

`std/NAME` is the pinned standard library. Modules in your own package use
their short names. `ziran pkg list` shows each direct dependency, its URL,
ref, commit and checkout, and every import path it provides:

```text
plot  git@github.com:kryonlabs/plot.git  master  3e1433d0b2c1
  checkout /home/me/.cache/ziran/sources/p1f0c…
  #import "plot/Plot"                src/module.zi
std  /home/me/.cache/ziran/sources/p5a7…/std
  #import "std/NAME"               the standard library
```

A short import such as `#import "Plot"` also works. A short name resolves in
the importing package first, then among its direct dependencies' exports, then
in the standard library. When two direct dependencies export the same name,
the short import reports both qualified spellings; import one of them, or
rename one with `module_aliases`:

```toml
[dependencies.Graphics]
git = "https://example.com/graphics.git"
ref = "master"
module_aliases = { Drawing = "GraphicsDrawing" }
```

Only the dependency's `[exports]` are visible. Its other modules are private
to it, so their names never collide with yours.

## Publishing a package

An exported module is a package-owned file. Its public name may differ from
the file name, so a package can keep the short path `src/module.zi`:

```toml
[package]
name = "Plot"
module_roots = ["src"]

[toolchain]
git = "https://github.com/ziranlang/ziran.git"
ref = "master"

[exports]
Plot = "src/module.zi"
```

Push the repository anywhere Git can reach. Prefer HTTPS URLs in published
manifests so fresh clones and CI can fetch without an SSH key.

Package modules receive an internal identity derived from their repository
and commit. Two versions of a transitive dependency can coexist in one build.
The identity is carried through checked IR, saved `.zir`, portable bundles,
and native output names.

## Commands

```sh
ziran add URL|OWNER/REPO
ziran lock
ziran update
ziran update plot
ziran update ziran
ziran fetch
ziran pkg list [plot]
ziran pkg path plot --locked
ziran check --project
ziran ir --project --entry app:main -o build/ir
ziran build --project --target=c --entry app:main -o build/c
```

`ziran update` refreshes every package and the toolchain to their current refs.
`ziran update plot` refreshes Plot and its transitive dependencies while
keeping unrelated packages pinned. `ziran update ziran` refreshes only the
compiler toolchain.

`--locked` requires a matching lock and ignores local development overrides;
`--offline` requires cached checkouts. The cache is under
`$XDG_CACHE_HOME/ziran` or `~/.cache/ziran`.
`ziran pkg path` returns the checkout selected by the lock, including a unique
transitive dependency. Add `--submodules` when a platform build needs its
vendored backend source. The command initializes the submodules at the commits
recorded by that package; with `--offline`, it requires them to be present.

A platform build can also pin a repository that is not a Ziran package, such
as a C library it compiles from source. Mark the dependency with
`source = true`:

```toml
[dependencies.sqlite]
git = "https://github.com/sqlite/sqlite.git"
ref = "master"
source = true
```

`ziran add URL --name sqlite --source` writes the same declaration. A source
package is locked, fetched, and cached like any other dependency, and
`ziran pkg path sqlite --locked` returns its checkout, but it has no
`ziran.toml`, exports no modules, and cannot declare dependencies or module
aliases. Build scripts use that path instead of a `vendor/` submodule.

For local development, an ignored `ziran.local.toml` can map package names to
working directories:

```toml
[overrides]
Kryon = "../kryon"
ziran = "../ziran"
```

Production builds omit overrides and use the locked Git commits. A host module
that calls into the application can use an explicit reverse bridge:

```toml
[package]
bridge_modules = ["app"]
```

This makes only the named app module visible to direct dependencies. It does
not change the package's public exports. Native system libraries remain
ordinary platform build dependencies.

## Project tools and their options

A package can ship a project tool, such as the command that builds an
application with a UI library. It names the tool and declares the options an
application may set for it in its own `ziran.toml`:

```toml
[tool]
project = "build/bin/kryon"

[options]
default_profile = "string"

[options.profiles."*"]
backend = "string!"
codegen = "string=c99"

[options.install]
autostart = "bool=false"
```

`[options]` holds top-level keys, `[options.NAME]` one table, and
`[options.NAME."*"]` any number of named tables. Each value is `"string"` or
`"bool"`, followed by `!` when it is required or `=DEFAULT` for a default.

The application sets those options under the dependency's alias, and its
ignored `ziran.local.toml` may replace any of them on one machine:

```toml
[tool.kryon]
default_profile = "desktop"

[tool.kryon.profiles.desktop]
backend = "desktop"
```

`ziran tool kryon COMMAND` rebuilds the tool if its sources changed, checks
every setting against the declaration, and reports an unknown or missing key
with its file and line. It then runs the tool from the project root. The tool
reads the merged settings as `KEY=VALUE` lines from the file named by
`ZIRAN_TOOL_OPTIONS`, for example `default_profile=desktop`,
`profiles=desktop` and `profiles.desktop.backend=desktop`, so no tool parses
TOML itself. `ZIRAN_PROJECT_ROOT`, `ZIRAN_PROJECT_NAME`,
`ZIRAN_PROJECT_ENTRY`, `ZIRAN_PACKAGE_ROOT`, `ZIRAN_PACKAGE_ID` and
`ZIRAN_TOOLCHAIN_ROOT` describe the project and the tool's checkout.

## Handing a project to its tool

`[package] tool` names the dependency whose project tool runs, builds, checks,
and installs the application, and `[install] bin` the command name it
installs as:

```toml
[package]
name = "example"
entry = "src/app.zi"
tool = "kryon"

[install]
bin = "example"
```

`ziran run`, `ziran build`, and `ziran check` without a file then run
`ziran tool kryon run`, `build`, or `check` with the remaining arguments, so
`ziran run desktop` is `ziran tool kryon run desktop`. `ziran install` runs
the tool's `install` command with `ZIRAN_INSTALL_PREFIX` (`~/.local` unless
`--prefix DIR` is given) and `ZIRAN_INSTALL_BIN`; other arguments are passed
on. The tool decides what else an install includes, such as a desktop entry,
from its own options. The older `[install] tool` key is rejected with a
message naming its replacement.
