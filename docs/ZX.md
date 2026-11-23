# Ziran portable bundle (`.zx`)

The file roles are `.zi` for source, `.zir` for checked intermediate modules,
and `.zx` for portable applications and libraries. The portable tool is `zi2zx`,
and its target name is `zx`. Revision-28 executable ZX contains verified
lowered instructions, assets and the names its embedder sees.
The normative byte contract is
[the executable format specification](../ASSEMBLY/zx-executable-format.md); see
[Implementation status](IMPLEMENTATION_STATUS.md) for the remaining work.

## Loading libraries

Build a provider with a logical name and explicit export roots. Save it as
`LOGICAL_NAME.zx` beneath a module root. This example exports `counter:Add`,
which takes an `s32` and returns an `s32`.

```sh
mkdir -p modules/demo
zi2zx --library demo/counter --export counter:Add \
    -o modules/demo/counter.zx counter.zi
```

In `app.zi`, declare the import with a `u64` instance handle followed by the
provider's parameter types. Load the declared interface before calling it.

```ziran
using Modules :: #import "std/zx_module"

host_api :: #system_library "host_api";
AddExternal :: (handle: u64, amount: s32) -> s32 #foreign host_api;

main :: () -> s32 {
    handle := Modules.Load("Counter.v1")
    if handle == 0 { return 1 }
    result := AddExternal(handle, 2)
    if !Modules.Release(handle) { return 1 }
    return result
}
```

```sh
zi2zx --require-module Counter.v1=demo/counter \
    --import app:AddExternal=Counter.v1:Add -o app.zx app.zi
ziran run --module-root modules app.zx
ziran inspect --emit linkage modules/demo/counter.zx
```

Repeat `--module-root` to search several folders. With no explicit root, a
saved bundle uses its own directory and a source run uses the current
directory. The loader checks dependency graphs and import signatures before
running initializers. Distinct files claiming the same requested path are
ambiguous. Each successful `Load` creates independent module state and
returns a handle scoped to its caller. Use `Problem()` to read the loader's
latest diagnostic and the export and parameter queries to inspect its types.

`Replace(handle, artifact_name)` resolves a candidate under the same roots.
Use its path without `.zx`. It must declare the same logical name and export
interface. Replacement retains the handle and migrates compatible globals.
An incompatible candidate leaves the original instance available. C embedders
can supply a state migration callback and grant libraries host bindings through
[`ziran_zx_module.h`](../include/ziran_zx_module.h). Module calls are synchronous.

Embedders with a separate filesystem namespace can pass a
`ZxModuleLoaderConfig` to `ZxModuleLoaderOpenConfigured`. Open the app with
`BundleOpenBytes`, then pass it to `ZxModuleProgramOpen`. If the embedder can
transfer ownership of the input, use `BundleOpenOwnedBytes` with a release
callback that matches its allocator. The callback runs once before return,
including on failure. On a successful read, the bundle retains the decoded
program, assets and SHA-256 before releasing the input ahead of planning.

For each named library the loader asks the configured resolver for `ROOT/LOGICAL_NAME.zx`
under each root, including for transitive dependencies. The guest `Replace`
service uses the same search. C `ZxModuleReplace` passes the supplied path
directly to the resolver.
Return `ZX_MODULE_IMAGE_MISSING` to advance the search or
`ZX_MODULE_IMAGE_ERROR` to abort. When returning `ZX_MODULE_IMAGE_FOUND`, keep
the bytes valid through verification. The loader calls the optional release
function with the resolver context after it finishes with those bytes. It
uses only the resolver for library reads when one is configured. Distinct
guest paths that both return an image are ambiguous. The loader verifies
each selected bundle and its logical name, dependency graph and typed imports
before running any initializer.

