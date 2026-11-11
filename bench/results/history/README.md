# Historical benchmark records

These samples, summaries, metadata and profiles were retained before the old
`backup/` directory was removed. `migration.json` records every original path,
retained file and SHA-256 hash. Compressed records keep their original bytes.

They describe older revisions and experiments, including warmups and failed
baselines where those are recorded. Keep their original correctness labels,
instrumentation caveats and provenance when interpreting them. Paths inside a
record are historical; old executables and downloaded toolchains were removed.

The maintained tools in `bench/` supersede the old one-off wrapper scripts.
Fresh compiler rebuild timing and recipe profiling use `bench/compiler_build.zi`.
