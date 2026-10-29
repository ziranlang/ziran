# Changelog

## [Unreleased]
### Added
- Linux and Android hosts on x86-64, ARM and ARM64 run portable bundles as native code, compiled in memory and cached between launches.
- Natively run bundles execute `#parallel for` regions on worker threads, one per processor.
- `#parallel for` bodies can hand out results by writing their own slot of an array or slice the function owns, such as `out[it] = value`, or their own window of a slice, such as the row `pixels[it * width : it * width + width]`.
- The new `zi2wasm` tool lowers a checked program's scalar subset to one wasm32 module: the portable control flow, calls, casts and wrapped integer widths, with a step-budget import matching the native tier's counting. Programs outside the subset fail with a `zx.wasm` diagnostic, and `tests/zx_wasm.sh` compares the module's result with the native tier and the portable VM under node (and wasmtime when installed).
- The wasm execution tier and `zi2wasm` lower strings, slices and module globals: string values are i64 handles over interned data-segment literals with byte indexing, `.count` and equality through a generated compare helper; slices borrow their base's storage as (length, address) handles with bounds-checked element views; globals take fixed data-segment slots whose literal bytes reload fresh on every instantiation. Pointers cross as opaque handles. Probing Inner Breeze's real bundles now passes strings, slices and pointer signatures, leaving enums and expression-initialized globals as the remaining rejections.
- The wasm execution tier and `zi2wasm` lower records and fixed arrays: fields laid out exactly as the checker sizes them, values as arena slots with lexical region lifetimes, literals, member and index places with bounds checks, value copies through calls and returns, and parameter slots the callee owns.
- `BundleInstantiate` gained a WebAssembly execution tier behind a provider seam (`ZxWasmSetPlatform`): with a provider installed, bundles in the scalar subset run as WebAssembly lowered from their verified IR at open time, and everything else falls back to the portable VM; without a provider, behavior is unchanged. Native TinyCC hosts are untouched. `tests/zx_wasm_engine_test.c` checks the seam, handoff and fallback natively, and `tests/zx_wasm_web.sh` runs a bundle end to end through the wasm-built library and a JavaScript provider (skipped without emcc and node).

### Changed
- Native builds run `#parallel for` regions on one thread per processor instead of four; `ZIRAN_PAR_THREADS` still chooses.
- Saved IR (format 59) writes each string once and refers back to it, stores numbers in as few bytes as they need, and stores source positions relative to the previous one, so `.zir` files and `.zx` bundles carry about a third of their former code size at the same open cost. Rebuild format 58 IR and bundles from source.

### Fixed
- Bundles accept arrays of fixed arrays as function and host capability parameters.
- `#parallel for` regions reject mutating, foreign and procedure-value calls nested inside larger expressions.
- Native parallel workers see every binding their body reads, including parameters and fixed arrays.
- `#parallel for` bodies can write into arrays they declare themselves.
- Saved IR keeps the result type of a call whose argument is another call with widened arguments, such as `!Open(board, Index(x, y))`, so builds from it no longer fail with "saved IR does not match the checked program".
- `#parallel for` regions reject handing a shared slice to a callee that writes through it, directly or by passing it on.
- Natively run bundles reach a bound Ziran provider exported under another symbol.
- Go output converts a conditional's arms to its type, such as a float32 arm of a float64 conditional.

## [0.3.1] - 2026-10-23
### Fixed
- Source generation now preserves output errors on hosts that use file-backed text streams.
- Website release links now follow the installed compiler version.

## [0.3.0] - 2026-10-23
### Changed
- Portable executables and libraries now use `.zx`, the `zi2zx` tool, and the `zx` target. Source stays `.zi` and checked intermediate modules stay `.zir`. Rebuild previous `.zib` programs; renaming a file does not convert it.

### Fixed
- Go output no longer needs temporary filesystem space and reports output write failures instead of producing incomplete source files.

## [0.2.0] - 2026-09-28
### Added
- `ziran version` (and `--version`) prints the compiler version.
- Release tags now follow the compiler, so `ref = "v0.2.0"` in `ziran.toml` pins a release.

### Changed
- A clean build of the compiler is much faster and uses at most eight cores.

### Fixed
- Building `ziran` from scratch on older `make` versions no longer fails to link.

Earlier versions were tagged `v0.1.x` without a changelog.
