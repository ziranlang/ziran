# Checked native pointers (2026-11-15)

Owner decision (2026-11-14, verbatim): "fallbakcs are illegal". ZX is shared
like `.wasm`; each device translates it to native code. Backend `Auto` chooses
by platform only: browser → Wasm, a CPU with a native backend → Native, a CPU
without one → Engine. A native gap on a supported CPU is a named error.

Coordinator assignment: native code must enforce the engine's pointer and
freed-block checks (dangling locals, use after free, bounds, read-only bytes),
so `New`, `free` and `Vec` programs run natively. Then `ziran run`, `zi2zx run`
and the embedding API's `Auto` become Native on linux-x86_64 (Kryon players
included), and `zx2native` ELF output uses the same preflight. Nothing runs
unchecked; what still cannot run is a named gap with a test.

## Current state (uncommitted, 2026-11-15)

- Native in-memory loader, stack limit, fuel, traps with origins, typed host
  stubs, the Resize runtime and the code cache work (`cmd/zir/zir_host_native.c`).
- `cmd/compiler_native_safety.zi` is a conservative preflight: no Resize, no
  frame pointer that escapes, no frame pointer handed to a host as a raw
  handle. `--backend native` refuses everything else with
  `assembly.gap.native-memory-checks`. The default is still the engine.
- `zx2native` ELF output is not gated yet; it runs unchecked programs.

## Engine semantics to match (`cmd/compiler_exec.zi`)

A pointer is a token naming an address-table entry: global, frame or heap
block, generation, offset, extent, type, writability. Frame exit and heap
resize/free end a generation, so every derived address stops being valid.
Loads and stores check liveness, extent and writability; element and field
derivation check extent. Slot reuse bumps a version that is part of the
token; an exhausted version traps.

## Design: tokens with an offset field

A native data pointer stays one 64-bit word (records, arrays and the host ABI
keep their layouts):

    bit 63..44  slot      (20 bits; slot 0 is never valid, so null traps)
    bit 43..32  version   (12 bits; a slot is retired, not reused, after 4095)
    bit 31..0   offset    (bytes from the allocation start, < 4 GiB)

Each instance owns a slot table in its data section, one 16-byte entry per
slot: `base: u64`, `size: u32`, `version: u16`, `flags: u16` (live,
writable). Function pointers stay raw code addresses.

- **Resolve** (new machine opcode, before every Load, Store and BytesEqual
  operand): `slot = p >> 44`, compare the entry's version with
  `(p >> 32) & 0xfff`, check `offset + width <= size` (and writable for
  stores), produce `base + offset`. About ten instructions and one 16-byte
  load; a failure takes the bounds trap.
- **Derivation** (FieldAddress, ElementAddress): offset arithmetic on the
  token with the extent checked at derivation, as the engine does; the slot
  and version bits never change.
- **Globals**: slot `1 + global` with version 0, created at instance open;
  `GlobalAddress` is a constant. Read-only globals are entries without the
  writable flag.
- **Frame storage**: `Allocate` calls the runtime to take a slot for the
  stack object and links it into the frame's slot chain (head in a frame
  slot); `Return` (and the trap/longjmp path) retires the chain: version
  bump, slot back on the free list. Executing an `Allocate` again retires
  the previous slot first.
- **Heap**: `Resize` returns a token; resizing or freeing retires the old
  slot. Blocks keep the per-instance accounting in `zir_host_native.c`.
- **Hosts and platform services**: the bridge resolves tokens for views it
  copies (strings, slices) and hands raw-pointer arguments over as opaque
  tokens. Views a host returns get slots for the retained payload.
- **ELF**: the same table and runtime entry points, provided by the image
  writer's runtime section, so AOT and in-memory output share every check.

## Refined machine design (all checks inline, shared by ELF and in-memory)

Engine facts that shape it: every execution of `allocate` gets fresh storage
that stays valid until its frame exits (a loop keeps earlier iterations'
cells), so native frame storage comes from a bump arena, not a fixed stack
slot (today native reuses one slot per `allocate`, which aliases them). A
host-returned view is instance-owned read-only storage.

