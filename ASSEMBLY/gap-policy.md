# Assembly-target gap and error policy

Status: normative for the direct assembly target and the revision-28 executable
path. This policy applies to every delivery milestone and completion matrix cell.

## Why gaps are not fallbacks

A fallback makes an unsupported path look successful. It can hide missing
lowering, CPU support, host services, or target validation behind a different
engine, compiler, or semantic implementation. The assembly target therefore has
no fallback state.

Each feature/target cell has exactly one of these states:

- **Implemented** — the named implementation ran on the required target and has
  fresh retained evidence.
- **Gap** — required work is missing. Selecting the cell fails before output or
  execution with its stable diagnostic.
- **Excluded** — the contract deliberately does not promise the feature for that
  target, and capability output names the exclusion.
- **Unknown** — implementation may exist, but the required execution evidence is
  absent. Unknown never counts as implemented.

`fallback`, `downgrade`, `skip`, `emulated`, `cross-compiled`, and `legacy engine`
are evidence annotations, not successful assembly-target states. A comparison
may run a legacy or reference engine only when the command selects it explicitly;
that result cannot close a direct-target cell.

## Required failure boundary

The compiler and loader reject a gap at the earliest boundary that has enough
information to identify it:

1. source/lowering rejects unsupported language operations;
2. target preflight rejects CPU, OS/ABI, runtime, capability, and native-extension
   mismatches;
3. object/link validation rejects missing formats, relocations, libraries, entry
   support, or signing facilities;
4. loading rejects unavailable executable-memory, cache, or host integration.

Every rejection has a stable diagnostic code and reports the selected target,
engine, missing feature or facility, and source origin when one exists. It must
not run module initializers, call a host, open a window, execute an entry, invoke
another backend, or overwrite the last valid output.

Examples of required errors include:

- a native target without a matching `#asm` implementation;
- a required ISA feature absent from the selected CPU contract;
- a parallel or GPU region without its promised target implementation;
- a WebAssembly operation outside the direct Wasm lowering;
- a missing host capability or foreign ABI adapter;
- an unsupported object format, relocation, linker input, or platform signing
  facility;
- a request to run a legacy or incompatible executable revision.

## Host capabilities and ABI veneers

Host capabilities and platform ABI veneers are separate contracts. A host
capability is a portable, typed service named by linked ZX instructions and
bound by an embedder or runtime provider. A platform veneer is direct-target
code for entry, calling conventions, syscalls or system libraries, error
handling, and process exit. CPU instruction selection is a third concern.

An implemented host capability does not satisfy a missing direct-target veneer,
and an implemented veneer does not bind a portable host capability. Generated C
or Go bindings are also independent: they may produce working platform binaries
without closing the direct-native target cell. A missing veneer, unsupported
service, or unavailable binding is rejected at preflight or link time without
starting another engine.

The opt-in Linux x86-64 direct console veneer is implemented around an owned
`_start`, Linux `write` syscall 1, `exit_group` syscall 231, and no libc, CRT,
or dynamic linker. Its `write_all` retries `EINTR` and partial progress; other
write failures take the image's fixed trap exit. This is execution evidence for
the experimental AOT slice, not a completed target-row or native-ZX claim.

The implemented direct print subset covers read-only literal format strings,
strings and text views by exact byte length, booleans, every fixed signed and
unsigned integer type, and `%%`. Floating-point formatting remains
`assembly.gap.native-float-format` until its exact locale-independent formatter
and tests exist. A missing target veneer or service reports
`assembly.gap.native-platform-service-unavailable`. The machine-IR path uses
the corresponding `assembly.gap.machine-*` codes. None permits a C/Go route or
portable interpreter to start.

## Target preflight codes

Target preflight uses three stable codes, all hard errors:

- `assembly.gap.target-unimplemented` — the selected CPU/OS row is Gap,
  Excluded or Unknown. Only an Implemented row passes.
- `assembly.gap.cpu-feature` — the row does not implement a required CPU
  feature, including a feature of another CPU family.
- `assembly.gap.os-service` — the row does not supply a required operating
  system service.

The message names the selected row, the `assembly` engine, the row state and
the missing feature or service. The check needs no input, so it runs before
any source or saved `.zir` is read and both report the same gap. `ziran
capabilities --json` lists every row's state and code under
`assembly_targets`; `ziran explain` describes each code.

## Explicit engine and target selection

Portable execution, native execution, direct Wasm, C, and Go are separate
targets. One may be the documented default of a command, but none is a rescue
path for another. An explicit native request never starts the interpreter; an
explicit portable request never invokes native code; direct Wasm never routes
through generated C. Old executable revisions produce a rebuild error instead of
selecting an old runtime.

CPU variants are also explicit target contracts. If the chosen variant requires
features the selected or running CPU lacks, preflight fails. A lower-feature
variant may be selected only when it is an independently declared implementation
of that exact target cell, not a portable body or another engine disguised as
successful native execution.

## Tests and completion accounting

Every Gap cell needs a negative fixture that checks:

- the stable diagnostic code and relevant target/feature fields;
- failure before initializer, host, entry, or output replacement side effects;
- identical rejection from source and saved `.zir` where both inputs apply;
- capability and inspection output reporting the same gap.

Every Implemented cell needs positive source/saved-IR execution evidence on its
named target. Optional tools or hardware may leave a cell Unknown, but tests may
not silently skip it and completion reports may not round it up.

Milestones and final completion count only Implemented cells and explicit
Excluded cells. Any Gap, Unknown, fallback, downgrade, skip, or substituted
engine keeps its gate open.
