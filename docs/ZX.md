# Ziran portable bundle (`.zx`)

This describes the target contract and the experimental subset that ships now.
See [Implementation status](IMPLEMENTATION_STATUS.md) for the remaining work.

The file roles are `.zi` for source, `.zir` for checked intermediate modules,
and `.zx` for portable executables and libraries. The portable tool is
`zi2zx`, and its target name is `zx`. This is a permanent format cutover:
rebuild previous `.zib` programs from source or current `.zir`; changing a
filename alone does not convert the binary format. The previous tool and
target names are not aliases.

## Running bundles

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
or entries or open a window. The selected executable then receives `FILE.zx`,
or `--vm FILE.zx` when portable execution was requested. Its exit status is the
run status; an execution failure never retries the app on another provider.
Malformed bundles fail before discovery. Missing providers produce a diagnostic
before the application executes. Provider registration is part of the library
platform installation and adds no requirement to a console-only Ziran install.

For example, Kryon `make install-user` installs its reusable desktop provider.
A graphical `.zx` then runs with `ziran run app.zx` without application sources,
a manifest or generated native output. The portable bundle carries its linked
UI code and embedded assets; the installed provider bridges the local OS.

## Opt-in executable instruction container (revision 28)

ASSEMBLY step 05 now has an independent executable-container foundation. It is
opt-in through `zi2asm`; the installed/default `ziran` and `zi2zx` routes still
write and read experimental versions 26/27 until the complete ASSEMBLY cutover
gates pass. The two paths never guess or fall back between formats.

```sh
zi2asm --root src --entry app:main --emit zx -o app.zx src/app.zi
zi2asm --input zx --emit elf -o app app.zx
zi2asm --input zx --run app.zx
zi2asm --input zx --emit lir -o - app.zx
```

Revision 28 stores deterministic verified lowered instructions, not `.zir`.
Source is checked and lowered to LIR, verified, then optimized before ZX is
encoded. Native output from source serializes and reads this same verified ZX
boundary; source and ZX entry routes therefore produce identical ELF bytes.
There is no standalone `.lirb`, `--emit exec` or `--input exec` route. The
instruction codec remains internal to ZX. Source `--emit lir` retains the raw
inspection view; `--opt --emit lir` and `--opt --emit opt-report` inspect the
pre-ZX optimization stage. ZX input is never reoptimized by `--opt`.

`--emit zx` removes source names and instruction origins from the required
program before encoding. `--input zx` validates section framing, SHA-256 content
identities, the owned LIR decoding, the LIR verifier, entry identity and the
reachable symbol/capability subset before inspection, re-emission, native local
generation, or portable execution. The input bytes may be released after the
read. A stripped file therefore runs through the bounded portable instruction
engine with no source or semantic-IR parser.

Native maps retain instruction IDs and final byte extents, but no source
origins from this stripped payload; executable debug attachments remain a gap.
The current `zir_zx_wasm` route still calls `zir_wasm_lower` on semantic
`ZirProgram`. It does not lower revision-28 ZX and does not satisfy the shared
instruction-to-Wasm cutover. Default semantic bundles, generated C/Go binaries,
and that Wasm route are separate current implementations, never native fallback.

The fixed 64-byte header uses little-endian fields: `ZX\0\0`, version 28,
header size 64, directory-entry size 32, required and optional feature masks,
section count, flags, total byte length, directory offset 64, identity algorithm
1 (SHA-256), and a zero reserved word. Required feature bit 0 selects the
manifest's portable-LIR contract. The internal instruction header independently
checks current LIR schema 4. Unknown required feature bits reject the file; unknown
optional feature bits are recorded but do not affect execution.

Each 32-byte directory record contains kind, flags, offset, length, alignment
and a zero reserved word. Kinds are strictly increasing and unique. Required
manifest, program and asset sections are kinds 1, 2 and 3. Kind 4 is reserved
for native variants. Optional debug and source attachments are `0x80000001` and
`0x80000002`; producer-defined optional attachments begin at `0x80000100`.
Required sections have flag bit 0. Unknown optional sections are skipped;
unknown required sections reject. Offsets use canonical zero padding and are
currently 8-byte aligned. The reader rejects arithmetic overflow, invalid
alignment, gaps, overlaps, truncation and bytes after the last section.

