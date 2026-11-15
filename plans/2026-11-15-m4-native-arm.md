# M4: native ZX on ARM devices (2026-11-15)

Owner: "do teh remaningi tasks", "so implemnet it prorely", "phone is adb
conneciton dude you can use it", "we alse have a pixle device on thinpkdpa
delta which you can use as well".

This continues `plans/2026-11-14-native-zx-players.md` sessions 6 to 8 with
what the tree actually has after M3.

## Devices

- Moto e6 play `ZE2223BQZT` on Omega's adb: armeabi-v7a, armv7l kernel,
  Android 9 (API 28). Static NDK binaries run from `/data/local/tmp`.
- Pixel 4a `09231JEC206210` on Delta (`ssh wao@thinkpad.local`, then adb):
  arm64-v8a and armeabi-v7a, aarch64, Android 13 (API 33).
- NDK 28.2 on Omega: `armv7a-linux-androideabi28-clang` and
  `aarch64-linux-android33-clang`. qemu-user is not installed; the devices
  replace it.

## Where the tree is

- `cmd/compiler_aarch64.zi` encodes allocated machine IR. It has no
  checked-pointer forms (`PointerResolve`, `ArenaAllocate`, `PointerCreate`)
  and no stack-limit check. Its image publishes `stack_limit_offset = -1` and
  `pointer_offset = 0`, so the checked native loader rejects it. Calls pass
  the record-result address in x8 (6f69f017).
- `cmd/zir/zir_host_native.c` is x86-64 only. Its host, trap and resize
  veneers are x86 bytes, it patches relocations as rel32, its host frame is
  SysV, and trap sites come from x86 return addresses.
- `cmd/compiler_machine_elf.zi` writes x86-64 ELF only. Its runtime (writes,
  number and float printing, the resize allocator) is an x86 byte blob with
  no source.
- There is no ARMv7 encoder.

## Rows, in order

Each row is committed and pushed once its focused tests and the full suite
pass, from a private index on origin/master.

1. **AArch64 checked memory in the encoder.** Port `PointerResolve`,
   `ArenaAllocate` and `PointerCreate` from `cmd/compiler_x86_64.zi`
   (`:1729-1960`) to AArch64. Add the prologue stack-limit check and the
   `StackLimit` and `PointerRuntime` relocations. Publish `stack_limit_offset`
   and `pointer_offset` like x86. Test: `aarch64_encoder` cases for each
   form, and `machine_ir`.
2. **AArch64 in the in-process loader.** Split `zir_host_native.c` into a
   shared part and per-architecture veneers. AArch64 host, trap and resize
   veneers save the AAPCS64 argument banks into the same `NativeAbiFrame`
   contract. Relocations write the four MOVZ/MOVK halves. Code is published
   RX after `__builtin___clear_cache`. The CPU key comes from `getauxval`
   HWCAP/HWCAP2.
3. **Android build and device runner.** Cross-build `libziran.a` and
   `zi2zx` for `aarch64-linux-android33` with the NDK in a private
   BUILD_DIR. The compilers that run during the build stay host binaries.
   Push the binaries and the `zx_lowered_parity` bundles to the Pixel.
   Native and `--backend engine` output must match on the device, and each
   must match the Omega engine. The run is driven from a Ziran test with a
   recorded receipt.
4. **ARMv7 encoder** (`cmd/compiler_armv7.zi`, AAPCS32, hard-float, A32
   only). It covers 32-bit host pointers and 64-bit integer pairs, with
   encoder cases like AArch64.
5. **ARMv7 loader and the Moto row.** ARMv7 veneers, an
   `armv7a-linux-androideabi28` build, and parity bundles run on the Moto.
   Then the Kryon APK runs on the Moto with native as the default.
6. **Standalone ELFs for AArch64 and ARMv7** (`zx2native --target`). This
   needs a per-architecture runtime with a maintained source instead of a
   byte blob. It is the last row.

No row adds an engine fallback. Anything native code cannot run on these
targets fails preflight with a named gap, as on x86-64.

## Row 1 design (AArch64 checked memory)

Port these from `cmd/compiler_x86_64.zi` to `cmd/compiler_aarch64.zi`:
`PointerHeader`, `PointerEntry`, `PointerResolve`, `ArenaAllocate`,
`CreateSlot`/`FreePop`/`PointerCreate`, `RetireSlot`,
`PointerFrameEnter`/`PointerFrameLeave`/`PointerFrameStep`, the slot-aware
`Resize` (`CheckHeapSlot`, `ResizeSlots`, `RetireToken`, `HeapSlot`), the
prologue stack-limit check (`MachineX64StackChecked`), and the image data
layout. That layout is fuel, then the trap code at fuel+8, the stack limit at
fuel+16 and the pointer header at fuel+24 (`MachinePointerHeaderBytes`). It
publishes `stack_limit_offset` and `pointer_offset`.

Register plan: x16 holds the header address (the role r11 has on x86), x17
is a temporary (both reserved, never allocated), and x10 is the fixed
`PointerScratch` operand (`MachineAbiAddressScratchRegister`). x86 `push`
and `pop` become `str xN, [sp, #-16]!` (0xF81F0FE0|Rt) and
`ldr xN, [sp], #16` (0xF84107E0|Rt). Shifts and fields use UBFM
(0xD3400000 | immr<<16 | imms<<10 | Rn<<5 | Rd): `lsr #s` is immr=s,
imms=63; `lsl #s` is immr=64-s, imms=63-s; the version field is
`ubfx #32, #12`; the offset is `ubfx #0, #32`.

Trap codes reuse the x86 numbers so the loader's messages match: Stack
0xffff0006, Storage 0xffff0007, Pointer 0xfffeffff. Add `StackLimit` and
`PointerRuntime` to `MachineA64RelocationKind`, published with width 2, as
four MOVZ/MOVK halves through `MaterializeRelocated`.

Row 2 (loader): every width-2 relocation writes imm16 bits 5..20 of its
instruction, with the half taken from addend/2; width-4 relocations
(`bl` to the host, trap and resize veneers) write imm26.
