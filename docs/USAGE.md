# Compiler and standard library guide

[Back to the README](../README.md) · [Implementation status](IMPLEMENTATION_STATUS.md)

## Build and test

Run `make` to build the current toolchain and `make check` for its language,
backend, and portable bundle tests. `make check` runs independent test scripts
with four workers by default; set `CHECK_JOBS=1` to run them serially or choose
another positive worker count.

## Compiler commands

The compiler commands are `zi2zir` (including
`--check-only`), `zi2c`, `zi2cpp`, `zi2go`, `zi2rust`, `zi2py`, and `zi2zib`.
`zi-fmt` formats source. `ziran guide` prints a short reference from the
installed compiler, including current safety and target limits.
`ziran capabilities --target=go --json` reports a versioned machine-readable
target summary; see the [capability schema](CAPABILITIES.md).
`ziran api --json --root src src/main.zi` checks an entry and its imports,
then reports their public types and procedure signatures; see the
[API query schema](API_DISCOVERY.md).
`ziran check|ir|api|inspect|build|bundle|run|fmt` remains a convenience dispatcher
for the same tools. [Cross-target benchmarks](../bench/README.md) measure the
compiler, downstream toolchains, and validated runtime cases separately.

## Generate native code

Use `ziran build --target=c` for C99 output. Use
`ziran build --target=py --exe --entry module:function -o DIR` for Python 3.10
source that `python3 DIR` runs; it needs only the standard library. Use
`ziran build --target=plan9-c` for the experimental Plan 9 C output path.
Pass `--entry module:function` to a C99 build to retain functions, types,
globals, and constants reachable from that entry. A native host implementation
of a called foreign function is retained when it is among the input modules.
Type-only linked modules emit headers without empty C files. Without `--entry`,
all checked declarations and module C files are emitted as before. Runtime
branches in reachable functions remain; this pass does not specialize them.

`ziran compile-commands DIR` writes `DIR/compile_commands.json` for the C
files in `DIR`, so clangd and other editors can read generated code with the
flags it builds with. `--compiler "cc -O2"` sets the compiler and its extra
flags (default `$CC`, then `cc`), `--include DIR` names Ziran's headers
(default: the pinned toolchain's with `--project`, otherwise the checkout
beside the launcher), and `-o FILE` writes elsewhere.

## Save and inspect checked modules

`ziran ir --entry module:function` saves that reachable checked graph as
per-module `.zir` files before native code generation.
Saved `.zir` files are binary. `ziran inspect path/to/module.zir` shows their
declarations, statements, and checked expression links as readable text;
`ziran inspect --hex path/to/module.zir` shows bytes and offsets. Both views
read the saved file and leave it unchanged.

## Imports

For ordinary imports, pass the entry file and a library directory with
`--module-path DIR` (repeat for multiple directories). `check`, `ir`,
`api`, `build`, and `bundle` load extensionless `#import "module"` dependencies
transitively from `.zi` or saved `.zir`. For example:

```sh
build/bin/zi2zir --check-only --root app --module-path ../kryon/src/ui app/main.zi
```

A package-qualified import such as `#import "kryon/Widgets"` resolves
through the package's dependencies in a `--project` build. Outside a
project, name the package's module directory instead:
`--module-path kryon=../kryon/src/ui` serves `kryon/NAME` imports from that
directory (and only those; repeat it for more directories). A package
builds the same way in both modes when each export has its module's name.

`#import, file "../lib/helper.zi";` loads an explicit source file relative to
the importing file. Saved `.zir` builds resolve its module by name from the
saved IR directory or a module path.
`Helper :: #import "helper";` and
`Helper :: #import, file "../lib/helper.zi";` expose public declarations as
`Helper.Name` without adding them to the unqualified scope.
`Helper :: #import, dir "../lib/helper";` loads that directory's `module.zi`.
An ordinary `#import "helper"` also discovers `helper/module.zi` on a module
search path.
`#load "relative/file.zi";` adds another file to the current module. Loads
can nest, and imports inside loaded files resolve relative to those files.
`#import, string "Value :: () -> s32 { return 42 }";` compiles source text
as a module. It also accepts a raw `#string` body, and
`Generated :: #import, string "...";` exposes its declarations as
`Generated.Name`.

## Foreign functions

Ordinary `#import` accepts a module identifier. C headers are no longer
imported as source modules; foreign procedures use `#system_library` and
`#foreign` declarations.
For a native symbol with an unmangled name, put `#program_export` on its own
line immediately before the function declaration.
Foreign procedures use a Jai-style library declaration and `#foreign`:

```jai
libc :: #system_library "libc";
Abs :: (value: s32) -> s32 #foreign libc "abs";
```

### Go packages

For a Go package, use an explicit `go:` library path. This also supports
standard packages whose import paths contain no slash:

