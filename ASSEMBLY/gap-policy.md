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
