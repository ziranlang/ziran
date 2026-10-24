# Ziran portable bundle (`.zx`)

This describes the target contract and the experimental subset that ships now.
See [Implementation status](IMPLEMENTATION_STATUS.md) for the remaining work.

The file roles are `.zi` for source, `.zir` for checked intermediate modules,
and `.zx` for portable executables and libraries. The portable tool is
`zi2zx`, and its target name is `zx`. This is a permanent format cutover:
rebuild previous `.zib` programs from source or current `.zir`; changing a
filename alone does not convert the binary format. The previous tool and
target names are not aliases.

## Experimental version 26

`zi2zx bundle --root DIR [--module-path DIR] [--define NAME] [--asset-dir NAME=DIR] [--bind caller:capability=provider:function] [--bind-host MODULE] --entry module:function -o FILE file.zi|file.zir ...`
loads explicit inputs and their extensionless imports, then links reachable
modules. `zi2zx run FILE` validates and executes
zero-argument integer, bool, or void entry functions on a portable interpreter.
Integer entries support `s8`, `s16`, `s32`, `s64`, `u8`, `u16`, `u32`, and `u64`;
the command prints unsigned results over their full range. C hosts recover a
`u64` entry result by casting the returned `long long` to `uint64_t`.
Pointer-sized `isize` and `usize` remain outside the portable execution subset.
Called functions can use these integer types,
`bool`, `float32`, and `float64`, plus
immutable `string` values, enums, and plain records with scalar, enum, string,
or nested record fields. Strings support UTF-8 literals, byte length and
read-only indexing, equality, parameters, returns, and record fields. Record defaults and literals,
member reads and writes, scalar compound assignments, parameters, returns,
and value copies execute in the interpreter. The unsigned integer subset includes
bitwise `&`, `|`, `^`, `~`, shifts, and their compound assignments; `u64`
comparisons, arithmetic, and shifts use the full unsigned range. Signed `s64`
arithmetic, casts, bitwise operations, and shifts are also supported. Portable
enum initializers support integer literals and references to preceding members joined by `+`
or `-`; other constant expressions remain outside this subset. The bundle is
`ZX` plus two zero bytes, a little-endian version, length-prefixed entry module
and function names, a host capability count and its required module/function
names, and a
length-prefixed version 58 `.zir` payload. Named law and waiver tables precede
that payload; each law includes its method, declared domain, concrete case
count, counterexample, and waiver state. The loader rechecks embedded theorem
certificates and compares the outer tables against the checked IR. Verification
roots retain their imported laws, proof certificates, scalar types, constants,
and called procedures even when the executable entry does not use them.
See [Laws and scalar proofs](LAWS.md). The current linker follows direct
function calls from the entry, keeps record and enum declarations used by
those functions (including types from imported modules and nested record
fields), and retains imported startup paths even when no symbol from them is
called. It removes other unreachable functions, modules, types, and imports before
writing the payload. Bundle serialization omits native `#asm` variants and
writes their checked local bodies as ordinary blocks. Typed expression graphs,
source locations, certificates, capabilities and assets remain intact. Source
and saved `.zir` keep their native variants for native builds; their encoding
and the bundle version are unchanged.

The bundle writer first validates and measures the portable payload, then
streams it directly to the destination. It needs neither a temporary payload
file nor destination seeking, including for pipes and append streams. The
input graph remains immutable. This performs a second field traversal but
avoids a whole-payload buffer or temporary-file copy. Output failures are
reported by the writer.