Per-instance memory, in the RW data section and lazily zeroed space after it
(ELF: BSS; in-memory: one reserved mapping):

    table header: free_head (slot), next_unused (slot), capacity
    table entries: 32 bytes each: base u64, size u32, version u16, flags u16
                   (live, writable, heap), next u32 (free list / frame chain)
    globals: slots 1..G, written by the ELF writer / loader before entry
    frame arena: bump region, top word in data, limit = engine value bytes

Machine opcodes (lowering inserts them; the encoder emits fixed sequences
through the reserved r11 and an explicit r10 clobber):

- `PointerResolve raw, token, width|width-value, writable`: slot < capacity,
  version equal, live, offset + width <= size, writable for stores; else the
  bounds trap. Before every Load and Store, both BytesEqual operands, and
  pointer arguments of platform services (write_all resolves `count` bytes).
- `ArenaAllocate raw, size`: bump the frame arena; trap past its limit.
- `PointerCreate token, raw, size, flags, chain`: pop the free list (or take
  next_unused; trap at capacity), fill the entry, link it into the frame
  chain unless it is heap storage.
- `PointerRetire token`: bump the version (permanently retired at 4096),
  clear flags, push the free list.
- Prologue of a function with `allocate`: save the arena top and a zero chain
  head in two frame words. `Return`: retire the chain, restore the arena top.

Lowering: `allocate` = ArenaAllocate + Zero/initial Store on the raw address
+ PointerCreate. `GlobalAddress` = the constant token of global slot `1 + g`.
`Resize p, old, new` = PointerResolve p (offset 0, heap flag) unless null,
the raw resize runtime, PointerRetire p, PointerCreate unless new == 0.
`FieldAddress`/`ElementAddress` stay offset arithmetic on the token (their
existing static bounds checks remain; Resolve checks the rest).

C bridge (`zir_host_exec.c`, bench lane, needs sequencing): a resolver and a
creator callback for `ExecHostsCallNative`, so views are copied through
resolved addresses and returned views get read-only slots; raw `*T` handles
cross as token bits.

## Stages (each keeps `zx_lowered_parity`, `machine_elf`, `x86_encoder`,
## `aarch64_encoder`, `regalloc` and `native_load` passing)

1. Token layout constants and the slot table in `MachineImage` (data offset,
   capacity); `Resolve` and token derivation in machine IR, verifier and
   allocator; x86 encoding with exact-byte tests.
2. In-memory runtime: slot allocate/retire, Resize returning tokens, trap
   unwinding that retires active frames; native platform services and the
   host bridge resolve tokens. Pointer fixtures: dangling local, use after
   free, double free, bounds, read-only store, all trapping natively.
3. ELF runtime with the same entry points; `zx2native` uses the shared
   preflight; parity cases stay native.
4. Remove `compiler_native_safety.zi` checks that tokens now cover; keep
   named gaps (with tests) for anything still unsupported.
5. Flip the defaults: `zi2zx run`/`ziran run` and the embedding API's `Auto`
   to Native on linux-x86_64; Kryon player tests; full `make check` and
   `make check-bootstrap`; then commit.

AArch64 follows the same opcodes after stage 5.

## Progress (2026-11-15, uncommitted)

- Stages 1–2 implemented for x86-64: `PointerResolve`, `PointerCreate`,
  `ArenaAllocate` in machine IR, lowering and the encoder; frame words and
  chain retirement at `Return`; checked `Resize` (old slot retired only after
  the runtime freed or moved the block; a request that does not fit gives null
  and keeps the old block, as the engine does); globals as constant tokens;
  platform-service pointers resolved; the ELF writer lays out the header,
  table and arena (slot 0 and the global slots in the file, the rest zero
  fill) and re-measures the encoder to cross-check relocations; the in-memory
  loader maps the table and arena; the host bridge resolves view tokens and
  gives returned views read-only slots.
- The memory preflight no longer gates native compilation.
- Known difference from the engine: the engine also checks a pointer's
  element type on access; native checks liveness, version, writability and
  bounds. Recorded for a later stage.
- Remaining: full parity/suite runs on the checked route, delete
  `compiler_native_safety.zi` (stage 4), then the default flip (stage 5).