The required 128-byte manifest contains schema 1, LIR schema 3, the signed
entry function ID, reachable capability count, required native-variant count,
asset count, proof status, a reserved word, then three 32-byte SHA-256 values.
They identify the domain-separated stripped program, canonical asset sequence,
and combined executable content (entry, required feature contract, program and
assets). Optional debug/source changes do not change executable content identity.
Proof status is currently zero: producer law/proof attachments are not treated
as verified evidence.

Reachable `#foreign host_api` calls with scalar signatures become bodiless
host capabilities called by `host.call`. The reader recomputes the reachable
function set from the entry, direct calls, host calls and function addresses;
indirect calls conservatively retain every portable function. The manifest's
capability count must equal the recomputed count. Other external functions and
non-portable calling conventions reject before execution. An embedder binds each
capability with `LirExecBindHost`; a run with any unbound capability stops before
the entry executes, so the standalone `zi2asm --run` reports `problem=host`. Required native variants and arbitrary machine-code sections are
also rejected. Portable instructions remain mandatory.

Executable-container reads charge the supplied LIR work and byte limits before
hashing or allocating. Symbol identities use bounded deterministic hash tables,
including name-byte and collision work, and the first indirect call expands the
conservative all-functions set only once. A limit failure leaves no valid owned
program. Writes use the default LIR budget, including stripping, hashing and
temporary output storage; short-output calls remain non-mutating.

Asset encoding retains the existing canonical sorted relative-name contract:
little-endian u32 count, then u32 name length/name bytes and u32 data length/
exact binary bytes. NUL is valid in asset data and invalid in names. Measuring
and short-output calls do not modify the destination, so the CLI keeps its
temporary-and-replace behavior and a failed write preserves an existing file.
Full symbolic linking across separate revision-28 library containers, export
tables, capability ABI tables, verified proof attachments, native variant
metadata and the default-runtime cutover remain later step-05/integration work.
Versions 26/27 library and partial-module composition continue unchanged on
their existing APIs while this path remains opt-in. The normative byte contract
and exact remaining gates are in
[the executable format specification](../ASSEMBLY/zx-executable-format.md).

## Experimental versions 26 and 27

Run source in one command: `ziran run hello.zi` or `zi2zx hello.zi`.
Both check the file and execute its `main`; its folder is the default module
root. The direct runner also accepts saved `.zir` and portable `.zx` files.

`zi2zx bundle --root DIR [--module-path DIR] [--define NAME] [--asset-dir NAME=DIR] [--bind caller:capability=provider:function] [--bind-host MODULE] --entry module:function -o FILE file.zi|file.zir ...`
loads explicit inputs and their extensionless imports, then links reachable
modules. `zi2zx run [--vm] FILE` validates and executes a bundle file, or
builds one in memory from `FILE.zi` with the bundle options, its folder as
the default root and its `main` as the default entry, and executes
zero-argument integer, bool, or void entry functions through generated
C/TinyCC where that route is supported and on the portable VM elsewhere or
with `--vm`.
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
length-prefixed version 64 `.zir` payload. Named law and waiver tables precede
that payload; each law includes its method, declared domain, concrete case
count, counterexample, and waiver state. The loader rechecks embedded theorem
certificates and compares the outer tables against the ZIR. Verification
roots retain their imported laws, proof certificates, scalar types, constants,
and called procedures even when the executable entry does not use them.
See [Laws and scalar proofs](LAWS.md). The current linker follows direct
function calls from the entry, keeps record and enum declarations used by
those functions (including types from imported modules and nested record
fields), and retains imported startup paths even when no symbol from them is
called. It removes other unreachable functions, modules, types, and imports before
writing the payload. Bundle serialization keeps procedures' `#asm` variants
and writes local `#asm` blocks as their checked bodies in ordinary blocks.
Only `zi2zx run FILE.zi` and `ziran run FILE.zi`, which build the bundle from
local source in memory, run those variants natively; a bundle file, however
it was built or obtained, and the portable VM run each procedure's portable
body, so a downloaded bundle cannot run instructions of its own. A run that
links libraries from other bundles runs no variants either. Code compiled
with the variants is cached apart from the same bundle run without them.
Typed expression graphs, source locations, certificates, capabilities and
assets remain intact. Source and saved `.zir` keep every native variant for
native builds. Run `sh tests/zx_native_asm.sh build/bin/ziran`.

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
the page, and it says so in the diagnostic. Embedding hosts can instead call
`BundleInstantiateLimited(bundle, bindings, count, max_steps)` with a positive
statement budget. This explicitly selects the portable VM and applies one
cumulative budget to global initialization and every entry run; initialization
failure returns `NULL`. In Ziran, `std/bundle_host` exposes the same operation as
`InstantiateBundleLimited`. The ordinary `BundleInstantiate` and later
`BundleInstanceLimitSteps` retain their per-entry limit behavior. Hosts needing
the portable engine alone link `build/libziran-vm.a`, built by
`make build/libziran-vm.a`; that archive excludes the native code compiler.
A function holds as many locals as
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
nested arrays and `u8` arrays inside imported records. Indexing is bounds-checked. Versions 26 and 27 are
experimental and have no compatibility promise. Host calls with procedure
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

