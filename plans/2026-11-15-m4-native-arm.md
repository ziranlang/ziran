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
