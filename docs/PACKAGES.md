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

`--overrides FILE` copies a validated local configuration into the new
project's ignored `ziran.local.toml` before locking. Use it with a local
template and overrides for the toolchain and dependencies to build against
existing source checkouts without fetching copies. Relative override paths
are relative to the created project. An existing `ziran.local.toml` is never
overwritten, including when applying a template with `init`.

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
Normal `fetch` and `pkg path` commands also validate cached tracked payloads.
If an immutable source-cache entry has missing files or staged deletions only,
they restore the locked source from existing Git objects, including with
`--locked --offline`; no separate repair command or network access is needed.
A matching HEAD alone does not establish that the source is ready. Modified
tracked files, or untracked/ignored data alongside missing files, prevent repair
and produce a `package cache ... preserved at` diagnostic. Failed entries are
never automatically deleted. Redirected, sparse or flagged checkouts are refused;
explicit local overrides remain user-owned and are never repaired this way.

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
kryon = "../../kryonlabs/kryon"
ziran = "../../ziranlang/ziran"
```

Each override is the top of its own Git checkout: `ziran lock` records the
commit checked out there. A directory inside another repository, such as an
uninitialized submodule, is refused, because its HEAD would be the enclosing
repository's commit. A root dependency's override selects that source for
all incoming edges, including transitive dependencies with different aliases
or exact revision pins. These edges use one actual checkout and module type
identity. The lock records each edge's declared ref separately, so changing a
ref, URL, alias, source kind or dependency inventory still requires `ziran lock`.
Source-only edits and newer local commits can be used without changing those
declarations; build commands derive local module identities from the actual
checkout HEAD. Conflicting overrides for one source are refused.

After removing overrides, run `ziran lock` to resolve the declared revisions
again. Exact pins select their declared commits rather than retaining a newer
local override commit. `--locked` refuses an override-only ref substitution.

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

## Dependency-owned native artifacts

A dependency can declare named artifacts alongside its existing project tool:

```toml
[tool]
project = "build/bin/provider"

[artifacts.native]
inputs = ["tools/provider.zi", "src/library.c", "Makefile"]
outputs = ["lib/library.a", "include/library.h", "resources/data.bin"]
requires = ["support:native"]
```

`inputs` and `outputs` are nonempty arrays of package-relative input files and
staging-relative output files. List all recipe inputs, including ignored native
source inputs, configuration, assets and tool build rules; directories and
escaping paths are rejected. `artifact.tsv` and `dependencies.tsv` are reserved.
`requires` is optional and names artifacts through the provider's own locked
dependency aliases; `self:NAME` names another recipe in the same package.
Unknown selections and recipe cycles fail before invoking the affected provider.

Recipes derive outputs only from declared inputs, dependency artifacts, the
platform, resolved options and keyed toolchain/environment. Consumer identity
is not an input; encode consumer-specific variation as a declared option.
The current host command needs `make`, `git`, `mktemp`, `which` and `sha256sum`.

Consumers request the recipe without reconstructing its source or link graph:

```sh
ziran pkg artifact provider:native --platform linux-x86_64-gnu
ziran pkg artifact provider:native --platform windows-x86_64-mingw --locked
```

`--platform` is a required explicit OS-architecture-ABI identifier, independent
of the compiler backend (`--target=c`, `go`, or `py`). The provider must validate
supported platforms and choose the correct native toolchain. The engine treats
libraries, bundles and resources identically. This command does not alter
project lifecycle delegation or explicit compiler target selection.

Per-invocation options override `[tool.Alias]` and `ziran.local.toml` values
in memory, using the dependency's existing declared option types:

```sh
ziran pkg artifact provider:native --platform linux-x86_64-gnu \
  --option provider:mode=release \
  --option 'support:sdk.path=/opt/SDK folder/tool=chain' \
  --option provider:enabled=true
```

Repeat `--option Alias:key=value` after the platform. `key` is a top-level key,
`group.key`, or `group.name.key` for a declared named table. Alias matching is
case-insensitive and resolves to a locked package identity, so overrides also
reach prerequisite recipes (including `self` recipes). A prerequisite uses its
root consumer alias when present, otherwise its locked package name; the
provider's `requires` edge still uses the provider's dependency alias.
Strings are nonempty literal argv values, including spaces, quotes, backslashes
and additional `=` characters. Boolean values are exactly `true` or `false`.
There is no TOML parsing or shell evaluation of invocation values. Quote the
whole argument as required by your shell. Control characters, unknown or
ambiguous packages, undeclared keys, malformed paths and duplicate keys for the
same package are rejected before compiler identification, cache lookup or any
provider runs. Provider-specific value constraints remain the provider's job.
New named tables are ordered deterministically, so reordering invocation
arguments does not change the effective option file or cache identity.
Manifests, local overrides and lock files are never edited by these options.
Artifact providers receive atomic, content-addressed option snapshots so
concurrent invocations cannot overwrite one another's effective values.

The ordinary project tool is built through its existing Makefile and invoked as
`artifact NAME --platform TARGET --output STAGE` with the existing project,
package, toolchain and resolved `ZIRAN_TOOL_OPTIONS` environment. Additional
variables are `ZIRAN_ARTIFACT_OUTPUT`, `ZIRAN_ARTIFACT_TARGET`, and
`ZIRAN_ARTIFACT_DEPENDENCIES`. The last names a TSV file with one
`dependency:artifact<TAB>absolute-descriptor-path` line per requirement. Providers
read those descriptors instead of rebuilding dependencies. They write every
declared output below STAGE, send diagnostics to stderr, return nonzero on
failure, and honor serial build settings (`MAKEFLAGS=-j1` and
`CMAKE_BUILD_PARALLEL_LEVEL=1`). Invocation uses argv, never shell interpolation.

On success stdout contains the absolute path of `artifact.tsv` in the shared
Ziran artifact cache. Its TSV header records `schema`, `package`, `artifact`,
`platform`, and `key`; remaining lines map declared relative output paths to
SHA-256 digests. Resolve each output beside the descriptor. Descriptors and
outputs are published together by directory rename only after all declared
files exist and remain inside staging. A failed build removes staging and
preserves previously published results. Cache hits verify output digests.

### Python build-tool consumers

The resolved toolchain provides `scripts/ziran_artifacts.py`. Build scripts
can load it from the directory returned by their package wrapper's
`pkg path ziran`; they do not need a copied descriptor parser. For example,
with `tool` naming that wrapper and `project` the consuming project directory:

```python
import importlib.util
from pathlib import Path
import subprocess