Supply ordinary library host grants in `library_bindings` and grant app
bindings separately in `ZxModuleProgramOpen`. Supply `write` to route the
app's and libraries' platform writes, including initializers, to a common
descriptor callback. Select `BUNDLE_BACKEND_ENGINE` for an explicit engine
policy. With a console callback, a native or Wasm backend selection fails
before initialization. The loader copies roots and options. Keep callback
contexts and library grants alive through loader close, and the app bundle
alive through program close.

Use the instruction engine to run a module that a supervisor can stop.
Create a `ZxRunControl` before starting the guest and pass it to
`ZxModuleProgramOpenControlled` and `ZxModuleProgramRunControlled`. The control
also applies to initializers and calls in libraries the guest loads. The
equivalent APIs for a plain bundle are `BundleInstantiateControlled` and
`BundleInstanceRunControlled`. Select `BUNDLE_BACKEND_ENGINE` explicitly for
controlled initialization. The engine checks the request at execution
checkpoints, including in its packed instruction tier.

A supervisor can call `ZxRunControlRequest` and `ZxRunControlRequested` from
another thread. The guest thread retains ownership of the loader and its
instances. Only the caller can wake a blocked resolver or host I/O callback.
Keep the control alive until the guest calls and all supervisor accesses have
returned. A request stays set, so create a fresh control for a later
independent run. These calls borrow the control for the duration of the call.

Open returns `NULL` and sets the optional status to `ZX_RUN_CANCELLED` on a
stop. Run returns `ZX_RUN_CANCELLED` and leaves its result outputs untouched.
A request observed before entry skips the run and leaves the instance usable.
A request observed during execution retires the instance, which the caller
can then close. `ZX_RUN_ERROR` covers other runtime failures, including
exhausting the configured step budget. `ZX_RUN_OK` reports normal completion.

Portable embedders can call `ZxModuleProgramBorrowHostString` or
`BundleInstanceBorrowHostString` after initialization to borrow one granted
host function's top-level string parameter by zero-based index. The
callback reads the checked bytes synchronously as `HOST_STRING`. Keep
those bytes read-only and within the call, and do not reenter or close the
instance from the callback. Return values are serialized immediately, so a
returned string can refer to the input. Nested values and mutable slices use
the usual host images.

`ZxModuleProgramBorrowHostSlice` and `BundleInstanceBorrowHostSlice` opt in
to an input-only slice of fixed-width integers or floats on a `#reads`
capability. On success the instruction-engine callback receives
`HOST_BORROWED_SLICE`, the declared type, an element count in `length`, and
packed little-endian bytes in `data`. Use byte reads or `memcpy` because
the storage may be unaligned. The bytes are valid only until the callback
returns. The host must leave them unmodified and cannot reenter or close
the instance while it uses them. The usual `HOST_SLICE` interface remains
available without opting in, including on the native and Wasm backends.

## Running bundles

Run source in one command with `ziran run hello.zi` or `zi2zx run hello.zi`.
Both check the file, write the bundle in memory and execute its `main`.
The file's folder is the default source root. `ziran run app.zx` and
`zi2zx run app.zx` run a
bundle file. Zero-argument entries returning an integer, `bool` or nothing
run; the command prints a non-void result on its own line, unsigned results
over their full range.

On Linux x86-64, AArch64 and ARMv7, `ziran run app.zx`, `zi2zx run` and the embedding API run a
bundle as native code compiled from its verified instructions; in a browser
it runs on the Wasm tier and on other CPUs on the portable instruction engine.
The platform makes that choice, never the bundle: a bundle native code cannot
run fails before it starts with a named gap. `--backend engine` picks the
engine explicitly. The Linux i386 packed engine can compile short runs of
scalar integer instructions once a block becomes hot. It uses a bounded
cache of executable pages. The interpreter handles memory, calls, control
flow and checked failures. Both paths apply the same instruction work and
fuel charges. See the [i386 execution plan](../plans/2026-11-21-i386-execution.md)
for measurements and remaining work.