```jai
strings :: #system_library "go:strings";
TrimSpace :: (value: string) -> string #foreign strings "TrimSpace";
```

The Go target emits a direct package call. These declarations remain Go
imports in saved `.zir`; they are not portable host capabilities.

### Foreign Go types

Declare an opaque Go type alongside the package's foreign procedures:

```jai
json :: #system_library "go:encoding/json";
RawMessage :: #type #foreign json "RawMessage";
Envelope :: struct {
    payload: RawMessage #go_tag "json:\"payload\""
}
```

The Go target emits a type alias, preserving the imported type's identity,
methods, interface behavior and zero value. Ziran can store, pass, return and
assign these values, including across imports. Fields and layout remain opaque:
record literals and `size_of` are unavailable. These declarations require an
explicit `go:` package and are preserved in checked IR. Other targets reject
foreign Go types; an entry build can discard them when they are unused.

A slice can be explicitly converted to or from a foreign Go type whose
underlying storage supports the conversion. The Go compiler checks that
underlying type. Conversions preserve native length, capacity, nil values
and shared backing storage; returned native slices keep that storage alive:

```jai
Wrap :: (value: []u8) -> RawMessage { return cast(RawMessage)value }
Unwrap :: (value: RawMessage) -> []u8 { return cast([]u8)value }
```

Fixed-array casts, slice-to-slice casts and owned vector elements are rejected.

### Go receivers, fields and multiple results

A quoted method expression supplies its receiver as the first parameter.
The receiver must match an opaque type declared from the same Go package:

```jai
sync :: #system_library "go:sync";
Mutex :: #type #foreign sync "Mutex";
Lock :: (value: *Mutex) #foreign sync "(*Mutex).Lock";
Unlock :: (value: *Mutex) #foreign sync "(*Mutex).Unlock";
```

Use `#go_field` before `#foreign` to read a native field through a typed
getter. It takes exactly one receiver and returns the field value. The Go
compiler verifies that the field exists and has the declared storage type:

```jai
http :: #system_library "go:net/http";
Request :: #type #foreign http "Request";
Remote :: (request: *Request) -> string #go_field #foreign http "(*Request).RemoteAddr";
```

Use `#go_results` to pack a Go function's multiple results into a concrete
record. Record fields correspond to Go results in declaration order, including
native error interfaces. It also works with method expressions:

```jai
#import "go_types"
net :: #system_library "go:net";
HostPort :: struct { host: string; port: string; error: Error }
Split :: (address: string) -> HostPort #go_results #foreign net "SplitHostPort";
```

The result record must be nonempty, without owned vectors, `using` fields or
discard fields. Variadic declarations and combinations with `#go_field` are
rejected. Both attributes require explicit Go package targets. Checked IR
stores their typed signatures and attributes; generation from saved IR does
not infer them from diagnostic source text.

The standard modules `sync_go`, `time_go`, `random_go`, `text_go`, `net_go`
and `http_go` expose mutexes, native monotonic timestamps, cryptographic random
bytes, copied byte strings, IP primitives, HTTP request fields and headers.
`atomic_go` provides native `Uint64` counters with atomic `Add` and `Load`.
`http_go` also preserves the native `ResponseWriter` interface and exposes
response headers and `SetHeader`.
`time_go` provides parsing, UTC conversion and formatting while preserving
native `time.Time` values and parse errors. `context_go` preserves native
contexts and provides `Background`. `sql_go` exposes native database,
transaction, row and result handles, nullable strings, row iteration and
transaction cleanup. Declare query, execution and scan bindings with the
argument types required by the application; Go checks those native signatures.

### Go deferred foreign calls

Declare a void foreign procedure with `#go_defer` to schedule its native Go
call at the end of the calling function, including panic unwinding:

```jai
#import "sync_go"
sync :: #system_library "go:sync";
UnlockAtReturn :: (value: *Mutex) #go_defer #foreign sync "(*Mutex).Unlock";
Work :: (mutex: *Mutex) {
    Lock(mutex)
    UnlockAtReturn(mutex)
    // Work while the mutex is locked.
}
```

Arguments are evaluated and captured at the call statement. Scheduled calls
run in reverse order when that function returns or panics; a nested block
does not end the schedule. Declare and call the foreign binding in the
function that needs cleanup. A Ziran wrapper would schedule cleanup at the
wrapper's own return.

The binding requires an explicit Go package target, a void result and no
owned vector arguments or variadic parameters. `#go_results`, `#go_field`
and Go predeclared builtins cannot be combined with this attribute. Calls
must be standalone statements. Checked IR preserves the attribute, and
other targets reject reachable Go bindings. Ordinary Ziran `defer` keeps
its lexical scope cleanup semantics across targets.

### Go predeclared primitives

Typed foreign declarations in `go:builtin` can allocate Go heap objects and
slices/maps, copy bytes into a string, or read a native length:

```jai
builtin :: #system_library "go:builtin";
Allocate :: () -> *HostPort #foreign builtin "new";
Bytes :: (count: isize, capacity: isize) -> []u8 #foreign builtin "make";
Text :: (value: []u8) -> string #foreign builtin "string";
Length :: (value: []u8) -> isize #foreign builtin "len";
Append :: (values: []u8, value: u8) -> []u8 #foreign builtin "append";
Panic :: (value: Error) #foreign builtin "panic";
```

`new` takes no arguments and derives the allocated type from its pointer
result. `make` derives its slice or map type from the result; slices take a
length and optional capacity, and maps take an optional size hint. `len`
returns `isize` and accepts strings, arrays, slices, maps and declared foreign
Go types whose underlying type supports native `len`. Go compilation checks
the underlying operation for opaque types. These calls need no package import
and retain Go's allocation, zero-value and byte-copy behavior.
`append` takes a slice and one element, returns the same slice type, and keeps
Go's length, capacity and shared backing-storage behavior. Owned vector
elements are rejected; variadic slice expansion is not supported by this binding.
`panic` takes one non-owned value and returns void. It preserves the supplied
native Go value, including error identity, during panic unwinding.

### Go record metadata

Go record fields can carry reflection tags. The checked string is preserved
in saved IR and emitted as a Go struct tag; other targets keep the same fields
and ignore this Go metadata:

```jai
Response :: struct {
    userID: string #go_tag "json:\"user_id\""
    expiresAt: s64 #go_tag "json:\"expires_at,omitempty\""
}
```

### Host capabilities and visibility

Ziran also resolves `host_api :: #system_library "host_api";` to its host
capability bridge. `ziran bundle --bind caller:capability=provider:function`
can satisfy a portable host capability with an exported Ziran function in the
bundle; the provider must have the same signature.
Use `#scope_file`, `#scope_module`, and `#scope_export` to change the
visibility of following declarations.

## Multiline strings

Raw multiline text uses Jai's `#string` delimiter form. The body keeps its
whitespace and the newline before the closing delimiter:

```jai
Greeting :: #string END
Hello, "world"!
END;
```

## Standard library

Import a standard module as `#import "std/text"`. In a project the pinned
toolchain supplies it; a standalone command finds `text` on its
`--module-path`, so pass `--module-path std` from the Ziran checkout. The short
spelling `#import "text"` also works when no module of your own has that name.
Dependencies from other Git repositories import the same way, as
`#import "NAME/Module"`; see [Packages](PACKAGES.md).

### Text and UTF-8

`std/text.zi` supplies ASCII case folding, prefix matching, and substring
matching over immutable UTF-8 strings. Non-ASCII bytes compare unchanged. These
functions use only portable Ziran operations, so the same source builds for C,
C++, Go, and `.zib`. Add `--module-path std` when importing it from an app.
`std/string_range.zi` provides `Substring(source, start, length)` over borrowed
bytes, using Ziran's checked `source[low:high]` string range syntax. Choose
UTF-8 codepoint boundaries when the result must remain valid text.

`std/utf8.zi` advances through UTF-8 scalar boundaries and counts codepoints.
Invalid bytes advance one byte, which keeps a scanner moving while preserving
valid multibyte sequences.

### Generic values

`std/option.zi` and `std/result.zi` define generic records. Import a
template and create a concrete type with `Number :: Option(s32)` or
`Outcome :: Result(s32, string)`. An `Option` has `has_value` and `value`
fields; a `Result` has `is_ok`, `value`, and `error` fields. Check the status
field explicitly before reading the associated value.

`std/pair.zi` defines a generic record. Import it and write
`PairNumberText :: Pair(s32, string)` to create a concrete record
with `first: s32` and `second: string` fields.

A polymorphic procedure binds its type parameter through a direct `$T`, a
slice `[]$T`, or a pointer `*$T` parameter: `Sum :: (values: []$T) -> T`
specializes for whatever element type the caller passes.

### Sorting and queues

`std/sort.zi` sorts any slice whose elements support `<` in place with
`Sort(values[:])`, and searches a sorted slice with `LowerBound` and
`BinarySearch`. Sorting allocates nothing and is not stable.

`std/queue.zi` is an allocation-free bounded byte FIFO. Queue state is passed
and returned by value, and every operation receives the caller-owned backing
storage explicitly. Full pushes and empty pops are rejected without unwinding
the queue; partial byte pops report the number removed.

### JSON and ZIP

`std/json_scan.zi` supplies allocation-free JSON value skipping, object member
and array element lookup, string spans, and decimal number reading. It validates
the value shape and escapes while scanning; member names match unescaped ASCII
bytes. Callers keep the original input string and use byte offsets returned by
the scanner. Its tests compare source and saved-IR portable bundles, and the
same module is exercised through native C, C++, and Go by a downstream client.