Compile-time definitions and imported modules needed by
array bound expressions remain available when the bundle rechecks saved IR. Enum
member references keep their declaration even when
the expression uses the member as an integer without an explicit enum cast.
The reader rejects unsupported versions, truncated or trailing data, malformed
embedded IR, semantic errors and divergent fields in saved IR, unresolved imports,
unsupported capabilities, and functions outside the current portable subset.
Reachable `#foreign host_api` calls with scalar, string, enum, plain record, or void signatures become
explicit capabilities. A bundle may instead bind one such call to an exported
Ziran function supplied among its input modules with `--bind`. The linker checks
that the provider is reachable and its parameter and return types match; a
bound call executes inside the same VM instance and is absent from the external
capability list. An embedding C host links `build/libziran.a`, includes
`ziran_host.h`, opens a bundle with `BundleOpen`, and inspects its required
module/function names with `BundleCapabilityCount`, `BundleCapabilityModule`,
and `BundleCapabilityFunction`. It passes `HostBinding` entries to `BundleRun`.
The runner checks that every required binding is present before executing the
entry function. The standalone `zi2zx run` command has no host bindings and
rejects a call that needs one. A run is not bounded: like the program's native
builds, it continues until the entry returns. Only the web playground stops a
run after 1,000,000 statements (`VmRunBounded`), so a runaway loop cannot hang
the page, and it says so in the diagnostic. A function holds as many locals as
it declares. Calls nest until the C stack is nearly full, where the thread's
stack bounds are known (Linux); `zi2zx run` runs the program on a 256 MiB
stack, room for about 60,000 nested calls, and a runaway recursion stops with
"N nested calls filled the stack". Elsewhere, such as in the playground, calls
nest 128 deep. Functions take the language's full 64 parameters. The loader compares the capability list with
the linked IR; the VM checks signatures before execution and verifies returned
record field names and types. Record fields may themselves contain records,
strings, or enums.
Numeric, boolean, string, and plain-record slice parameters use `VM_HOST_SLICE` and a mutable
`elements` array. Hosts keep the element count and pointer intact; the VM
validates each element and copies changes back into the caller's slice after a
successful synchronous call. Overlapping mutable slice arguments are rejected.
Declared pointer types cross as `VM_HOST_POINTER` with the raw address in
`pointer`. The VM treats handles as opaque: bundles store, compare against
`null` and each other, and pass them to host capabilities, but cannot
dereference, index, or do arithmetic on them. The host owns the storage behind
every handle it hands out.
Source and saved-IR bundle bytes match in scalar, record, and enum tests,
including Kryon's geometry, layout, and popup ownership tests. Synchronous
procedure type aliases with imported named functions run from
source and saved IR, including pointer and slice arguments, nullable values,
global initializers, and calls through record fields and fixed arrays.
Fixed arrays with numeric or resolved integer constant-expression capacities support defaults,
positional literals, element reads and writes, and value copies, including
nested arrays and `u8` arrays inside imported records. Indexing is bounds-checked. Version 26 is
experimental and has no compatibility promise. Host calls with procedure
slots remain unsupported. Default-initialized module globals of
portable value types work within one `BundleRun`; each call starts with fresh
global values. `BundleInstantiate` creates an instance whose globals persist
across `BundleInstanceRun` calls. Checked runtime global initializers run once
when an instance opens, after imported modules initialize. Native pointer
casts and callbacks across the host boundary remain unsupported; required
capabilities must use the portable signatures described above.

Opening a bundle verifies its final linked code, including shared libraries.
Each fresh instance validates its host bindings and creates independent globals;
it reuses verification of the immutable bundle. Separate instances can execute
concurrently while the bundle and their binding contexts remain alive.
Capability enumeration reuses the final code's immutable list, including library
requirements. Instantiation copies binding names into compact storage and indexes
them by module/function, so caller order does not affect dispatch. Missing or
duplicate bindings fail before any global initializer or entry executes.

Local slices may borrow fixed arrays or other slices, cross ordinary Ziran
function calls, return views of permitted backing storage, and read or write
indexed elements. The loader checks the existing slice lifetime rules; the VM
checks ranges and indexes during execution. A live view keeps its backing
array identity across an array value assignment.

## Target contract

A `.zx` is a versioned, portable executable bundle linked from checked
`.zir` modules. Its format identifier is `ZX`. It contains the program's
portable executable content, data, entry point, linked module identities,
exports, and explicit host capability requirements. The exact instruction and
section encoding will be specified before the format is called stable.

The linker resolves imports and includes reachable code from ordinary
libraries. That includes Kryon only when the program imports it. A non-UI
command-line program is a first-class `.zx`; it launches without a graphical
host. Kryon UI code is portable Ziran code in the bundle, with rendering and
input supplied through declared host capabilities. The same bundle format
serves graphical and non-graphical programs.

