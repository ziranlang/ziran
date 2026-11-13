# Target-neutral machine IR contract

Status: implemented for Linux x86-64 SysV as the verified pre-allocation
boundary for the current lowered-IR subset, deterministic
[register allocation](#register-allocation), a separate allocated-program
verifier, and a [direct x86-64 encoder](#machine-code-encoder) that turns a
verified allocated program into an in-memory code image with relocations.
The encoder is not yet connected to `zi2asm`, an ELF writer or a native ZX
loader, and none of this completes [step 06](06-optimization-and-native-backend.md)
or milestone M2. The implemented native call ABI is general-register only;
float and vector call arguments and results and stack-passed arguments are
[unsupported forms](#supported-call-forms) that fail with a stable diagnostic.

## Public boundary and ownership

The target surface is defined by `compiler_target.zi`:

- `TargetDescribe` describes supported architecture/OS combinations.
- `TargetValid` checks that a description is canonical rather than accepting a
  caller-modified register or ABI model.
- `TargetMachineLoweringSupported` separately states whether the current machine
  lowerer can execute that description. At present this is Linux x86-64 SysV only.

The owned machine-IR surface is defined by `compiler_machine_ir.zi`,
`compiler_machine_lower.zi`, `compiler_machine_verify.zi`, `compiler_abi.zi`,
`compiler_regalloc.zi`, `compiler_machine_verify_allocated.zi`, and
`compiler_x86_64.zi` (which reuses the byte encoders in `compiler_x86_bytes.zi`):

- `MachineDefaultLimits` and `MachineDefaultOptions` provide bounded defaults.
- `MachineLower(owner, lir, target, options, limits)` verifies the input LIR,
  copies its required metadata, lowers it, verifies the resulting machine IR,
  and publishes the owner only after success.
- `MachineProgramView(owner)` returns a borrowed flat view while the owner is
  valid. `MachineFree(owner)` is repeatable and invalidates all prior views.
- `MachineVerify(program, limits)` validates the target, table bounds, metadata,
  register classes, operand constraints, flags lifetimes, calls, stack objects,
  parallel copies, and relocations without repairing malformed input.
- `compiler_abi.zi` is the only module that names target registers. The
  lowerer, allocator and verifiers ask it for argument and result registers,
  call clobbers, the fixed registers of division and shifts
  (`MachineAbiFixedRegister`), which call classes the native ABI implements
  (`MachineAbiCallRegisterClass`) and which relocation addends a target can
  hold (`MachineAbiRelocationAddend`). Its register enum is file-private.
- `RegAllocAllocate(owner, target, limits)` replaces a verified `Virtual`
  program with a verified `Allocated` one ([register allocation](#register-allocation)).
- `MachineVerifyAllocated(program, limits)` checks an allocated program
  independently of the allocator.
- `MachineX64Validate`, `MachineX64Measure` and `MachineX64Encode` verify and
  encode an allocated program ([machine-code encoder](#machine-code-encoder)).

Schema version 2 adds the allocation stage. `MachineProgram.stage` is
`Virtual` after lowering and `Allocated` after `RegAllocAllocate`; every operand
carries the SSA `value` it reads or writes (or none), stack objects carry a
frame `offset` and the `value` they hold, and allocated programs add one
`MachineFrame` per function, explicit `Spill`/`Reload` opcodes with a
`MachineSpill` record each, and the per-bank scratch registers.

`MachineOwner` owns every published table. Source names and function names are
copied into its byte table and referenced by offsets; source origins, type shape,
aggregate members, function signatures, globals, CFG records, operands, calls,
copies, stack objects, and relocations do not borrow LIR storage. Lowering the
same valid input and target produces the same table order and contents.

## Representation

`MachineProgram` is a deterministic collection of flat, index-addressed tables.
Functions own contiguous block, virtual-register, and stack-object ranges.
Blocks own contiguous instruction ranges. Instructions own contiguous operand
and edge ranges. Edges reference contiguous simultaneous-copy ranges. Aggregate
member and function-parameter type IDs share the owned `type_ids` table.

Operands distinguish virtual and physical registers, immediates, blocks,
functions, stack objects, and globals. Each operand records its use/define role,
register class, optional fixed register, and optional tie to another operand.
Condition results use an explicit flags virtual: `Compare` defines it and the
adjacent `SetCondition` consumes it. Calls carry fixed argument/result
constraints plus general, float, and vector clobber masks. A `Fuel` check is
explicit before liveness: its first operand is the amount, its second names the
instance fuel counter through a `FuelCounter` relocation, and it defines (so
clobbers) a fresh flags virtual. Relocations identify
the exact instruction operand and symbol rather than hiding symbol references in
an encoder-specific payload.

The current lowerer covers the verified LIR operations represented by the
machine opcode enum: scalar integer and float operations, comparisons and
conversions, aggregate pseudos, checked-memory/address pseudos, direct and
indirect calls, CFG transfers, returns, fuel, traps, and unreachable control.
These are pre-allocation pseudos; their presence does not claim that a complete
native encoder can execute every lowered program.

`Allocate` remains an explicit effectful machine pseudo. Every dynamic execution
must produce fresh storage whose lifetime lasts until that allocating frame
exits, including repeated visits to one instruction in a loop or recursive
activation. It therefore is not represented as `StackAddress` plus one static
stack object per instruction site. Allocation and encoding must preserve the
pseudo until they implement that exact dynamic frame-storage behavior.

## Linux x86-64 ABI facts

Register numbers follow x86-64 encoding order. General-register argument
constraints are RDI, RSI, RDX, RCX, R8, and R9; float arguments use XMM0 through
XMM7 independently. General results use RAX and float results use XMM0. Calls
record the SysV caller-clobbered general registers and all sixteen XMM registers.
Division fixes the dividend and result to RAX and clobbers RDX; shift counts
use RCX. The lowerer obtains all of these from `compiler_abi.zi` queries for the
described target rather than naming x86-64 registers itself.

The general allocatable mask is `0xffcf`; bits 4 and 5 are excluded by the
`0x30` reserved mask, so RSP and RBP can never be assigned as ordinary virtual
register locations. Float and vector registers are separate logical classes but
share the same physical XMM bank (`float_vector_overlap = true`). Allocation
must therefore treat a live float and vector assignment to the same physical
number as interference.

Aggregate sizes use Ziran's packed internal layout. Aggregate arguments/results
are represented through stack objects and are deliberately distinct from the
external System V aggregate classification rules. C, System, and Host calling
forms, including external aggregate ABI, varargs, and callbacks, are rejected
explicitly; they are not silently treated as internal calls.

### Supported call forms

Machine IR describes the whole internal call shape: float arguments carry
their XMM constraints, a seventh integer or any aggregate argument is counted
in `stack_argument_bytes`, and allocation lays out the outgoing area. The
implemented native call ABI is narrower. A call may pass at most six
arguments, each a general-class value in its ABI register in order, and may
return nothing or a general-class value in RAX. `MachineVerifyAllocated`
rejects every other call with `Unsupported` at the first offending argument or
result operand, or at the call when it reserves stack argument bytes: float
and vector arguments and results, aggregate arguments, arguments beyond the
sixth, and any outgoing stack bytes. The check runs before an encoder starts,
so no unsupported call reaches encoding or another engine. Incoming stack
parameters of a function that is not called natively remain representable
and are verified against the parameter order. The full SysV argument/result
classification, including float and stack passing, is later work; it is not
partially implemented.

## Described targets versus executable targets

Canonical descriptions exist for maintained x86-64, ARMv7, AArch64, and wasm32
OS combinations so later allocation and backend work shares one target model.
A valid description is not an implementation claim. ARMv7, AArch64, wasm32,
Windows, macOS, Android, and WASI currently fail machine lowering with
`UnsupportedTarget`.

## Register allocation

`RegAllocAllocate(owner, target, limits)` in `compiler_regalloc.zi` takes the
owner's verified `Virtual` program and, on success, replaces it with a verified
`Allocated` program. The allocation is built in a separate owner and verified
with `MachineVerifyAllocated` there; only then are the two owners' tables
exchanged with `VecSwap`, which moves storage without allocating, so no step
after the owner starts changing can fail. On any failure, including one the
allocated verifier finds in the built program, the owner keeps its original
program and the `MachineDiagnostic` names the input function, block,
instruction, operand and virtual: an allocated instruction maps back to the
input instruction it rewrote, with operands at the same positions, and a
transfer the allocator inserted reports only its function and virtual. Each
function is processed in a fixed order:

1. `MachineVerify` and `MachineVerifyFuel` run first. Every control-flow cycle
   must pass a fuel check, and every function a direct call or a taken address
   can reach must charge fuel in its entry block before any call. A missing or
   malformed check fails with `Fuel`.
2. Constraints become explicit transfers. A fixed input is moved into its
   register just before the instruction, a fixed result out of it just after,
   a register parameter out of its incoming register at entry, and a tied input
   without a fixed register is copied into a short interval that the result
   must reuse. Division keeps the dividend and quotient in RAX and RDX clobbered
   across the instruction, shift counts use RCX, and call arguments and results
   use their ABI registers.
3. Block liveness is solved over the item order, then each SSA virtual gets
   one hull interval with its use positions. Block parameters are defined at
   the block start; edge-copy sources are used at the predecessor terminator.
4. Linear scan runs per register bank (general; float and vector share the XMM
   bank on x86-64) inside the target's allocatable masks minus one scratch
   register per bank (R11 and XMM15). Fixed operands, early clobbers and call
   clobbers block registers. An interval is split where its register becomes
   blocked; under pressure the value with the farthest next required use goes
   to memory until that use. Values live across a call take callee-saved
   registers (RBX, R12–R15) or are spilled with reason `Call`.
5. Split boundaries, fixed transfers, block-parameter copies and edge location
   fixes become `Move`, `Spill` and `Reload` instructions. Each edge's copies
   are one parallel copy, sequentialized with cycles broken through the scratch
   register or a frame `Temporary`; a branch edge that needs moves gets its own
   block (`lir_block` is -1). Allocator transfers preserve flags, so they may
   sit between a compare and its consumer. A transfer with equal source and
   destination register renames a value and encodes no bytes.
6. The frame pointer is pushed on entry. Callee-saved registers the function
   writes get `CalleeSave` homes; values, spills and temporaries sit at aligned
   negative frame-pointer offsets; outgoing stack arguments occupy the 16-byte
   aligned bottom of the frame; incoming stack arguments start 16 bytes above
   the frame pointer in parameter order. The frame size is a multiple of 16.

Every `Spill` and `Reload` has a `MachineSpill` record with its value, slot,
register, reason (`Pressure`, `Call`, `Fixed`, `Parameter` or `Cycle`) and
nearest source origin. Flags-class operands or copies, a virtual read-modify-write
operand, conflicting fixed registers, broken ties, call clobber masks that keep
caller-saved registers or clobber reserved ones, non-internal calling forms,
unsupported targets and exceeded limits fail explicitly. Nothing is routed to
another engine.

`MachineVerifyAllocated(program, limits)` in `compiler_machine_verify_allocated.zi`
is independent of the allocator. It rejects virtual operands, unresolved edge
copies, registers outside the bank masks (scratch registers only in transfers),
fixed-register mismatches, broken ties, malformed transfers, bad relocations,
relocation addends the target cannot hold (a direct call, function address or
fuel counter carries none; a global address carries a signed 32-bit addend),
[unsupported call forms](#supported-call-forms), spill records that disagree
with their instruction, misplaced fuel checks and frame layouts that are
misaligned, overlapping, missing callee-saved homes or whose incoming stack
arguments disagree with the parameter order. A forward dataflow over
registers, flags and stack objects then proves that every use finds its
value: a wrong value is `Interference`, a value lost to a call or clobber is
`Clobber`, a missing one is `Undefined`, and a parallel copy that reads a
source it already overwrote is `Copy`.

## Machine-code encoder

`compiler_x86_64.zi` encodes a program that passes `MachineVerifyAllocated`
for Linux x86-64 SysV into an in-memory image: code bytes, function, block and
instruction offsets, and PC-relative 32-bit relocations for global addresses,
the fuel counter and the trap handler. Direct calls and function addresses
inside the image are resolved by the encoder. A measuring pass and an exactly
sized writing pass must agree on every offset. `MachineX64Encode` publishes
the image only after verification, form checks and measuring succeed; any
diagnostic leaves the output owner unchanged. Every instruction form the
encoder does not implement is a hard diagnostic, never another engine.

Still required: wiring the machine-IR path into `zi2asm` and an executable
writer, the full SysV call ABI, external ABI classification, relocatable
objects/linking, unwind/debug information, native ZX loading, the remaining
target matrix, removal of the direct LIR encoder at the M2 exit, and
physical-Alpha performance validation.

## Direct native code versus generated-code exporters

Direct native code means that Ziran's own encoders write machine code from
verified IR, with no generated C, external assembler, C compiler or system
linker. Two such paths exist: the direct LIR encoder behind `zi2asm`, which
reads verified ZX instructions and writes a Linux x86-64 ELF executable, and
this machine-IR path, which produces a verified relocatable image and feeds
the internal ELF writer through `zi2asm --machine --emit elf`. Source and saved
`.zir` first go through optimized ZX serialization and verification on both
paths. In-memory native ZX loading and runtime cutover remain milestone M3.

The C target (generated C compiled by a C toolchain) and the Go target
(generated Go built by the Go toolchain) also produce runnable binaries, but
they are separate exporters with their own toolchains and test rows. They are
never a fallback for direct native code: a form the native path does not
implement fails with its diagnostic and is not rerouted to C, Go, the portable
engine or any other backend, as the [gap policy](gap-policy.md) requires. The
focused tests below run their fixtures with `--target=c` only as the test
harness; what they check is the direct native pipeline.

## Focused local test

```sh
env -u DISPLAY -u WAYLAND_DISPLAY HARMONY_DESKTOP_RECOVERY=0 \
  build-queue -- sh tests/machine_ir.sh build/bin/ziran
env -u DISPLAY -u WAYLAND_DISPLAY HARMONY_DESKTOP_RECOVERY=0 \
  build-queue -- sh tests/regalloc.sh build/bin/ziran
```

The machine-IR fixture checks target descriptions, real LIR lowering, ABI
constraints, owned metadata, deterministic output, dynamic-allocation
representation, explicit unsupported targets/calling forms, bounded limits,
malformed MIR rejection and repeatable cleanup, then runs the encoder fixture,
which checks exact bytes, frames, relocations, fuel, traps, hard pre-encoding
call/addend rejection and output preservation. The allocation fixture lowers
real LIR and checks register pressure, call splitting, spills and reloads,
division and shift registers, flags, register, memory and aggregate copy
cycles, float and vector banks, frame and incoming stack-argument layout
(including out-of-order incoming homes), fuel placement, deterministic
output, explicit unsupported forms, and rejection of malformed allocated
programs at the faulting location. Its late-failure fixtures allocate
successfully and are then rejected by the allocated verifier (a float call
argument, a float call result, an aggregate stack call argument and a direct
call relocation with an addend); each checks the input-location diagnostic and
that the owner still holds its unchanged `Virtual` program, and the addend
fixture allocates the same owner once the input is valid. Float call arguments
and results, stack call arguments and global-address addends outside 32 bits
fail in `MachineVerifyAllocated` before any encoder pass runs.
