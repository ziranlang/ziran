# Assembly target: open work (updated 2026-11-15)

M0–M3 are done (`ASSEMBLY/milestones.md`, "Current position"). M3 closed
with interim comparison 2 in `bench/zx_compare.md`. What remains, in
milestone order:

1. **M4: ARM rows.** AArch64 native loading, ARMv7/AAPCS32 with 32-bit host
   images, native ZX on Android, and proof of the USB Moto row. Steps and
   device notes are in `plans/2026-11-15-m4-native-arm.md`. Direct wasm
   lowering from verified ZX already exists.
2. **M5: objects, linker and foreign libraries.** Relocatable ELF, COFF and
   Mach-O, archive reading, the external C ABI and foreign libraries,
   Windows/macOS ABIs and signing. Not started.
3. **M6: inspection and final cutover.** Unwind, debug and profiler mapping,
   then the owner's full before/after ZX comparison on Alpha's physical host.
4. **Native code quality.** Warm native ZX is still 2.8× (scalar) and 1.5×
   (structured) slower than the M0 TinyCC receipt, and slower than the engine
   on the Sieve, Vectors and Printing kernels
   (`bench/results/wasm-tier-alpha-2026-11-15-records.json`). See
   `plans/2026-11-15-native-speed.md`.
5. **Self-hosting S5–S18.** `cmd/zir` still holds 62 C files for the checker,
   parser, emitters, host API and Go backend (`plans/2026-11-14-self-hosting.md`).
   S18 is bootstrap convergence.
6. **Legacy leftovers.** R1/R3 (VM-era public C names, then docs and site)
   and N5 (duplicated relocation code), in `plans/2026-11-14-legacy-removal.md`.
7. **Deferred metaprogramming** (after the self-hosted checker, S5): notes
   with `#modify`, `type_info`, the compiler message loop,
   build-as-Ziran-program, `#bake_arguments` and SOA layout. `#placeholder` is
   dropped. Unchecked is the default; checks are opt-in with `#checks`
   (`plans/2026-11-15-manual-safety.md`), so `#no_abc` is not needed.
8. **Known language gaps** that stay explicit Gap diagnostics: ownership of
   resizable collections, reentrant callbacks, and general native
   string/arbitrary-host instructions.
9. **Environment.** `zstd_go` needs Go 1.25; Omega's default is 1.24.4.
