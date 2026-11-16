# Manual safety by default, checks at full granularity (2026-11-15)

Owner, verbatim: "wer ar getitng closer to see how to improev vectors , we
dont want ziran to be a safde language we want manual saftey like in c if we
want like hva teh full g ranuality with optional safety". Earlier: "as long
as its good code safety i not neceasysr we nede to optimez" and "make sure we
oly have guardarils wher ew enede tehm". On downloaded bundles: "well yeah
that acutalyl sounds prtety good acuse we want preframnce and fully
optimaziton and mosta pps will be from our secur channels".

Ziran is not a memory-safe language. Default output on every compiled
backend (native x86-64 and AArch64, in-memory and ELF; Wasm; C; Go) has no
defensive runtime checks: pointers and indexes are raw, as in C. Checks are
opt-in for the whole build, a module, a block or statement, or one access.
With checks on, today's traps, named diagnostics and hostile-twin tests are
unchanged. Results stay correct; only defensive checks become optional.

## State

Stages a and b are done: default output on native x86-64 and AArch64, Wasm,
C and Ziran's own Go checks carries no defensive check, and `--checks` builds
today's checked program (tokens, every bounds check, the traps and the
hostile-twin tests). The ZX header flag records it. The portable engine stays
checked: it is the reference and test runner, and its pointer model is the
address table.

Kept everywhere: divide by zero and `INT_MIN / -1`, shift count, float
conversion range, `unreachable`, fuel, the stack limit and the frame-arena
limit. They define results or bound resources rather than guard memory.
Whether shift and conversion become C-style undefined is an owner question.

Still checked by default (open):

- SIMD gathers: `zi_simd_gather_check` in C (`include/ziran_simd.h`) and Go,
  and Go's `zi_simd_check` helpers.
- Go's own implicit bounds checks: Ziran does not run `go build`; the Go
  toolchain removes them with `-gcflags=-B`. Unchecked Go indexing through
  `unsafe` is possible but not planned.
- Wasm host-boundary checks on returned views and block limits: these guard
  the module against its host, not program bugs.
- Plain `std/binary.zi` `Read*LE` and `std/http_get_linux.zi` `GetHttps`
  still route through the validated forms (stage f).
- AArch64 unchecked `Resize` and frames have encoder coverage only through
  the checked fixture; device parity runs on the Pixel lane.

## Design

Two things differ in kind, and the syntax says so:

- **Pointer checks** (dangling locals, use after free, stale versions,
  read-only stores) need tokens, which change what a pointer is. They are
  whole-build: `--checks`.
- **Bounds checks** (array, string, slice and view indexes, slice ranges,
  dynamic aggregate members) need nothing but the count beside the index.
  They work at every level.

Syntax, one directive in four places:

```zi
ziran build --checks app.zi       // whole build: tokens + every bounds check

#checks                           // top level of a module: its procedures
Parse :: (bytes: []u8) { ... }

#checks { total += values[i] }    // a block or statement
x := #checks values[i]            // one index or slice access
```

`#checks` follows the existing directive style (`#scope_export` at top level,
`#if` before a block, `#insert` before a statement). No `#checks off`: the
default is already off.

Representation:

- ZIR expressions carry a `checked` bit on index and slice nodes, set by the
  front end from the directive scope.
- LIR instructions gain `checked: bool` (element address, view address,
  dynamic extract/insert). The program gains `checks: bool`.
- The LIR codec's header flag 1 records a `--checks` build (done); the
  per-instruction bit joins the instruction encoding in stage c. The native
  loader builds the pointer table only for a checked bundle (done).
- `MachineProgram.checks` gates `CheckedMemory` and token lowering (done);
  per-instruction bits select checked element forms (stage c).

## The `.zx` tension

A downloaded `.zx` built without checks can do anything a native program can:
it runs as machine code in the player's process. Owner's answer: performance
and full optimization first, most apps come from trusted channels, so bundles
are unchecked by default too. Optional last item: the loader refuses an
unchecked bundle the user has marked untrusted (it does not add checks; a
bundle is checked only if built with `--checks`).

## Stages (each pushed)

c. **Granularity.** Front-end `#checks` (module, block/statement, access),
   ZIR/LIR/ZX bits, native, Wasm and C honor per-access checks; Go emits an
   explicit compare where asked.
d. **Tests.** New cases: default native output has no resolve sequence and
   no index trap; each `#checks` level adds checks exactly where asked. (The
   trap and hostile-twin cases already build with `--checks`; the default C
   build's lack of checks is in `tests/default_bounds.sh`.)
e. **Docs and site.** `docs/LAWS.md`, `USAGE.md`, `ZX.md`,
   `IMPLEMENTATION_STATUS.md`, site guide: Ziran is manually safe like C;
   checks are opt-in. No page calls Ziran memory-safe.
f. **Std readers.** Plain `Read*LE` and `GetHttps` stop routing through the
   validated forms; audit callers that relied on a short read giving 0.
g. **Optional.** Loader refusal of unchecked bundles marked untrusted.

## Coordination

- Native speed lane (`claude-ziran-records-aggregate-speed-2`) owns
  `ElementAddress`, `MemberMemory`, aggregate lowering, fuel, `Return`,
  `Prologue` and `Call` in the encoder. This plan changes `LowerResolve`,
  `LowerMemory`, `LowerAllocate`, `LowerResize`, `LowerBytesEqual`,
  `ProvenBound`, `PointerFrameEnter/Leave`, `Resize` and `CheckedMemory`, and
  selects the existing unchecked element forms from lowering so their
  encoders stay untouched.
- AArch64 loader lane (`claude-ziran-m4-aarch64-loader`) owns
  `compiler_aarch64.zi`, `Makefile`, `assembly.mk`, `zi2zx/main.c`,
  `zir_host*`; edits there are agreed with it first.
- Work happens in `work/msafe` and lands with only these hunks.

## Open questions carried from the trust-tier and checked-pointer plans

- With `--checks`, native does not check a pointer's element type on access
  (the engine does); the load-time verifier covers verified programs.
- AArch64 does not yet elide proven resolves under `--checks`.
- Records' proven `extract`/`insert` indices under `--checks` belong to the
  native speed lane.