toolchain = Path(subprocess.check_output(
    [tool, 'pkg', 'path', 'ziran'], cwd=project, text=True).strip())
spec = importlib.util.spec_from_file_location(
    'ziran_artifacts', toolchain / 'scripts/ziran_artifacts.py')
artifacts = importlib.util.module_from_spec(spec)
spec.loader.exec_module(artifacts)
result = artifacts.request('provider:native', 'linux-x86_64-gnu',
                           project=project, ziran=tool)
library = result.output('lib/library.a')
```

`request` forwards repeated `options` as literal `--option` arguments and
uses `--locked` when the project has no `ziran.local.toml`. Its optional `env`
is passed to the selected tool unchanged. Callers own build resource limits
and display isolation. `Artifact(absolute_descriptor, artifact=..., platform=...)`
also consumes an already requested result. Construction verifies the schema,
identity, unique rows, canonical contained paths and every declared output's
SHA-256 digest. `output(name)` requires a declared output; `path(name)` resolves
a contained path such as a provider's include directory. Symlinks cannot
escape the descriptor directory.

`metadata(name)` reads a declared JSON object whose integer `schema` is `1`
and whose `artifact` and `platform` match the descriptor. Its remaining
fields belong to the provider. Renderer layouts, assets, Android editions
and application output names remain provider or application policy.

Providers may expose an ordered `link.txt` output for foreign native support.
Each row is `kind<TAB>value`; supported kinds are:

- `archive`: a declared archive output, passed as one argument.
- `library` or `flag`: one literal linker argument.
- `pkg-config`: shell-word-separated package names, queried with `--libs`.
- `js-library`: a declared output passed after `--js-library`.
- `embed`: a declared output and absolute virtual mount, joined with `@`,
  passed after `--embed-file`.

`result.link_flags()` expands those rows, followed by each prerequisite's
metadata in the order recorded by `dependencies.tsv`. Each prerequisite is
verified against its own output paths, artifact name and the same platform.
Cycles are rejected; repeated prerequisites on separate edges retain their
static link order. `env` also selects `PKG_CONFIG` and its environment. No
metadata or option is evaluated by a shell.

Use `link_flags(include_archives=False)` when ordinary imports already
compile this provider's source into the host. It omits only the current
provider's archives; its flags and prerequisite foreign archives remain.
For example, importing a Ziran library and linking its complete native
archive again would duplicate its module globals. A foreign C host instead
uses the default to link that archive once.

Identity includes the locked package graph and toolchain revisions, actual
checkout HEADs, launcher/compiler bytes, native platform/ABI, recipe and input
bytes, resolved options, dependency descriptors, CC executable bytes/version and
relevant native build environment (`PATH`, `CFLAGS`, `CPPFLAGS`, `CXX`,
`CXXFLAGS`, `LDFLAGS`, `LDLIBS`, `AR`, `LD`, `RANLIB`, `PKG_CONFIG`,
`PKG_CONFIG_PATH`, `PKG_CONFIG_LIBDIR`, `SYSROOT`, `SDKROOT`,
`MACOSX_DEPLOYMENT_TARGET`). `CC` must name one executable. Any dirty tracked or untracked
checkout in the resolved graph disables reuse; declared ignored inputs are
hashed regardless. Dirty builds receive unique publication directories, so
local edits cannot hit or overwrite clean results. Two independent consumers
with the same graph, options and environment share a clean artifact.

The package-driver entry point is
`PkgBuildArtifact(project: string, package: string, artifact: string, target: string) -> s32`.
For invocation overrides use
`PkgBuildArtifactWithOptions(project: string, package: string, artifact: string, target: string, overrides: []string) -> s32`.
Each slice element is the same `Alias:key=value` string as the CLI; an empty
slice has the original entry point's behavior. CLI dispatch passes its already
resolved graph to the same implementation.
Downstream adoption consists of adding provider-owned recipes and an `artifact`
command to the existing Ziran project tool, then replacing consumer source/link
reconstruction with this request and descriptor consumption. Platform-specific
sources, link decisions and assets stay in those providers.

Run the focused native fixture independently before adopting provider recipes:

```sh
env -u DISPLAY -u WAYLAND_DISPLAY TMPDIR="$PWD/work" python3 tests/package_artifacts.py build/bin/ziran
```

The regular `sh tests/packages.sh build/bin/ziran` runner includes this fixture.
It creates only disposable miniature input repositories under ignored `work/`.
