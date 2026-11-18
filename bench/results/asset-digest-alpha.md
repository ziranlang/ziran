# Executable asset digest throughput on Alpha

The reader now uses the existing C streaming SHA-256 implementation. It feeds
whole strings, encodes small integer fields in one stack buffer, and uses the
runtime's shared constants and CPU dispatch. The separate Ziran compressor is
removed. Domain tags, integer byte order, budget accounting and identity checks
are unchanged.

| Synthetic asset | Before read (ms) | After read (ms) | Before MB/s | After MB/s |
| --- | ---: | ---: | ---: | ---: |
| 1,000,000 bytes | 5.088 | 3.890 | 196.5 | 257.1 |
| 75,000,000 bytes | 372.171 | 292.035 | 201.5 | 256.8 |

The 75 MB read takes 21.5% less time. These are direct `ZxExecRead` timings on
physical Alpha (Intel Core i7-8700), pinned to CPU 3, compiled with `-O2` and
learning off. Each series has one warmup and eleven retained raw samples.
Before and after were measured sequentially; they were not interleaved.
All program, asset and content identities match between implementations.
The same symmetric 1.5×IQR rule with inclusive interpolated quartiles applies
to every series. Excluded sample indices remain in the metadata.

The baseline reader source matches the current checkout before the change.
Only `compiler_exec_link.zi` changed on Alpha between accepted measurements;
the linked runtime archive, compiler, probe and other sources stayed fixed.
The temporary reader source edit was restored afterward. Source, executable,
generated-reader and runtime hashes, exact commands, excluded indices and
computed medians are in [metadata](asset-digest-alpha-metadata.json).
Raw samples are in [before](asset-digest-alpha-before.jsonl) and
[after](asset-digest-alpha-after.jsonl). An earlier exploratory baseline,
before adding all identity fields to the probe's output, is retained in
[initial baseline](asset-digest-alpha-before-initial.jsonl) and is not used
for the table. The reusable probe is [asset_digest.zi](../asset_digest.zi).

This is a synthetic asset-size measurement, not Breeze's startup or first-frame
time. Android SHA2 acceleration and the application's nested bundles still
need measurements on the device. A parent asset digest and a nested bundle's
program/asset/file digests cover different byte streams and domain tags, so
this change does not skip or reuse any of those validation steps.

Local verification: the executable bundle test passes independent SHA-256
asset identity vectors at 17 sizes (empty payload, SHA padding and block
boundaries, binary bytes, and more than 1 MiB), plus reader rejection after
payload mutation. Forced portable SHA dispatch passes the same bundle cases.
`env -u DISPLAY -u WAYLAND_DISPLAY ZIRAN_LEARN=off build-queue -- sh -c
'make check && make check-bootstrap'` passes all 271 retained local tests
(284.7 seconds, none reused), and bootstrap output matches. The benchmark
registry test also passes; `bench/bench.zi -- asset-digest` supplies the
compiler module path and runtime archive used by the probe.
