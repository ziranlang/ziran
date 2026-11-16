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

## State

Done: default output on native x86-64 and AArch64, Wasm, C and Ziran's own
Go checks carries no defensive check; `--checks` builds today's checked
program and the `.zx` header records it; `#checks` keeps checks at module,
block, statement and access level (`tests/checks_granularity_cases.zi`); the
guide documents manual safety (`docs/USAGE.md`, Checks). The portable engine
stays checked: it is the reference and test runner, and its pointer model is
the address table.

Kept everywhere: divide by zero and `INT_MIN / -1`, shift count, float
conversion range, `unreachable`, fuel, the stack limit and the frame-arena
limit. Whether shift and conversion become C-style undefined is an owner
question.

## Open work

- **ARMv7.** Native ARMv7 lowering keeps checked pointers: its encoder needs
  the unchecked forms AArch64 has (frames on `arena_mark`, raw `Resize`, the
  immediate element form, a nonzero member bound).
- **SIMD gathers and Go helpers.** `zi_simd_gather_check` in C
  (`include/ziran_simd.h`) and Go, and Go's `zi_simd_check` helpers, still
  check by default.
- **Go's own checks.** Ziran does not run `go build`; the Go toolchain
  removes its implicit bounds checks with `-gcflags=-B`.
- **Std readers.** Plain `std/binary.zi` `Read*LE` and
  `std/http_get_linux.zi` `GetHttps` still route through the validated
  forms; audit callers that rely on a short read giving 0 before making them
  plain.
- **Device parity.** AArch64 unchecked `Resize` and frames have host encoder
  coverage only; a Pixel parity run of default bundles confirms them.
- **Element types.** With `--checks`, native code does not check a pointer's
  element type on access (the engine does); the load-time verifier covers
  verified programs.
- **Optional.** The loader refuses an unchecked bundle the user marks
  untrusted.

## The `.zx` tension

A downloaded `.zx` built without checks can do anything a native program can:
it runs as machine code in the player's process. Owner's answer: performance
and full optimization first, most apps come from trusted channels, so bundles
are unchecked by default too. Optional last item: the loader refuses an
unchecked bundle the user has marked untrusted (it does not add checks; a
bundle is checked only if built with `--checks`).
