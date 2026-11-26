# Hello World and native runtime measurements on Alpha

Measured directly on physical Alpha (Intel Core i7-8700, Linux x86-64),
with the same locked Kryon `6c3aaf46684cdabb640a10fc560ee48bd052a4cb`
consumer and unchanged Hello World source on both sides. Package fetch and
building the toolchain are excluded from application build times.
The host is shared; these are observations, not a controlled frequency campaign.

`before` uses Ziran `6edbcebbf7907e1a2e3032ba21a31247ff9afcf3`.
`after` uses `ad95cb3fc4aa9a08cebd7d76351b5c2deb3a206b` (compiler changes
from `5167dd5f`). That first cleanup increased C build time. `final` uses
`0f3cb63c9c658ed8d6b9484fae9ff4b52a11db51`, which avoids rechecking the
whole program on every pruning pass and gives packaged foreign bindings
readable names. All three versions are retained, including the regression.

## Hello World builds

One warmup and nine builds per target, rotating target order; warm filesystem,
package and Go caches. Medians use symmetric 1.5×IQR filtering with inclusive
interpolated quartiles. Raw samples and excluded indices are in
[hello-before.json](hello-before.json), [hello-after.json](hello-after.json)
and [hello-final.json](hello-final.json).

| Target | Before, ms | Initial cleanup, ms | Final, ms | Final artifact, bytes |
| --- | ---: | ---: | ---: | ---: |
| C plus cc/link | 3190.68 | 3731.94 | 3296.28 | 403200 |
| Go plus go build | 1103.87 | 1144.45 | 1197.10 | 2038065 |
| Wasm | 1166.09 | 1116.67 | 1169.10 | 384621 |
| Direct Linux x86-64 native | 1320.92 | 1307.29 | 1260.25 | 598224 |
| ZX bundle only | 1077.43 | 1100.66 | 1065.70 | 1471959 |

The final pruning change recovers most of the C regression (11.7% faster
than the initial cleanup), but C remains 3.3% slower than before. Go also
remains slower. These measurements do not establish a speedup across targets.

These are successful builds, not five working GUI hosts. In particular, Kryon
has no Go window host; its unsupported-host result does not display Hello World.
Wasm needs its web host, and the bundle uses the installed desktop host.
Sizes exclude system libraries and portable host libraries.

C output fell from 75 C files plus 83 headers (158 total) to 70 C files plus
78 headers (148 total), without combining modules. Generated source bytes fell
from 1,245,478 to 1,168,348 and the executable from 409,704 to 403,200 bytes.
The requested 60% file reduction is **not achieved**: this is a 6.3% reduction.

## Full compiler rebuild

`make -B -j2 all`, one warmup and three measured rebuilds per version, warm
filesystem caches. This builds all native compiler tools and `libziran.a`,
including 317 C compile commands; it excludes tests, installation and the site.
Both reports retain all samples, host and compiler metadata, source fingerprints,
and the same symmetric IQR statistics.

| Revision | Median, seconds |
| --- | ---: |
| Before | 134.010 |
| Initial cleanup | 136.872 |
| Final | 131.375 |

See [compiler-before.json](compiler-before.json),
[compiler-after.json](compiler-after.json) and
[compiler-final.json](compiler-final.json). The final result is about 2 minutes
11 seconds with two jobs. Its three samples are 131.699, 131.375 and 131.140
seconds, with none excluded. The observed 2.0% difference from before is small
on this shared host; three samples do not establish a general full-build
speed improvement. `working_tree_dirty` includes untracked benchmark reports;
the tracked build-source diff hashes are empty and unchanged.

## ZX versus generated C runtime

These are arithmetic/record kernels, **not Hello World rendering or startup**.
The same Ziran algorithm is compiled to a native ZX bundle and to C with zi2c,
then compiled with cc -O2. Runtime inputs and checksums are verified. Warm
measurements exclude loading, compilation and process startup; native backend
selection is checked. ZX obtains three runtime inputs through host calls;
the C control receives them as arguments. See the protocol and raw samples in
[zx-runtime-before.json](zx-runtime-before.json) and
[zx-runtime-after.json](zx-runtime-after.json) and
[zx-runtime-final.json](zx-runtime-final.json). The table below uses `final`.
The central benchmark driver verifies the physical Alpha host; the additional
environment-based scoped-git attestation was not requested and is false in
the raw runtime metadata.

| Warm kernel, 100,000 rounds | Native ZX, ms | zi2c plus cc -O2, ms | Ratio |
| --- | ---: | ---: | ---: |
| Integer arithmetic | 0.119732 | 0.115020 | 1.041× |
| Indexed record updates | 0.548411 | 0.170202 | 3.222× |

Native ZX is close to C on this integer kernel. The record kernel remains
substantially slower, so general native speed parity is **not achieved**.
The generated machine code copies packed record values through stack storage
around field updates; scalar replacement remains necessary.
