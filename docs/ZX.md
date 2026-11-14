# Ziran portable bundle (`.zx`)

The file roles are `.zi` for source, `.zir` for checked intermediate modules,
and `.zx` for portable executables. The portable tool is `zi2zx`, and its
target name is `zx`. A `.zx` file is revision-28 executable ZX: verified
lowered instructions, the program's assets and the names its embedder sees.
The normative byte contract is
[the executable format specification](../ASSEMBLY/zx-executable-format.md); see
[Implementation status](IMPLEMENTATION_STATUS.md) for the remaining work.

## Running bundles

Run source in one command: `ziran run hello.zi` or `zi2zx hello.zi`. Both
check the file, write the bundle in memory and execute its `main`; its folder
is the default module root. `ziran run app.zx` and `zi2zx run app.zx` run a
bundle file. Zero-argument entries returning an integer, `bool` or nothing
run; the command prints a non-void result on its own line, unsigned results
over their full range.

Use `ziran run app.zx` for both console and graphical programs. The linker
includes the reachable code of imported modules, including a UI library.
Platform effects such as windows and input are supplied by an installed runtime
provider. Ziran discovers providers generically, without UI-library names or
widget-specific startup behavior. A console bundle with no required host
capabilities runs directly and never starts a graphical provider.

Packages install a `.host` registration containing one absolute executable path
under `share/ziran/hosts`. Discovery searches `$XDG_DATA_HOME/ziran/hosts`
(default `$HOME/.local/share/ziran/hosts`), then each `$XDG_DATA_DIRS` entry
(default `/usr/local/share:/usr/share`). Registrations are tried in filename
order within each directory. `ZIRAN_HOST_PATH` overrides these locations with
colon-separated absolute registry directories; empty and relative entries are
ignored. Bundle directories and embedded assets are never provider search roots.

A provider implements `--check FILE.zx`, returning zero only when it can supply
all required capabilities. Checking must not execute application initializers
or entries or open a window. The selected executable then receives `FILE.zx`.
Its exit status is the run status; an execution failure never retries the app
on another provider. Malformed bundles fail before discovery. Missing providers
produce a diagnostic before the application executes. Provider registration is
part of the library platform installation and adds no requirement to a
console-only Ziran install.

For example, Kryon `make install-user` installs its reusable desktop provider.
A graphical `.zx` then runs with `ziran run app.zx` without application sources,
a manifest or generated native output. The portable bundle carries its linked
UI code and embedded assets; the installed provider bridges the local OS.

## Writing bundles

`zi2zx bundle --root DIR [--module-path DIR] [--define NAME] [--asset-dir NAME=DIR] [--retain module:function] [--bind caller:capability=provider:function] [--bind-host MODULE] --entry module:function -o FILE file.zi|file.zir ...`
loads explicit inputs and their extensionless imports, lowers what the entry
reaches to instructions, verifies and optimizes them, and writes the bundle.
`zi2asm --emit zx` writes the same bytes, and source and saved `.zir` give the
same file:

```sh
zi2asm --root src --entry app:main --emit zx -o app.zx src/app.zi
zi2asm --input zx --emit elf -o app app.zx
zi2asm --input zx --run app.zx
zi2asm --input zx --emit lir -o - app.zx
zi2asm --input zx --emit wasm -o app.wasm app.zx
```

The required program section holds deterministic verified lowered
instructions, stripped of source names and instruction origins. A procedure's
`#asm` variants are lowered from its portable body, so a bundle holds no
machine code of its own. Native output from source serializes and reads this
same verified ZX boundary; source and ZX entry routes produce identical ELF
bytes. `--opt --emit lir` and `--opt --emit opt-report` inspect the pre-ZX
optimization stage; ZX input is never reoptimized.

Reading a bundle validates section framing, SHA-256 content identities, the
instruction decoding, the verifier, the entry identity and the reachable
symbol and capability set before anything runs. The input bytes may be released
after the read. A file of another revision is rejected; there is no fallback
reader. An edited program byte fails the identity check.

