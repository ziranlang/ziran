# JSON string scanning and parsing

The 0.4.2 JSON readers share an allocation-free scanner that examines complete
16-byte blocks through portable SIMD operations. Unescaped text remains a borrowed
view. Escape decoding, error positions, duplicate member behavior and secure
cleanup retain their existing policies. Short strings use the scalar tail.
`Simd.Load` and `Simd.LoadLow` accept immutable strings for `u8` vector lanes;
whole-load bounds checks apply on C, Go and the portable VM.

Run `sh bench/json_document.sh`. It builds through Zi2c with the normal native
executable defaults, constructs a 1 MiB ASCII string outside the timer and checks
every parse. Eleven batches of 64 parses report raw nanoseconds per parse and
symmetric inclusive-quartile 1.5×IQR exclusions. Process startup, compilation,
input construction and transport are excluded. The scanner-only comparison is
`bench/json_string_scan.zi`; it checks byte semantics before timing alternating
reference and vector scans. Repeated scans use an opaque start and a compiler
memory barrier to prevent loop hoisting.

The retained reports name the CPU, compiler inputs and exact runtime binaries.
These are warm-cache local measurements on a busy Ryzen 9 9950X Linux machine;
they do not establish performance on other architectures. Go and the portable
VM use the same lane semantics; native vector instructions depend on the target.

The corresponding Xcoder diagnostic also measures the original preserved Codex
V8 host. It times repeated `JSON.parse` inside that actual host and `JsonRead`
inside the sandboxed Zi2c cell, separately from the complete cell round trip.
The JavaScript timer has 1 ms resolution, amortized across 512 parses per batch.
Both validate payload length and its first and last bytes on every parse.

A parser improvement does not remove native sandbox creation and executable
startup. The original single-cell JSON benchmark must remain visible alongside
the direct parser result. Cold custom compilation is reported separately from
warm execution. Ordinary precompiled tool plans are a different workload and do
not invoke the custom-cell compiler or sandboxed executable.