The intended common pipeline is source → `.zir` → linked and verified
portable ZX instructions. An explicitly selected portable route interprets
those instructions. An explicitly supported direct target lowers the same
verified instructions to machine code, applies the target ABI and platform
veneers, and writes or loads a platform binary. The direct route is still
experimental and incomplete; current generated-C bundle execution below is a
separate route, not proof that direct native lowering is complete.

Generated C and generated Go can also be compiled into platform binaries by
their downstream toolchains. This document reserves “native” for direct
lowering from verified ZX instructions to machine code. No generated-code
route, interpreter, or legacy engine substitutes for a failed native request.

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

Host capabilities name portable typed services and are bound independently of
CPU lowering. A direct target's platform veneer owns its entry convention,
syscall or system-library boundary, error rules, and process exit. Missing host
bindings and missing ABI veneers are different gaps and neither is repaired by
selecting the other layer.

## Shared libraries and partial modules

`--library FILE.zx` writes a thin bundle checked against that exact library.
`--include-library FILE.zx` writes the composed program and assets as a
standalone bundle. Repeated libraries must provide disjoint module identities.

A shared library is pruned for its entry and retained roots. An add-on may
need further declarations from a module that already occurs in that library:
for example, a nested record type discarded by the library's entry. The
producer subtracts matching portable declarations individually, retaining new
functions, types, constants, globals, laws and proof certificates with their
required import links. Conflicting declarations reject the build. The loader
composes the disjoint fragments into one module with its original identity;
it rejects overlapping declarations, inconsistent imports and duplicate
library providers before execution. Source locations and private visibility
remain part of declaration identity. Existing library record layouts must
match exactly; this does not extend an already retained record's fields.

Ownership comes from the payload and supplied library boundaries, not from
module name or untrusted serialized annotations. The loader checks the
payload's capability and law tables against its own declarations, then
validates the composed graph. Capability names remain module/function pairs;
a capability called by both fragments appears once in the final requirement
list. Library executable bodies are shared after validation, with their original
compilation contexts retained by linked copies. Each instance
has one independent global state for the composed module, including both
library globals and add-on globals.

Versions 26 and 27 share one encoding. The writer stores 27 only in a thin
add-on that composes declarations into a module its library already provides,
and 26 in every other bundle, so hosts built before 27 still open standalone
bundles, included libraries and add-ons that share no module. Older hosts
reject a version 27 header before reading capability requirements or executing
initializers. This reader accepts both and rejects a version 26 add-on that
needs composition. The saved `.zir` version did not change: its declaration
encoding and the behavior of source and native builds are unchanged.
`tests/bundle_partial_modules.sh` exercises source and saved IR, generated C and
both ZX engines, with capability, private access, duplicate provider,
overlapping declaration, format rejection and instance isolation cases. With
`ZIRAN_ZX26_ROOT` naming a built toolchain from before version 27, it also
checks that host opens current plain bundles and refuses composed add-ons.

## Component identity, state, and replacement

A component is an ordinary `.zx` entry plus its declared host capabilities;
Ziran has no UI- or application-specific component names. `BundleCodeIdentity`
returns a 32-byte content identity for the bundle's immutable program bytes and
the exact supplied library identities, while `BundleEntryModuleName` and
`BundleEntryFunctionName` expose the entry identity. `BundleCapabilityAbiAt`
returns each capability's module, function, signature, argument types, return
type, varargs flag, and argument-read flag. `BundleAbiCompatible` requires the
same entry identity and entry signature and the same capability ABI set in both
directions. Format version checking happens while opening the bundle.
The `std/bundle_host` wrappers expose the same current C ABI to bundle code.