Native code follows manual safety like C: a pointer is its
address and indexes are raw, and each run of `allocate` gets fresh zeroed
storage that lasts until its frame returns. Each active instance reserves a
contiguous frame arena for its outer call. The runtime tries smaller arenas
if the host cannot reserve the preferred 256 MiB. A frame allocation beyond
the mapped extent traps, and the reservation is released when the call ends.
A bundle built with `--checks`
(header flag 1 of its instructions) checks every pointer as the engine does:
a read through a pointer into a returned frame, a use after `free`, a second
`free`, a store through read-only bytes and an index out of range all trap.
`#checks` marks single instructions to keep their bounds check in a bundle
built without it. The engine always checks. A native trap is reported once. Nothing ever falls back to, or retries on, the
engine. `zi2zx --debug` also writes
the program's source origins to the optional debug attachment, so native trap
diagnostics and `zx2native --emit map` name source lines.

C embedders can open verified bytes with `BundleOpenBytes` and use
`BundleInstantiateWithConsole` to run them on the instruction engine. The
callback receives writes to descriptors 1 and 2, including writes during
global initialization. The embedder can connect those descriptors to its
own console or process namespace. See [ziran_host.h](../include/ziran_host.h).

For a memory-constrained embedder, build `libziran.a` with
`-DZIRAN_COMPACT_EXEC_PLAN` in `CFLAGS`. The instruction engine runs verified
instructions directly, with the same binding checks and execution limits.
This saves the fast-code tables and can make execution slower.

On hosts where growing a contiguous allocation can exhaust a fragmented heap,
build with both `-DZIRAN_EXEC_FRAME_RESERVE_BYTES=6291456` and
`-DZIRAN_EXEC_FRAME_RESERVE_MIN=1048576` in `CFLAGS` to reserve 6 MiB of frame
storage at portable instance creation when a verified function has a frame of
at least 1 MiB. Both settings are in bytes. The engine attempts the reserve
only within its existing value-byte limit. If the allocation fails, frame
storage grows on demand under that same limit. Without these settings, frame
storage always grows on demand.

Compiled native code is cached on disk in `$XDG_CACHE_HOME/ziran/native`, or
`~/.cache/ziran/native` without it, so a later run in another process maps the
code instead of compiling it again. An entry is keyed by the bundle's content,
the CPU's features and the build of the compiler that made it. It is checked
against that key and its SHA-256 digest before it is used, and a damaged entry
is removed and rebuilt. `ZIRAN_NATIVE_CACHE=DIR` uses another directory and
`ZIRAN_NATIVE_CACHE=off` turns the cache off.

Set `ZIRAN_PROFILE` to an absolute output filename to write a JSON timing
record at normal process exit. `zx.bundle.open` covers portable bundle
validation and content hashing after the bytes are read. `zx.native.lookup`,
`zx.native.compile` and `zx.native.store` separate cache lookup, compilation
and cache writing. `zx.native.load` covers native instance setup through
executable mapping. `zx.native.init` measures the module initializers before
the entry runs. A warm cache hit records lookup and instance startup without
a compile phase.

## Learning

`ziran build FILE.zi` keeps a corpus of the programs it compiles to native code,
as training data for improving Ziran's machine code. It is on by default,
`ziran learn off` stops it and `ziran learn on` resumes it; benchmarks run
through `bench/bench.zi` never record. Recording never prints during a build;
`ziran learn status` reports a record that failed. Each new build stores its verified
`.zx`, ELF, `zx2native --emit map` ranges and lowered input once under
`$XDG_DATA_HOME/ziran/learn` (or `~/.local/share/ziran/learn`), named by their
SHA-256 digests, so a rebuild writes nothing and a compiler that emits different
code adds a second record beside the first. Programs are filed by format,
`zx<R>-lir<S>/<ZX sha256>/`, from the bundle's own header revision and
instruction schema: tools read only their own revision, so a format change
starts a new folder instead of mixing in programs the current tools reject.
Each record ends with `<target>-<ELF sha256>.record.json`, written last, which
names the compiler (the SHA-256 of `zx2native` and `zi2zx` and the Ziran
version), the source and entry and the Unix time. `--limit-mb N` caps the
corpus (1024 MB by default), `ziran learn status` shows its size and each
format's programs and records, `ZIRAN_LEARN=on|off` overrides the switch and
`ZIRAN_LEARN_DIR=DIR` moves the corpus.