Reachable `#foreign host_api` calls become host capabilities, called by
`host.call`. A bundle may instead bind one such call to an exported Ziran
function among its input modules with `--bind`; the bound call runs inside the
same instance and is absent from the capability list. The optional host
interface section names, for each capability, the type spellings and record
field names of its parameters and result, its declaration as written, the
program's module globals and the entry's signature: what an embedder sees and
the instruction payload does not keep.

## Embedding

An embedding C host links `build/libziran.a`, includes `ziran_host.h`, and opens
a bundle with `BundleOpen` or `BundleOpenBytes` (`OpenBundle` and
`OpenBundleBytes` in `std/bundle_host`). It lists the required capabilities
with `BundleCapabilityCount`, `BundleCapabilityModule` and
`BundleCapabilityFunction`, and passes `HostBinding` entries to `BundleRun` or
`BundleInstantiate`. Every required capability needs exactly one binding before
any initializer or entry runs; missing or duplicate bindings fail first.
Instantiation copies the binding names and indexes them, so caller order does
not matter. Separate instances of one bundle may run concurrently; each owns its
globals, which persist across `BundleInstanceRun` calls. Global initializers run
when the instance opens.

Each host call is translated between the engine's host image and `VmHostValue`
trees by the host interface section. A value's type is the spelling declared at
its position, and a spelling that starts with `u` is unsigned; a `u64` above the
`s64` range reaches the host in `bits` alone. Returned records give every
declared field by name and in order, unless `BundleInstanceMatchFieldsByName`
lets a host built against another layout omit or add fields. Numeric, string
and record slice parameters cross as `VM_HOST_SLICE` with a mutable `elements`
array: the host keeps the count and pointer, and its changes are validated and
copied back into the caller's slice after a successful call. Two slice
arguments of one call may not share elements. Declared pointer types cross as
`VM_HOST_POINTER`: an opaque handle that bundles store, compare and pass back,
and that fails the run when the program reads through it. The host owns the
storage behind every handle it hands out.

A run is not bounded: like the program's native builds, it continues until the
entry returns. `BundleInstantiateLimited(bundle, bindings, count, max_steps)`
gives one positive step budget to global initialization and every entry run,
so a runaway initializer fails instantiation; `BundleInstanceLimitSteps`
bounds each later run on its own. Steps are function calls and loop
iterations. A runaway recursion stops with "nested calls filled the stack";
other run failures name what happened, such as an index out of bounds or
unreachable code executed, and the instruction where it did.

## Target contract

The pipeline is source → `.zir` → linked and verified instructions → `.zx`.
The portable instruction engine runs the instructions on any platform. A
supported direct target lowers the same verified instructions to machine code,
applies the target ABI and platform veneers, and writes a platform binary;
`zi2asm --input zx --emit elf` is the Linux x86-64 row. Generated C and Go are
separate routes, never native fallback.

The linker includes reachable code from ordinary libraries, Kryon only when
the program imports it. A non-UI command-line program is a first-class `.zx`;
it launches without a graphical host. Kryon UI code is portable Ziran code in
the bundle, with rendering and input supplied through declared host
capabilities. A portable bundle cannot contain an undeclared dependency on
target-specific C or Go code. Host capabilities name portable typed services
and are bound independently of CPU lowering; missing host bindings and missing
ABI veneers are different gaps and neither is repaired by the other layer.

## Component identity, state, and replacement

A component is an ordinary `.zx` entry plus its declared host capabilities;
Ziran has no UI- or application-specific component names. `BundleCodeIdentity`
returns the SHA-256 of the bundle's bytes, and `BundleEntryModuleName` and
`BundleEntryFunctionName` expose the entry identity. `BundleCapabilityAbiAt`
returns each capability's module, function, declaration, argument and return
text, varargs flag and argument-read flag. `BundleAbiCompatible` requires the
same entry identity and signature and the same capability ABI set in both
directions.

`BundleComponentOpen` retains the bundle, copies the host bindings, and creates
one independently initialized instance with a positive step budget. Handles
become stale when their generation is replaced or their component closes, and
running a stale handle fails without touching the current instance.