`BundleComponentOpen` retains immutable bundle code, copies the host bindings,
and creates one independently initialized portable-VM instance with a positive
work budget. Opening another component, including another instance of the same
bundle, does not share or reset its globals. The host owns component and handle
lifetimes; handles become stale when their generation is replaced or their
component closes, and running a stale handle fails without touching the current
instance.

`BundleInstanceExportState` snapshots all global values as recursive
`BundleStateValue` data. Owned strings, fixed arrays, records, enums, scalar
lanes, and SIMD vectors are deep-copied; pointers, slices, growable `Vec`
backing storage, and other borrowed or host resources are rejected. Snapshots
are bounded by the VM's representation depth and 16 MiB of copied payload.
`BundleStateValueBytes` reports that exported payload size, not allocator
overhead. A migration callback receives the old snapshot and a mutable snapshot
of the replacement's initialized state, then `BundleInstanceImportState`
validates and converts every value before changing any target global.

`BundleComponentReplace` is transactional at the component boundary. It checks
the ABI, retains the replacement, initializes it under the component budget,
exports both states, migrates and imports, and calls the optional verification
callback while the old instance remains live. Only after all those steps pass
does it close the old instance, retain the replacement as current, and advance
the handle generation. ABI, initialization, state, migration, import, or
verification failure leaves the previous instance and bundle live. Callers
serialize run and replacement operations for one component; distinct components
remain independent. `tests/bundle_component.sh` builds source through checked
IR into `.zx`, proves independent state, successful nested-state migration,
rollback on migration, schema, resource, initialization, and verification
failure, stale-reference rejection, typed-ABI rejection, and an unrelated live
component. It prints environment-dependent load, instantiate, and swap timings.

`BundleComponentLiveValueBytes` reports the live portable instance's global
slot storage plus VM-owned record, array, and string payload. It deliberately
does not claim shared immutable code, bundle bytes, host allocations, compiler
cache, or total process RSS.

<a id="native-execution"></a>
## Current generated-C bundle execution

On Linux and Android hosts with x86-64, 32-bit ARM or ARM64 processors,
`libziran` currently runs suitable bundles through generated C.
`BundleInstantiate` lowers the
verified program to C with the backend `ziran build --target=c` uses,
compiles it in memory with the bundled TinyCC (`third_party/tinycc`, LGPL-2.1;
applications that ship it include its notice) and calls it. Host
capabilities receive the same `VmHostValue` shapes the portable VM passes.

Compiled code is cached under a key of the SHA-256 of the bundle's bytes
before its assets, its libraries, the CPU and the compiler's own sources, in
`$ZIRAN_NATIVE_CACHE`, `$XDG_CACHE_HOME/ziran-native`, `~/.cache/ziran-native`
or `$TMPDIR/ziran-native-cache`; `ZIRAN_NATIVE_CACHE=0` turns it off. Kryon's
Android hosts point `TMPDIR` at the app's private files. Only this compiler
writes that code, from a checked program, so a bundle without libraries whose
code is cached opens without checking its program again; it is checked when
the cached code is gone or `BundleProgram` asks for the checked program.

A bundle that cannot be compiled fails with `zx.native`; there is no
fallback within this generated-C route. Runtime failures stop the run as in the VM: bounds, unreachable
code, a host breaking the value contract, the step limit and nested calls
filling the stack. Generated code counts the step limit in function calls and
loop iterations rather than statements; a loop over literal bounds that
nothing else leaves or steps is charged all its iterations as it starts. It
does not check pointer lifetimes at run time: it trusts bundle code as a
generated-C build of the program would.

`#parallel for` regions run on worker threads each instance starts at its
first region, one per processor up to 16; `ZIRAN_PAR_THREADS` in the host's
environment sets the count, and 1 runs regions in place. The calling thread
waits while the workers take chunks of the range in order. A failure inside
a worker stops the run with the failure a serial run would meet first. Each
worker counts steps against what the run has left, and the run is charged
for all of them when the region ends.

Windows and macOS hosts run the portable VM, and `zi2zx run --vm` and
`libziran-vm.a` run it everywhere for tests of the VM itself.
`ZIRAN_NATIVE_TIMING=1` reports where compilation time goes and
`ZIRAN_NATIVE_KEEP=1` keeps the generated C.