`ziran learn verify --candidates FILE --repo DIR --output FILE` validates the
C teacher's closed integer rewrite proposals against generated C, boundary and
deterministic random inputs, and the retained parity fixtures. A disagreement
leaves the table unchanged. `bench/bench.zi -- c-teacher` produces the assembly
comparison, sampled rankings and candidate files.

`ziran learn train --measurements FILE --table FILE --verification FILE
--output FILE` selects implemented policies from paired timing receipts joined
to current-format corpus artifacts. It checks the compiler, ZX, ELF and native
map identities, equal C/native checksums, learning-off measurements, common
host and source epoch. It retains symmetric 1.5×IQR filtering and deterministic
size/policy tie breaking. Unsupported unroll and allocation choices are refused.
Review the generated source table, rebuild and run parity before checking it in.
Normal builds use only the versioned table in `cmd/compiler_learning_table.zi`;
recording and offline training never change a running compiler's policy.

Use `ziran run app.zx` for both console and graphical programs. The linker
includes the reachable code of imported modules, including a UI library.
Platform effects such as windows and input are supplied by an installed runtime
provider. Ziran discovers providers generically, without UI-library names or
widget-specific startup behavior. A console bundle with no required host
capabilities runs directly and never starts a graphical provider.

Packages install a `.host` registration under `share/ziran/hosts`: one
absolute executable path, then a line `ziran IDENTITY` holding what
`ziran version --identity` prints for the toolchain that built the provider.
`ziran run` hands a bundle only to a provider of its own build; when only
providers of another or an unrecorded build are installed it names both
identities and exits non-zero without running anything, because another
runtime could misread the bundle. Discovery searches `$XDG_DATA_HOME/ziran/hosts`
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

### ZX Runner

`make install-user` installs `zx`, ZX Runner, beside the toolchain. `zx
app.zx [ARGUMENT...]` runs any bundle in-process with the linked runtime, as
Linux runs an ELF file; a console program and a window program are opened the
same way. The bundle carries all of its code, including the libraries it
imports, and imports only host capabilities (`module:function`), which the
runner answers itself. It names every capability it lacks and runs nothing;
unlike `ziran run`, it hands a bundle to no installed provider.

The capabilities every bundle may use are declared in standard modules:

| Module | Capabilities |
| --- | --- |
| `std/guest_assets` | `AssetCount`, `AssetName`, `AssetData`, `AssetFind`: the bundle's own embedded files |
| `std/guest_clock` | `MonotonicNanoseconds`, `WallMilliseconds` (since 1970 UTC) |
| `std/guest_files` | `FileStatus` (size, `FILE_MISSING`, `FILE_DIRECTORY`), `FileRead`, `FileWrite`, `FileRemove`, `FileRename`, `DirectoryMake`, `DirectoryList`, with a native program's reach |
| `std/guest_process` | `ArgumentCount`, `Argument` (0 is the bundle's path), `Environment` |

Capabilities are only added: a runner never changes what an existing name
means, and a different meaning gets a new name.

`zx` reads the header first, so a file
that is not a ZX program, or one written in an older or newer ZX format
revision, gets a plain explanation naming both revisions. Started without a
terminal on standard error, as a file manager starts it, ZX Runner also shows
any failure, including the program's own error output, in a dialog (`zenity`,
else `xmessage`).

