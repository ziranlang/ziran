# Packages and project imports

A Ziran application is a package with a `ziran.toml` and a committed
`ziran.lock`. The package command is compiled from Ziran source. Dependencies
and the compiler are fetched from public HTTPS Git
repositories and pinned to full commits in the lock. `make install-user`
installs the native `ziran` command; project commands build and use the
compiler pinned by the application lock.

```toml
[package]
name = "Example"
entry = "src/app.zi"
module_roots = ["src"]

[toolchain]
git = "https://github.com/ziranlang/ziran.git"
ref = "master"

[dependencies.plot]
git = "https://github.com/kryonlabs/plot.git"
ref = "master"
```

An exported module is a package-owned file. Its public name may differ from
the file name, so a package can keep the short path `src/module.zi`:

```toml
[exports]
plot = "src/module.zi"
```

Applications import the public entry by its module name:

```zi
using Charts :: #import "plot";
```

Each module resolves short imports in its own package, then its direct
dependencies, then the pinned standard library. Two direct dependencies that
export the same module name produce an error. Rename one at the dependency
declaration:

```toml
[dependencies.Graphics]
git = "https://example.com/graphics.git"
ref = "master"
module_aliases = { Drawing = "GraphicsDrawing" }
```

Package modules receive an internal identity derived from their exact Git
source and commit. Two versions of a transitive dependency can coexist in one
build. The identity is carried through checked IR, saved `.zir`, portable
bundles, and native output names. Source code continues to use short imports.

Commands:

```sh
ziran add kryonlabs/plot
ziran lock
ziran update
ziran update plot
ziran update ziran
ziran fetch
ziran check --project
ziran ir --project --entry app:main -o build/ir
ziran build --project --target=c --entry app:main -o build/c
ziran pkg path plot --locked
```

`ziran update` refreshes every package and the toolchain to their current refs.
`ziran update plot` refreshes Plot and its transitive dependencies while
keeping unrelated packages pinned. `ziran update ziran` refreshes only the
compiler toolchain. For other Git hosts, use
`ziran add NAME --git https://HOST/OWNER/REPO.git`.

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

`ziran add sqlite --git URL --source` writes the same declaration. A source
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
