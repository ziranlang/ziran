# M4: native ZX on ARM devices (2026-11-15)

Owner: "do teh remaningi tasks", "so implemnet it prorely", "phone is adb
conneciton dude you can use it", "we alse have a pixle device on thinpkdpa
delta which you can use as well".

This continues the native ZX player sessions 6 to 8 with
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

- AArch64 and ARMv7 run natively in process on Linux and Android, by default.
  `make android-parity ANDROID_NDK=… ANDROID_ARCH=aarch64|armv7
  ANDROID_API=… ANDROID_SERIAL=… [ANDROID_ADB_HOST=…]` cross-builds `zi2zx`
  and reruns the 72-program device parity
  (`tests/zx_android_parity_cases.zi`). Receipts:
  `bench/results/zx-android-parity-pixel-2026-11-15.json` (Pixel, aarch64,
  through `wao@thinkpad.local`) and
  `bench/results/zx-android-parity-moto-2026-11-16.json` (Moto, armv7, API
  28, Omega's adb). Kryon's ZX player APK (`tests/zx_android_test.py
  --serial ZE2223BQZT` in kryonlabs/kryon) runs `app.zx` natively on the
  Moto; its remaining test failures are Kryon's (a notification receiver
  without its JNI method, and an IME probe Android 9 cannot answer).
- The loader's machine-code half is per architecture:
  `cmd/zir/zir_host_native_arch.h` and `zir_host_native_<arch>.c`. The ARMv7
  register-pair contract is in `ASSEMBLY/machine-ir.md`.
- `cmd/compiler_machine_elf.zi` writes x86-64 ELF only. Its runtime (writes,
  number and float printing, the resize allocator) is an x86 byte blob with
  no source.

## Rows, in order

Each row is committed and pushed once its focused tests and the full suite
pass, from a private index on origin/master.

5. **ARMv7 unchecked forms.** Done (2026-11-16, `ab5f39b9`): the encoder
   has AArch64's arena-only frames, raw Resize, immediate element form and
   nonzero member bound, so lowering keeps no checked pointer without
   `--checks`. The Moto parity rerun on default bundles is
   `bench/results/zx-android-parity-moto-2026-11-23.json` (81 programs).
6. **Linear image relocations on ARM.** Done (2026-11-23): the shared ARM
   image builder indexes the program's relocations with
   `MachineRelocationIndex` and looks up only the relocation's instruction,
   as x86-64 does, instead of scanning every machine relocation per image
   relocation. Device receipts:
   `bench/results/zx-android-parity-moto-2026-11-23.json` (armv7) and
   `bench/results/zx-android-parity-pixel-2026-11-23.json` (aarch64), both
   81/81.
7. **Standalone ELFs for AArch64 and ARMv7** (`zx2native --target`). Done
   (2026-11-23). Each row has its own runtime as Ziran source, not a byte
   blob: `compiler_aarch64_runtime.zi` and `compiler_armv7_runtime.zi` emit
   the exit and write-all routines in their own instructions, reaching the
   operating system through `svc 0`. `compiler_aarch64_elf.zi` writes a static
   ELF64 (EM_AARCH64) and `compiler_armv7_elf.zi` a static ELF32 (EM_ARM,
   EABI5); both place an entry stub that sets the stack limit, calls the
   initializers and the entry, and ends through the runtime, and both resolve
   every image relocation at link time, so the image needs no loader. Device
   receipts: `bench/results/zx-aarch64-elf-pixel-2026-11-23.json` (Pixel 4a)
   and `bench/results/zx-armv7-elf-moto-2026-11-23.json` (Moto e6 play). A
   program that calls a formatter, a host capability or the resize allocator
   fails before output with
   `assembly.gap.machine-platform-service-unavailable` naming the row.

No row adds an engine fallback. Anything native code cannot run on these
targets fails preflight with a named gap, as on x86-64.