Before execution, the loader validates the format version, section bounds,
symbols, entry point, and capability contract. Missing symbols, unsupported
versions, malformed bundles, and unavailable required capabilities fail with
diagnostics. A portable bundle cannot contain an undeclared dependency on
target-specific C or Go code. Hosts may implement the declared
capability interfaces with native libraries.

## Native execution

On Linux and Android hosts with x86-64, 32-bit ARM or ARM64 processors,
`libziran` runs bundles as native code. `BundleInstantiate` lowers the
verified program to C with the backend `ziran build --target=c` uses,
compiles it in memory with the bundled TinyCC (`third_party/tinycc`, LGPL-2.1;
applications that ship it include its notice) and calls it. Host
capabilities receive the same `VmHostValue` shapes the portable VM passes.

Compiled code is cached under a key of the bundle's SHA-256, its libraries,
the CPU and the compiler's own sources, in `$ZIRAN_NATIVE_CACHE`,
`$XDG_CACHE_HOME/ziran-native`, `~/.cache/ziran-native` or
`$TMPDIR/ziran-native-cache`; `ZIRAN_NATIVE_CACHE=0` turns it off. Kryon's
Android hosts point `TMPDIR` at the app's private files.

A bundle that cannot be compiled fails with `zx.native`; there is no
fallback. Runtime failures stop the run as in the VM: bounds, unreachable
code, a host breaking the value contract, the step limit and nested calls
filling the stack. Native code counts the step limit in function calls and
loop iterations rather than statements, and does not check pointer lifetimes
at run time: it trusts bundle code as a native build of the program would.

`#parallel for` regions run on worker threads each instance starts at its
first region, one per processor up to 16; `ZIRAN_PAR_THREADS` in the host's
environment sets the count, and 1 runs regions in place. The calling thread
waits while the workers take chunks of the range in order. A failure inside
a worker stops the run with the failure a serial run would meet first. Each
worker counts steps against what the run has left, and the run is charged
for all of them when the region ends.

Other hosts (the browser, Windows, macOS) run the portable VM, and
`zi2zx-vm` with `libziran-vm.a` run it everywhere for tests of the VM itself.
`ZIRAN_NATIVE_TIMING=1` reports where compilation time goes and
`ZIRAN_NATIVE_KEEP=1` keeps the generated C.

## Embedded files and application hosts

`--define NAME` selects the same source branches as the native compiler.
`--asset-dir assets=assets` embeds regular files recursively under `assets/`.
The argument's left side is the path prefix inside the bundle, and its right
side is the source directory or file. Multiple mappings may be supplied.
Names are sorted bytewise for deterministic output. Duplicate names,
symlinks, absolute paths, backslashes, empty components, and `.` or `..`
components are rejected.

After the checked IR, version 26 stores a little-endian u32 file count. Each
file has a u32 name length, name bytes, a u32 data length, and exact binary
bytes. Empty files are supported. The loader checks lengths against remaining
input before allocating and rejects noncanonical names/order and trailing data.
`BundleAssetCount`, `BundleAssetName`, `BundleAssetData`, and `BundleAssetSize`
expose immutable files until `BundleClose`. Ziran hosts import `std/bundle_host`
for file access and enumeration of the exact required capability names.
`BundleOpenBytes` / `OpenBundleBytes` validate embedded or downloaded bytes
through the same reader and verifier, without requiring an application file
path. The caller may release the input bytes after opening the bundle. Unix,
Android (including API 21) and Web hosts read the input and embedded IR through
read-only memory streams; loading needs no temporary directory. Windows hosts
use seekable temporary streams.

Kryon's reusable desktop player supplies rendering and input to a ZX instance;
the application's widgets and state run inside the VM. A downloaded graphical
program launches with `kryon run app.zx`, including embedded images. See
[Kryon project profiles](https://github.com/kryonlabs/kryon/blob/master/docs/PROJECTS.md).
Application-specific storage, audio, and other native interfaces still require
declared host capabilities with portable signatures. A native application's
pointer casts or calls through native callbacks cannot simply be bundled.
