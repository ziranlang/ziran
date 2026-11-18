# Targets benchmark cohort and follow-up

The standard diagnostic cohort contains 29 workloads. Each uses the same inputs
and algorithm in the independent C and Go references and in the Ziran program.
Run `bench/bench.zi -- wasm-tier --standard --standard-verify` through
`build-queue` to check the small and diagnostic sizes without timing them. The
published diagnostic run uses `--standard --host-gate --samples 11`, with
learning disabled. Keep the raw receipts until the complete cohort has been
copied from the measurement host.

## Wasm widget Text

Wasm lowering now copies only the snapshot fields that are read, writes a
single replaced field back in place, and fills fresh allocations straight from
their source. In five matched Alpha rounds, Text measured 3.851 µs in Wasm and
3.848 µs in Go, with the paint result checked on every call. One more Text
change should put Wasm ahead. The hottest warm procedures are TextTokenNext,
NextTextLineUncached, QueuePaint and UTF-8 Decode. Multi-value record
returns, flat record arguments, leaf stack-pointer elision and SIMD copies
were tried and set aside; none gained more than 1.3% in paired runs.

## Button and Input regression

At `343de87f`, Boolean specialization made Button and Input 2.6 to 3.6
times slower in both ZX native and Wasm. The `TreeLayerAt$1` clone takes 6,567
of 10,020 samples in the Button profile: it still copies a large record whose
only use the folded branch removed, then reloads the address to read member 21.
The standard benchmark lane owns the fix. Regenerate the full Alpha cohort at
one clean pushed revision after it lands, then publish Targets.

## Kryon visual workloads

The initial CPU run verified all 30 frame hashes against a second Alpha run
at 960×600 and 420×900. It covered clear and resolve, unlit geometry,
textured geometry, a 12-instruction shader, and lit geometry. At 960×600 the
textured scene measured 24,875 µs mean and 25,217 µs p95. The lit scene
measured 53,646 µs mean and 53,750 µs p95. These runs used three timed frames
per scene and viewport and establish a diagnostic baseline.

Use the upstream visual suite for the site. Keep completed-frame mean and
nearest-rank p95 in microseconds, with warmup, shader compilation and output
hashing outside the measured duration. Publish per-frame hashes and the exact
Kryon source and tool revisions alongside the table. The browser workload
uses the CPU renderer. The native GPU workload requires the physical EGL/GLES
renderer identity and its own verified output; software rendering belongs in
correctness runs. The pinned cohort needs the richer UI and 2D effects as well
as the animated 3D scenes before publication.

## Fixed-array value copies

In the standard suite, the native k-nucleotide profile located 999 of 1,062
samples in the generated 128 KiB copy loop. The diagnostic medians are
1,971 µs for C, 71,099 µs for ZX native and 46,958 µs for Wasm. The source uses a
16,384-element histogram and iterates it by value. Trace the loop's aggregate
loads in both lowerings before changing the benchmark source. Preserve value
semantics for arrays that may escape or be modified through another alias.
