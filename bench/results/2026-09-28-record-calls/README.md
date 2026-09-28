# Record-call benchmark result

Run on 2026-09-28 from clean Ziran commit `1f5ce1b8c667b5f1941c26cb359147e1d9c53ebb`:

```sh
env -u DISPLAY -u WAYLAND_DISPLAY python3 bench/record_calls.py
```

The run collected 206 validated samples across 50 phase/case combinations. It
used three measured compilation samples plus one discarded warmup and five
measured runtime samples plus one discarded warmup per case. Inputs were 1,000
and 500,000 rounds over a fixed 32-record table.

Every runtime output matched the independent Python oracle. Generated C, C++,
and Go were identical from source and saved IR, and the source and saved-IR
bundles were byte-identical. Source hashes did not change during measurement.
All optional comparison toolchains were installed for this run.

For the 500,000-round input, median fresh-process times were about 25 ms for
handwritten C, C++, and Rust; 29 ms for handwritten Go; 141 ms for Java;
264 ms for JavaScript; and 10.8 s for Python. Generated Ziran C/C++ medians
were about 166–171 ms and generated Go about 220–228 ms. These are workload
observations with startup included, not a general language ranking.

See `benchmark.md` for every phase and case, `metadata.json` for commands,
versions, hashes, CPU affinity, and caveats, and `samples.jsonl` for raw
samples.