The installation registers the `.zx` file type for the user from
`packaging/linux`: `application/x-ziran-bundle`, matched by the `*.zx` name and
by the `ZX\0\0` signature at offset 0. The desktop entry
`org.ziran_lang.zx.desktop` runs the installed runner on that type and becomes
its default opener through `xdg-mime`. The header has to start the file, so a
bundle cannot carry a `#!` line; run it with `zx app.zx` or open it.

## Writing bundles

`zi2zx --root DIR [--module-path DIR] [--define NAME] [--asset-dir NAME=DIR] [--retain module:function] [--bind caller:capability=provider:function] [--bind-host MODULE] --entry module:function -o FILE file.zi|file.zir ...`
loads explicit inputs and their extensionless imports, lowers what the entry
reaches to instructions, verifies and optimizes them, and writes the bundle.
Source and saved `.zir` give the same file:

```sh
zi2zx --root src --entry app:main -o app.zx src/app.zi
zx2native --target x86_64 --platform linux --emit elf -o app app.zx
zi2zx run app.zx
zi2zx --emit lir -o - app.zx
zx2wasm -o app.wasm app.zx
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
the instruction payload does not keep. Every global of the entry module is
named, read or not; another module's globals are named when the program
reaches them. A record may hold itself through a slice. A bundle whose
interface cannot be written fails with `assembly.container` instead.

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

For app images without assets, module linkage or additional attachments,
`BundleOpenReadAt` verifies a seekable image without retaining the input
bytes. Provide the image size (up to 256 MiB) and a callback that fills each
requested range and returns its exact byte count. The source must stay stable
until open returns. Verification rejects short reads, read errors, and changed
bytes on a repeated read. The returned bundle owns its decoded program and
has the same SHA-256 identity as the byte-based open functions. You can close
the source and dispose of the callback context as soon as open returns.
If the optional `unsupported` output is 1, the source profile cannot verify
the image. The caller can explicitly retry through `BundleOpenOwnedBytes`.

`BundleOpenEngineReadAt` accepts the same app images with the same reader
lifetime and identity. Instantiate its bundle with `BUNDLE_BACKEND_ENGINE`,
including when passing it to `ZxModuleProgramOpen`. When the image allows it,
the reader decodes instructions into a compact table and expands one function
at a time for verification and planning. Other images use the full portable
instruction form during verification and can still be packed for execution.
To return unused storage to an allocator that keeps the full block on
shrink, supply a `BundleShrink` callback as the final argument. It runs during
open when packing leaves unused storage and receives the smaller size. Its
result follows `realloc` ownership rules and must be releasable with the
runtime's `free`. On callback failure the engine uses the existing allocation.
Passing `NULL` uses `realloc`. Use
`BundleOpenReadAt` for native and Wasm execution.

Each host call is translated between the engine's host image and `HostValue`
trees by the host interface section. A value's type is the spelling declared at
its position, and a spelling that starts with `u` is unsigned; a `u64` above the
`s64` range reaches the host in `bits` alone. Returned records give every
declared field by name and in order, unless `BundleInstanceMatchFieldsByName`
lets a host built against another layout omit or add fields. Numeric, string
and record slice parameters cross as `HOST_SLICE` with a mutable `elements`
array: the host keeps the count and pointer, and its changes are validated and
copied back into the caller's slice after a successful call. Two slice
arguments of one call may not share elements. Declared pointer types cross as
`HOST_POINTER`: an opaque handle that bundles store, compare and pass back,
and that fails the run when the program reads through it. The host owns the
storage behind every handle it hands out. A value that nests past 64 levels,
such as a tree that holds itself, fails the call. Values a host returns are
copied into the instance, which gives their storage back once nothing the
program can reach names them.

A run is not bounded: like the program's native builds, it continues until the
entry returns. `BundleInstantiateLimited(bundle, bindings, count, max_steps)`
gives one positive step budget to global initialization and every entry run,
so a runaway initializer fails instantiation; `BundleInstanceLimitSteps`
bounds each later run on its own. Steps are function calls and loop
iterations. A runaway recursion stops with "nested calls filled the stack";
other run failures name what happened, such as an index out of bounds or
unreachable code executed, and the instruction where it did.

For an entry that handles a long stream of events in one instruction-engine
invocation, start it with a positive step budget. A typed `HostCall` can
request `BundleHostRenewSteps()` after it observes an event it accepts as
progress. A successful callback gives the active entry a fresh full budget.
A loop between events can still exhaust it. In cumulative mode, subsequent
runs count from the last successful renewal. The request returns zero
outside an eligible callback, during initialization, with no active step
budget, or on native or Wasm. `BundleInstanceLimitSteps` updates the settings
used by subsequent invocations.

## Target contract

The pipeline is Source → ZIR → LIR → optimized LIR → ZX → native
(ELF/PE, x86-64/AArch64/ARMv7). Wasm lowers from ZX; C/Go export
from ZIR through `zi2c`/`zi2go`.
The portable instruction engine runs the instructions on any platform. A
supported direct target lowers the same verified instructions to machine code,
applies the target ABI and platform veneers, and writes a platform binary;
`zx2native --target x86_64 --platform linux --emit elf` is the Linux x86-64 row, and
`--platform windows` writes the Windows x86-64 row's PE32+ `.exe`. Generated C and Go are
separate routes, never native fallback. A Linux x86-64 executable links its
host capabilities by name from the registered host libraries, with the C ABI
the C target declares for them ([host capabilities](USAGE.md#host-capabilities-and-visibility));
a capability none defines is the `assembly.gap.machine-host` gap.

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
procedure values, slices and other borrowed or host resources are rejected.
Snapshots are bounded in depth and by 16 MiB of copied payload, which
`BundleStateValueBytes` reports. A migration callback receives the old snapshot and a mutable snapshot
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
`BundleInstantiate` lowers the bundle's verified program with the engine's
Wasm lowering, instantiates it through the provider, runs its module
initializers, and charges the run budget like the instruction engine. An
Emscripten build of `libziran` installs the browser provider itself. In an
Asyncify runtime, whose hosts may suspend the stack, modules are lowered
suspendable (`LowerProgramSuspendable`): a module unwinds with the stack
and resumes at the host call when it rewinds. A JSPI runtime leaves bundles to the
instruction engine. Set `ZIRAN_WASM_TIER=0` in the environment, or
`globalThis.ZIRAN_WASM_TIER = "0"` before a page loads the runtime, to leave
bundles to the instruction engine. Each instance
keeps its own bindings and linear memory. Host calls and module state cross as
host images, which the embedding API translates to `HostValue` trees by the
host interface section, so typed capabilities, records, strings, slices and
state export and import behave as on the engine. A program outside the
lowering, or a module the provider rejects, fails instantiation; no other
engine starts. A web build needs `libziran` built with the instruction engine,
that is without `BOOTSTRAP=1`. `tests/zx_wasm.sh` runs bundles on the tier
under node beside the engine, and `tests/zx_web_lifecycle.sh` runs source
through `.zx` in a private headless Chromium.

`zx2wasm` lowers the same verified ZX to one wasm32 module, and
`ziran build --target=wasm` runs it; source and saved-ZX input give identical
bytes. The module imports `zx.overrun` and the mutable `zx.budget` global that
every `fuel` instruction charges. `zx2wasm --unbounded` makes the module for a
run with no step budget, as a native executable is: its `fuel` instructions
charge nothing and it imports no `zx.budget`. A module imports `zx.host_i64`
when the program prints and `zx.host` when it calls a host capability. It
exports `zx_entry`, `memory`, `zx_initialize` when it has module initializers
and `zx_reserve` when it has a heap; a `zxhost` custom section names each
capability. A form it has no code
for stops at a named `wasm.gap.*` diagnostic. The host-call record layout is in
`cmd/compiler_zx_wasm.zi`.

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
