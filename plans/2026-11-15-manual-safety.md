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

## Inventory: every defensive check today

| Where | What it checks | Default after this plan |
| --- | --- | --- |
| `cmd/compiler_machine_lower.zi` `LowerResolve`, `LowerMemory`, `LowerBytesEqual`, platform-service arguments in `LowerCall` | Emits `PointerResolve` before every load, store, zero, byte compare and service pointer | Not emitted; the pointer is the address |
| `LowerAllocate` | `ArenaAllocate` + `PointerCreate` (slot on the frame chain) | Arena bytes only; the raw address is the pointer |
| `LowerElement`, `ProvenBound`, `ViewAddress`, `MemberOperands` | Index checked unless `compiler_lowered_proven.zi` proves it | Unchecked operand forms (the existing proven forms) |
| `cmd/compiler_x86_64.zi` `PointerResolve`, `PointerDirect`, `PointerCreate`, `CreateSlot`, `RetireSlot`, `PointerFrameEnter/Leave/Step` | Slot, version, liveness, writability, extent; chain retirement at `Return` | Unreachable unless checked; frame enter/leave keep only the arena mark |
| x86 `Resize` → `CheckHeapSlot`, `ResizeSlots`, `RetireToken`, `HeapSlot` | Old block is a live heap slot at offset 0; retire and re-slot | Raw pointer in, raw pointer out |
| x86 `ElementAddress`, `MemberMemory` index compare + `TrapReserved - 1` | Index below count, subview `low <= high <= count` | Selected only by the checked operand forms |
| `cmd/compiler_aarch64.zi` (same procedures, lines ~1577–2230) | Same as x86 | Same as x86 |
| `cmd/compiler_machine_verify.zi` `CheckedMemory` (+ `CheckedAddress`, `DirectProven`, `DirectRoom`, `ResolvedView`) | Every access goes through a resolve or a proven direct base | Obligation only for a checked program |
| `cmd/compiler_lowered_proven.zi` | Facts that remove checks | Built only when something is checked |
| `cmd/compiler_machine_image.zi`, `cmd/compiler_machine_elf.zi`, `cmd/zir/zir_host_native.c` (`native_pointers_open`, `native_resolve`, slot table) | Pointer table, globals as constant tokens | Table only for a checked image; arena kept |
| `cmd/zir/zir_host_exec.c` `native_resolve` / `native_create` | Host views resolved from tokens; returned views get read-only slots | Not installed: raw addresses cross (the NULL-resolver path) |
| `cmd/zx2wasm/zx_wasm_lower.c` `checked_index`, view checks | Index and view compare + `unreachable` | Off unless checked; Wasm's own sandbox stays |
| `include/zir_bounds.h` `ZIRAN_INDEX`, `ZIRAN_EMPTY_INDEX`; `include/zir_slice.h` `SliceIndex`, `SliceRange`; emitters `cmd/zir/zir_emit_expr.c`, `zir_emit_value.c` | Index and range checks with `abort()` | Plain `base[index]`, `SliceRangeAt`; checked helpers only where asked |
| Go (`cmd/zi2go`) | Go's own implicit bounds checks | `-gcflags=-B` on Ziran's `go build`; explicit checks only where asked |
| `std/binary.zi` `ReadU16LE`… via `TryRead*`; `std/http_get_linux.zi` `GetHttps` via `GetHttpsCapped` | Validation forced on plain readers | Plain readers are plain; `TryRead*`, `*Capped`, `Validate*` stay opt-in |
| Portable engine `cmd/compiler_exec.zi` | Address-table tokens, liveness, extent, element type | Stays checked: it is the reference and test runner, and its pointer model is the table |

Kept everywhere: divide by zero and `INT_MIN / -1`, shift count, float
conversion range, `unreachable`, fuel, the stack limit and the frame-arena
limit. They define results or bound resources rather than guard memory.
Whether shift and conversion become C-style undefined is an owner question,
not part of this plan.

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
- ZX revision 29 records both: a header flag (the build was `--checks`) and
  the per-instruction bit. `zir-inspect` prints the flag. The native loader,
  `zx2native`, `zx2wasm` and the engine read them; native builds the pointer
  table only for a checked bundle.
- `MachineProgram.checks` gates `CheckedMemory` and token lowering;
  per-instruction bits select checked element forms.

## The `.zx` tension

A downloaded `.zx` built without checks can do anything a native program can:
it runs as machine code in the player's process. Owner's answer: performance
and full optimization first, most apps come from trusted channels, so bundles
are unchecked by default too. Optional last item: the loader refuses an
unchecked bundle the user has marked untrusted (it does not add checks; a
bundle is checked only if built with `--checks`).

## Stages (each pushed)

a. **Switch.** `--checks` on `ziran build|run|bundle|test`, `zi2zx`, `zi2c`,
   `zi2go`; `LirProgram.checks`, ZX revision 29 header flag, read by
   `zx2native`, the native loader, `zx2wasm`, engine. C and Go output take the
   flag. Default off; behavior unchanged except where (b) lands.
b. **Native unchecked.** Off: raw pointers, no resolve/create/retire/slot
   table, raw `Resize`, unchecked element forms, `CheckedMemory` skipped,
   no host resolver. Delete what only the default path reached; keep the
   checked path for `--checks`. Wasm, C and Go defaults in the same stage.
   Benchmark before and after (all six kernels, Omega, 11 samples).
c. **Granularity.** Front-end `#checks` (module, block/statement, access),
   ZIR/LIR/ZX bits, native, Wasm and C honor per-access checks; Go emits an
   explicit compare where asked.
d. **Tests.** Trap and hostile-twin cases (`proven_pointer`, `native_run`
   pointer fixtures, `zx_lowered_parity`, bounds cases) run with `--checks`.
   New cases: default native output has no `shr 0x2c`/resolve sequence and no
   index trap; default C has no `ZIRAN_INDEX`/`SliceIndex`; each `#checks`
   level adds checks exactly where asked.
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
