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
- There is no ARMv7 encoder.

## Rows, in order

Each row is committed and pushed once its focused tests and the full suite
pass, from a private index on origin/master.

4. **ARMv7 encoder** (`cmd/compiler_armv7.zi`, AAPCS32, hard-float, A32
   only). It covers 32-bit host pointers and 64-bit integer pairs, with
   encoder cases like AArch64.
5. **ARMv7 loader and the Moto row.** ARMv7 veneers, an
   `armv7a-linux-androideabi28` build, and parity bundles run on the Moto
   (the parity runner, given an armv7a build and Omega's adb). Then the Kryon
   APK runs on the Moto with native as the default.
6. **Standalone ELFs for AArch64 and ARMv7** (`zx2native --target`). This
   needs a per-architecture runtime with a maintained source instead of a
   byte blob. It is the last row.

No row adds an engine fallback. Anything native code cannot run on these
targets fails preflight with a named gap, as on x86-64.