## Experimental direct-native route

The direct-native assembly work starts from linked, verified portable ZX
instructions and lowers them through machine IR to target machine code. It is
not the generated-C route above. Linux x86-64 is the first target row, and the
implemented allocator, allocated-program verifier, encoder, ELF writer, and
first console veneer are pieces of that row. The opt-in static ELF owns
`_start`, retries Linux `write` syscall 1 on partial progress and `EINTR`, and
uses `exit_group` syscall 231; it has no libc, CRT, interpreter, or dynamic
dependency. Its target-owned formatter covers literal byte strings/text views,
bools, all fixed-width signed and unsigned integers, and `%%`. Floating-point
formatting and other target service rows remain stable named gaps. Complete
language/call coverage, native ZX loading, installation, and the general
per-row cutover evidence are still required before the row or overall pipeline
is complete.

### Linked code in the browser

A browser page cannot compile at run time, so the bundles it ships are
compiled when the page is built. `zi2zx native-c [--embedded] -o DIR
file.zx ...` writes each bundle's checked program as the same C, one folder
per program listed in `DIR/programs.txt`, with `zx_native.json` naming its
sources and compile flags; `--embedded` adds the bundles packed in it that
open without libraries, such as a collection's games and Apps controller.
`scripts/linked-zx.py DIR --output OBJ --cc emcc --nm emnm` compiles them,
renaming each program's external symbols under a prefix of its own so
programs and the host's own compiled modules never collide, and prints the
objects to link into the page beside the WebAssembly `libziran`.

Each program's constructor registers it under the identity above. When a
page opens a bundle with that identity, it opens without checking again and
`BundleInstantiate` runs it as the linked code; every other bundle runs on
the portable VM. The immutable program and its compiled services are shared,
while every linked instance owns a private copy of the global data section and
swaps that copy in only at its run boundary. `#parallel for` regions run in
place, and runtime failures stop the run as natively.
`tests/zx_linked_web.sh` checks independent coexistent linked instances end to
end with emcc and node. Lifecycle components use the portable VM explicitly,
so nested state and replacement semantics are the same in native, WebAssembly,
and linked-browser hosts.

## WebAssembly execution tier

A host that installs a WebAssembly provider with `ZxWasmSetPlatform`
(`cmd/zir/zir_zx_wasm_platform.h`) runs suitable bundles as WebAssembly:
`BundleInstantiate` lowers the bundle's verified IR to a wasm32 module
with the same emitter as the `zi2wasm` tool, instantiates it through
the provider, and counts the run budget like the native tier, one
function entry and one loop iteration at a time. The current tier
supports scalars, owned strings, fixed arrays, records, and typed
`s64 -> s64` host capabilities. Each instance keeps its own bindings and
linear memory; a state table identifies its globals for owned export and
import. Other constructs fail with `zx.wasm` while the provider is selected;
the runtime does not start the portable VM. Hosts configured without a
provider use their VM mode, and embedders can select the VM explicitly with
`BundleInstantiatePortable`. `tests/zx_wasm_engine_test.c` checks the seam
and hard provider rejection, and
`tests/zx_web_lifecycle.sh` runs source through ZIR and `.zx` in a
private headless Chromium without Node. It checks actual Wasm execution,
nested state migration through the provider's memory interface, independent
linked instances, and portable component replacement and rollback in both
browser hosts.

## Embedded files and application hosts

`--define NAME` selects the same source branches as the native compiler.
`--asset-dir assets=assets` embeds regular files recursively under `assets/`.
The argument's left side is the path prefix inside the bundle, and its right
side is the source directory or file. Multiple mappings may be supplied.
Names are sorted bytewise for deterministic output. Duplicate names,
symlinks, absolute paths, backslashes, empty components, and `.` or `..`
components are rejected.

After the ZIR, both versions store a little-endian u32 file count. Each
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
program launches with `ziran run app.zx` after the desktop provider is installed,
including embedded images. See
[Kryon project profiles](https://github.com/kryonlabs/kryon/blob/master/docs/PROJECTS.md).
Application-specific storage, audio, and other native interfaces still require
declared host capabilities with portable signatures. A native application's
pointer casts or calls through native callbacks cannot simply be bundled.
