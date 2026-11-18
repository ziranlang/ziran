# Targets benchmark cohort and follow-up

The standard diagnostic cohort contains 29 workloads. Each uses the same inputs
and algorithm in the independent C and Go references and in the Ziran program.
Run `bench/bench.zi -- wasm-tier --standard --standard-verify` through
`build-queue` to check the small and diagnostic sizes without timing them. The
published diagnostic run uses `--standard --host-gate --samples 11`, with
learning disabled. Keep the raw receipts until the complete cohort has been
copied from the measurement host.

## Wasm widget Text

On the measured compiler revision, Text takes 4.153 µs per two-frame scenario
in Wasm and 3.837 µs in Go. Button and Input already beat Go. The Wasm runner
warms its existing instance and runs without a step budget. Each measured batch
checks the paint result. The next comparison should preserve that checksum,
the same input, and the C and Go timings from the same cohort.

Profile the warm Text call and inspect lowering of the small paint, rectangle,
string and decoding records that pass between its hot procedures. Keep eligible
values in Wasm locals or return multiple scalar values if the profile identifies
linear-memory traffic as significant. Retest the portable calling convention,
including any exported calls and imported bindings.

## Fixed-array value copies

In the standard suite, the native k-nucleotide profile located 999 of 1,062
samples in the generated 128 KiB copy loop. The diagnostic medians are
1,971 µs for C, 71,099 µs for ZX native and 46,958 µs for Wasm. The source uses a
16,384-element histogram and iterates it by value. Trace the loop's aggregate
loads in both lowerings before changing the benchmark source. Preserve value
semantics for arrays that may escape or be modified through another alias.
