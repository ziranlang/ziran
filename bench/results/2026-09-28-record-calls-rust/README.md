# Generated-Rust record-call baseline

This baseline was produced by:

```sh
env -u DISPLAY -u WAYLAND_DISPLAY python3 bench/record_calls.py --out bench/results/2026-09-28-record-calls-rust
```

It was measured on clean head `7aaeedac46ad81974ca5844eef274bc0537460eb` with no modified tracked files.
The run contains 238 validated samples across 58 phase/case combinations.
Every output matched the independent Python oracle; source and saved-IR emission
was identical for C, C++, Go, and Rust, and the two `.zib` bundles are byte-identical.
Generated Rust participated from source and saved IR with Cargo/Rust
1.85.1.

See [benchmark.md](benchmark.md) for medians and ranges. These are fresh-process
measurements on one unisolated machine; they identify workload behavior and are
not a general language ranking.
