# Ziran portable bundle (`.zib`)

This describes the target contract and the experimental subset that ships now.
See [Implementation status](IMPLEMENTATION_STATUS.md) for the remaining work.

## Experimental version 25

`zi2zib bundle --root DIR [--module-path DIR] [--bind caller:capability=provider:function] --entry module:function -o FILE file.zi|file.zir ...`
loads explicit inputs and their extensionless imports, then links reachable
modules. `zi2zib run FILE` validates and executes
zero-argument integer, bool, or void entry functions on a portable interpreter.
Called functions can use the IR scalar types `s32`, `s64`, `u8`, `u32`, `u64`,
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
`ZIB` plus a zero byte, a little-endian version, length-prefixed entry module
and function names, a host capability count and its required module/function
names, and a
length-prefixed version 48 `.zir` payload. Named law and waiver tables precede
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
writing the payload. Compile-time definitions and imported modules needed by
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
entry function. The standalone `zi2zib run` command has no host bindings and
rejects a call that needs one. A run is not bounded: like the program's native
builds, it continues until the entry returns. Only the web playground stops a
run after 1,000,000 statements (`VmRunBounded`), so a runaway loop cannot hang
the page, and it says so in the diagnostic. A function holds as many locals as
it declares. Calls nest until the C stack is nearly full, where the thread's
stack bounds are known (Linux); `zi2zib run` runs the program on a 256 MiB
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
source and saved IR. Fixed arrays with numeric or resolved integer
constant-expression capacities support defaults,
positional literals, element reads and writes, and value copies, including
nested arrays and `u8` arrays inside imported records. Indexing is bounds-checked. Version 24 is
experimental and has no compatibility promise. Host calls with procedure
slots remain unsupported. Default-initialized module globals of
portable value types work within one `BundleRun`; each call starts with fresh
global values. `BundleInstantiate` creates an instance whose globals persist
across `BundleInstanceRun` calls. Checked runtime global initializers run once
when an instance opens, after imported modules initialize. Complete
graphical runtime integration, slice fields/globals and unsupported host return shapes,
unresolved fixed-array bounds,
module state, custom `for_expansion` iteration, and `switch` remain unsupported.

Local slices may borrow fixed arrays or other slices, cross ordinary Ziran
function calls, return views of permitted backing storage, and read or write
indexed elements. The loader checks the existing slice lifetime rules; the VM
checks ranges and indexes during execution. A live view keeps its backing
array identity across an array value assignment.

## Target contract

A `.zib` is a versioned, portable executable bundle linked from checked
`.zir` modules. Its format identifier is `ZIB`. It contains the program's
portable executable content, data, entry point, linked module identities,
exports, and explicit host capability requirements. The exact instruction and
section encoding will be specified before the format is called stable.

The linker resolves imports and includes reachable code from ordinary
libraries. That includes Kryon only when the program imports it. A non-UI
command-line program is a first-class `.zib`; it launches without a graphical
host. Kryon UI code is portable Ziran code in the bundle, with rendering and
input supplied through declared host capabilities. The same bundle format
serves graphical and non-graphical programs.

Before execution, the loader validates the format version, section bounds,
symbols, entry point, and capability contract. Missing symbols, unsupported
versions, malformed bundles, and unavailable required capabilities fail with
diagnostics. A portable bundle cannot contain an undeclared dependency on
target-specific C, C++, or Go code. Hosts may implement the declared
capability interfaces with native libraries.

The old `.krb` cartridge is a Kryon-specific format. It is not renamed or
reinterpreted as `.zib`. Existing programs must be migrated to `.zi` and
recompiled through the Ziran pipeline.