`BundleInstanceExportState` snapshots the module globals the interface section
names as recursive `BundleStateValue` data, by module, name and declared type.
Strings, fixed arrays, records, enums and vectors are deep-copied; pointers,
slices and other borrowed or host resources are rejected. Snapshots are bounded
in depth and by 16 MiB of copied payload, which `BundleStateValueBytes`
reports. A migration callback receives the old snapshot and a mutable snapshot
of the replacement's initialized state, then `BundleInstanceImportState`
converts every value before changing any global.

`BundleComponentReplace` is transactional. It checks the ABI, initializes the
replacement under the component budget, exports both states, migrates and
imports, and calls the optional verification callback while the old instance
remains live. Only when every step passes does it close the old instance and
advance the handle generation; any failure leaves the previous instance and
bundle live. `BundleComponentLiveValueBytes` reports the instance's global and
owned value storage, not shared code, host allocations or process RSS.
`tests/bundle_step_limit.sh` covers migration, every rollback, stale handles and
typed-ABI rejection.

## WebAssembly execution tier

A host that installs a WebAssembly provider with `ZxWasmSetPlatform`
(`cmd/zir/zir_zx_wasm_platform.h`) runs bundles as WebAssembly:
`BundleInstantiate` lowers the bundle's verified program with
`zx_wasm_lower`, instantiates it through the provider, runs its module
initializers, and charges the run budget like the instruction engine. An
Emscripten build of `libziran` installs the browser provider itself; set
`ZIRAN_WASM_TIER=0` to leave bundles to the instruction engine. Each instance
keeps its own bindings and linear memory. Host calls and module state cross as
host images, which the embedding API translates to `VmHostValue` trees by the
host interface section, so typed capabilities, records, strings, slices and
state export and import behave as on the engine. A program outside the
lowering, or a module the provider rejects, fails instantiation; no other
engine starts. A web build needs `libziran` built with the instruction engine,
that is without `BOOTSTRAP=1`. `tests/zx_wasm.sh` runs bundles on the tier
under node beside the engine, and `tests/zx_web_lifecycle.sh` runs source
through `.zx` in a private headless Chromium.

`zi2asm --emit wasm` lowers the same verified ZX to one wasm32 module, and
`ziran build --target=wasm` runs it; source and saved-ZX input give identical
bytes. The module imports `zx.overrun` and the mutable `zx.budget` global that
every `fuel` instruction charges, `zx.host_i64` when the program prints and
`zx.host` when it calls a host capability. It exports `zx_entry`, `memory`,
`zx_initialize` when it has module initializers and `zx_reserve` when it has a
heap; a `zxhost` custom section names each capability. A form it has no code
for stops at a named `wasm.gap.*` diagnostic. The host-call record layout is in
`cmd/zir-wasm/zx_wasm_lower.h`.

## Embedded files and application hosts

`--define NAME` selects the same source branches as the native compiler.
`--asset-dir assets=assets` embeds regular files recursively under `assets/`.
The argument's left side is the path prefix inside the bundle, and its right
side is the source directory or file. Multiple mappings may be supplied.
Names are sorted bytewise for deterministic output. Duplicate names, symlinks,
absolute paths, backslashes, empty components, and `.` or `..` components are
rejected. The asset section is part of the bundle's content identity.

`BundleAssetCount`, `BundleAssetName`, `BundleAssetData` and `BundleAssetSize`
expose the immutable files until `BundleClose`; `std/bundle_host` wraps them
for Ziran hosts. Opening a bundle needs no temporary files.

Kryon's reusable desktop player supplies rendering and input to a ZX instance;
the application's widgets and state run inside it. A downloaded graphical
program launches with `ziran run app.zx` after the desktop provider is
installed, including embedded images. See
[Kryon project profiles](https://github.com/kryonlabs/kryon/blob/master/docs/PROJECTS.md).
Application-specific storage, audio, and other native interfaces still require
declared host capabilities with portable signatures.
