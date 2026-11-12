# Target-neutral machine IR contract

Status: implemented as the verified pre-allocation boundary for the current
lowered-IR subset. This is not a register allocator, instruction encoder, native
ZX backend, or completion of [step 06](06-optimization-and-native-backend.md).

## Public boundary and ownership

The target surface is defined by `compiler_target.zi`:

- `TargetDescribe` describes supported architecture/OS combinations.
- `TargetValid` checks that a description is canonical rather than accepting a
  caller-modified register or ABI model.
- `TargetMachineLoweringSupported` separately states whether the current machine
  lowerer can execute that description. At present this is Linux x86-64 SysV only.

The owned machine-IR surface is defined by `compiler_machine_ir.zi`,
`compiler_machine_lower.zi`, and `compiler_machine_verify.zi`:

- `MachineDefaultLimits` and `MachineDefaultOptions` provide bounded defaults.
- `MachineLower(owner, lir, target, options, limits)` verifies the input LIR,
  copies its required metadata, lowers it, verifies the resulting machine IR,
  and publishes the owner only after success.
- `MachineProgramView(owner)` returns a borrowed flat view while the owner is
  valid. `MachineFree(owner)` is repeatable and invalidates all prior views.
- `MachineVerify(program, limits)` validates the target, table bounds, metadata,
  register classes, operand constraints, flags lifetimes, calls, stack objects,
  parallel copies, and relocations without repairing malformed input.

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
constraints plus general, float, and vector clobber masks. Relocations identify
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

## Described targets versus executable targets

Canonical descriptions exist for maintained x86-64, ARMv7, AArch64, and wasm32
OS combinations so later allocation and backend work shares one target model.
A valid description is not an implementation claim. ARMv7, AArch64, wasm32,
Windows, macOS, Android, and WASI currently fail machine lowering with
`UnsupportedTarget`.

## Next allocation contract

The register allocator consumes a verified `MachineProgram` and must preserve
table identity, operand roles, fixed and tied constraints, flags adjacency,
call-clobber interference, float/vector physical overlap, simultaneous edge-copy
semantics, and source origins. It must add deterministic live ranges, physical
assignments, spills/reloads, frame layout, and resolved parallel copies, then
run a post-allocation verifier that rejects unresolved virtual locations,
overlapping assignments, broken ties, clobbered live values, or invalid stack
alignment. Allocation must not mutate or reinterpret the packed/external ABI
boundary.

After allocation, still-required independent work includes target encoders,
prologues/epilogues and callee-save handling, external ABI classification,
relocatable objects/linking, unwind/debug information, native ZX integration,
the remaining target matrix, and physical-Alpha performance validation.

## Focused local test

```sh
env -u DISPLAY -u WAYLAND_DISPLAY HARMONY_DESKTOP_RECOVERY=0 \
  build-queue -- sh tests/machine_ir.sh build/bin/ziran
```

The fixture checks target descriptions, real LIR lowering, ABI constraints,
owned metadata, deterministic output, dynamic-allocation representation,
explicit unsupported targets/calling forms, bounded limits, malformed MIR
rejection, and repeatable cleanup.
