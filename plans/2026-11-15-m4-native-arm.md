# M4: native ZX on ARM devices (2026-11-15)

Owner: "do teh remaningi tasks", "so implemnet it prorely", "phone is adb
conneciton dude you can use it", "we alse have a pixle device on thinpkdpa
delta which you can use as well".

This continues `plans/2026-11-14-native-zx-players.md` sessions 6 to 8 with
what the tree actually has.

## Devices

- Moto e6 play `ZE2223BQZT` on Omega's adb: armeabi-v7a, armv7l kernel,
  Android 9 (API 28). Static NDK binaries run from `/data/local/tmp`.
- Pixel 4a `09231JEC206210` on Delta (`ssh wao@thinkpad.local`, then adb):
  arm64-v8a and armeabi-v7a, aarch64, Android 13 (API 33).
- NDK 28.2 on Omega: `armv7a-linux-androideabi28-clang` and
  `aarch64-linux-android33-clang`. qemu-user is not installed; the devices
  replace it.

## Where the tree is

- AArch64 runs natively in process on linux-aarch64 and Android, by default.
  `make android-parity ANDROID_NDK=… ANDROID_SERIAL=09231JEC206210
  ANDROID_ADB_HOST=wao@thinkpad.local` cross-builds `zi2zx` and reruns the
  Pixel parity (`tests/zx_android_parity_cases.zi`); the recorded receipt is
  `bench/results/zx-android-parity-pixel-2026-11-15.json`.
- The loader's machine-code half is per architecture:
  `cmd/zir/zir_host_native_arch.h` and `zir_host_native_<arch>.c`. ARMv7 adds
  a third file there.
- `cmd/compiler_machine_elf.zi` writes x86-64 ELF only. Its runtime (writes,
  number and float printing, the resize allocator) is an x86 byte blob with
  no source.
- `cmd/compiler_armv7.zi` encodes allocated machine IR as A32 for
  armv7a Linux and Android (design below), with `armv7_encoder` cases; all
  72 parity programs encode. Nothing loads or runs it yet.

## Rows, in order

Each row is committed and pushed once its focused tests and the full suite
pass, from a private index on origin/master.

5. **ARMv7 loader and the Moto row.** `cmd/zir/zir_host_native_armv7.c`
   with the host, trap, resize and helper veneers of the contract below,
   MOVW/MOVT (imm4:imm12) and BL (imm24, addend -8) patching, two general
   and six float argument slots in `NativeAbiFrame`, an
   `armv7a-linux-androideabi28` build, and the parity runner on the Moto
   through Omega's adb. Then the Kryon APK runs on the Moto with native as
   the default.
6. **Standalone ELFs for AArch64 and ARMv7** (`zx2native --target`). This
   needs a per-architecture runtime with a maintained source instead of a
   byte blob. It is the last row.

No row adds an engine fallback. Anything native code cannot run on these
targets fails preflight with a named gap, as on x86-64.

## ARMv7 contract (encoder done, loader is row 5)

LIR is target-neutral: pointers and function values are 8-byte slots, and
`s64` is the common integer, so most general values are 64 bits. The
machine pipeline keeps one physical register per virtual. ARMv7 therefore
describes its general bank in register pairs, so allocation, verification
and lowering stay as they are:

- Pair n is (r2n, r2n+1) for n = 0..4; pair 5 is (r12, lr). r10 is the
  encoder's own temporary, r11 the frame pointer. `general_registers` = 6,
  all allocatable. Calls clobber pairs 0, 1 and 5; pairs 2-4 are
  callee-saved (AAPCS r4-r9). Pair 5 is the bank scratch (highest
  caller-saved), so r12 and lr are free temporaries inside any instruction;
  lr is saved by every prologue.
- A value of 32 bits or less lives in the even register; a 64-bit value
  (integers, LIR pointers and function values) in both, low word in the even
  one. Raw machine addresses (`pointer_bits` = 32) are 32-bit.
- Floats are d0-d15 (VFPv3-D16, the armeabi-v7a floor); f32 lives in
  s(2n). d0-d7 are caller-saved, d7 the float scratch, d8-d15 callee-saved.
- Our own convention between generated functions: general arguments in
  pairs 0 and 1, floats in d0-d5, the rest on the stack in 8-byte slots;
  results in pair 0 / d0; the record-result address in r10, stored by the
  callee's prologue. Indirect calls go through pair 4. Generated code is A32
  and enters C only through the loader's veneers (BLX, so Thumb callees
  work), so softfp versus hard-float never crosses an ABI boundary.
- 64-bit arithmetic is pairs: ADDS/ADC, SUBS/SBC, UMULL+MLA, compares as
  SUBS/SBCS (signed) or CMP/CMPEQ (unsigned and equality), shifts as
  conditional sequences. 64-bit divide and remainder and int64<->float
  conversions have no instruction; they call loader runtime helpers through
  a `RuntimeHelper` relocation (symbol = helper id, a BL). The encoder saves
  pairs 0 and 1 around the call and the loader's veneer keeps d0-d7, so a
  helper changes only its destination, as the instruction would.
- Addresses materialize as MOVW/MOVT halves (two relocations of width 2,
  addend 0 and 2); direct calls and veneers are BL imm24 (width 4).
- Fixed forms name pairs: BytesEqual inputs pairs 0-3, clobbers pairs 1-3;
  Resize inputs pairs 0-2, clobbers pairs 1-2. Their clobber lists end at
  the first -1, since six pairs cannot fill x86's eight. Resize calls the
  runtime with the old block's address, both counts and the element size
  (four 8-byte words) at sp and reads the new block's address (or null)
  back from sp; like the helper veneer, the resize veneer keeps every
  register but lr.
- The trap veneer receives the code in r0 and the return address in lr;
  host veneers save pairs 0-1 (r0-r3) and d0-d5 into `NativeAbiFrame` and
  return a general result in r0:r1, a float in d0.
