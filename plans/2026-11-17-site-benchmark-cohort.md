# Targets benchmark cohort and follow-up

The standard diagnostic cohort contains 29 workloads. Each uses the same inputs
and algorithm in the independent C and Go references and in the Ziran program.
Run `bench/bench.zi -- wasm-tier --standard --standard-verify` through
`build-queue` to check the small and diagnostic sizes without timing them. The
published diagnostic run uses `--standard --host-gate --samples 11`, with
learning disabled. Keep the raw receipts until the complete cohort has been
copied from the measurement host.

## Widget cohort

Wasm now writes zeroed records as wide stores or memory.fill instead of one
byte store per member. In five alternating Alpha rounds, Text measured
3.604 µs against 3.890 µs before, with the paint result checked on every
call; Go measured about 3.84 µs in the earlier cohort. The Button and Input
regression from Boolean specialization is fixed on master. Regenerate the
widget, kernel, JSON/text and example tables at one clean pushed revision on
Alpha, then publish Targets and check the live page.

## Kryon visual workloads

The shader lane's pinned GPU visual suite covers lit meshes, a skinned
character, a textured room and a particle field at 640×360 and 1920×1080,
with 60 hashed frames each. Its first receipt was built from a dirty Kryon
tree at 03016057 rather than the pushed e74f0804, so it is not publishable.
Publish one microsecond table of mean and nearest-rank p95 per scene and
resolution once a clean pushed Kryon revision is measured, and validate the
frame hashes, sample counts and Ziran pin.

## Fixed-array value copies

In the standard suite, the native k-nucleotide profile located 999 of 1,062
samples in the generated 128 KiB copy loop. The diagnostic medians are
1,971 µs for C, 71,099 µs for ZX native and 46,958 µs for Wasm. The source uses a
16,384-element histogram and iterates it by value. Trace the loop's aggregate
loads in both lowerings before changing the benchmark source. Preserve value
semantics for arrays that may escape or be modified through another alias.