`std/zip.zi` reads classic ZIP directories from caller-owned bytes, verifies
stored entries with CRC-32, and writes stored archives into caller-owned output.
It rejects split, encrypted, and malformed archives, and duplicate requested
entries. ZIP64 archives are unsupported.
It runs in portable bundles. `std/zip_linux.zi` adds raw DEFLATE extraction
through the system zlib library for 64-bit Linux native C builds; link those
builds with `-lz`. The caller supplies file I/O, memory limits, and which entry names
are meaningful to the application.
`std/zip_file_linux.zi` writes stored ZIP entries directly to a caller-owned
file from borrowed byte slices, including mapped files. The caller controls
file creation and publication. Its test checks the output with Python's ZIP
reader as well as Ziran's reader.

### HTTP and process capabilities

`std/net_http.zi` defines an explicit request/response host capability. A
platform adapter supplies transport and TLS; checked Ziran code owns request
construction and response interpretation. A missing host binding is reported
before a portable bundle runs.

`std/constant_time.zi` compares equal-length byte spans without branching on
byte values. Use fixed-size buffers for secret tags so unequal lengths remain
public shape rather than a secret-dependent oracle.

`std/process.zi` defines a line-oriented child-process capability with explicit
arguments, stdin, an optional credential binding, a timeout, and a desktop
isolation request. The host starts the child, yields output lines, and returns
its exit result. Its contract runs from source and saved `.zir` as a portable
bundle and through native C, C++, and Go mocks.

### Native Plan 9 files

Native `plan9-c` builds can import `std/file_plan9.zi` for positional byte
reads and writes, file size, read/update/replace handles, private files,
directory checks and creation, removal, and renaming without replacement.
`RenameNoReplace` changes only a basename within the same directory; it
rejects cross-directory moves. Paths reject embedded NUL bytes and must fit
the adapter's 4096-byte C buffer. These operations use native Plan 9 libc
and are not portable host capabilities. `tests/file_plan9.sh` checks source
and saved IR generation; TaijiOS's `rill-ziran-plan9-smoke` gate compiles
and runs the fixture with native `8c`/`8l`.

### Native Linux adapters

Native Linux C builds can import `std/file_linux.zi` for positional byte I/O,
`std/binary_linux.zi` for little-endian numbers, `std/date_time_linux.zi` for
Unix time, `std/random_linux.zi` for operating-system secure randomness, and
`std/byte_text_linux.zi` for borrowed byte-to-text views. `std/socket_linux.zi`
opens literal-IPv4 TCP sockets with bounded polling, cancellation, and I/O.
`std/timer_linux.zi` supplies monotonic millisecond timestamps and bounded,
interruptible sleeps; it does not expose wall-clock time.
The file module can create a private file exclusively, sync its contents, and
publish it through a hard link that fails if the destination already exists.
`std/byte_text_linux.zi` also borrows caller-owned C strings and byte buffers;
the caller must keep their memory alive. `std/mapped_file_linux.zi` maps a file
privately for bounded byte parsing without copying it. Its borrowed slice is
valid only until `UnmapFile`, and writes to that slice do not change the file.
`std/process_capture_linux.zi` captures a child process without a shell;
`std/net_http_linux.zi` uses it to send JSON over HTTPS through `curl`.
These native adapters keep libc calls out of applications and require glibc
Linux and a `curl` executable. Portable bundles should use the host capabilities
above instead.

### Native Go maps

Import `map_go` to use `Map(K, V)` with the Go target. Keys must be comparable;
values cannot own `Vec` storage. A map's zero value is nil. Copies share entries,
while replacing a map binding changes only that binding. Maps can be compared
with `null`, and their contents are accessed through these operations:

| Operation | Behavior |
|---|---|
| `MapInit(values)` | Allocate an empty map if the binding is nil |
| `MapSet(values, key, value)` | Insert or replace; allocate nil storage first |
| `MapGet(values, key)` | Read an entry, or return the value type's zero value |
| `MapLookup(values, key)` | Return `Option(V)`; import `option` to use this operation |
| `MapContains(values, key)` | Test key presence, including entries with zero values |
| `MapDelete(values, key)` | Remove an entry; missing keys are harmless |
| `MapClear(values)` | Remove every entry while preserving allocated storage |
| `MapCount(values)` | Return the entry count as `s64` |
| `MapKeys(values)` | Return an unordered `[]K` snapshot |

`MapInit` and `MapSet` require a mutable binding or field. Nil maps support
reads, membership checks, deletion and clearing. Map mutation in `#parallel`
regions is rejected because copies share storage. Other targets and portable
ABIs reject maps; checked `.zir` preserves their Go behavior.

Import `go_types` for the predeclared Go `Any` and `Error` interfaces. `Any`
accepts values that do not own `Vec` storage, including nested maps for JSON.
