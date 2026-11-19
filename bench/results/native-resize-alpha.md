# Native vector growth on physical Alpha

Retain the allocator change: Vectors improves in all 11 paired rounds. The filtered median falls from 40.038 to 38.188 µs per call (4.62% less time), against 34.069 µs for generated C at `cc -O2`. The absolute gap to C falls by 31%.

| Kernel | C µs | Native before µs | Native after µs | Change |
|---|---:|---:|---:|---:|
| Sieve | 35.036 | 38.917 | 38.876 | -0.11% |
| Records | 16.415 | 31.669 | 31.486 | -0.58% |
| Hash | 27.738 | 27.797 | 27.879 | +0.30% |
| Vectors | 34.069 | 40.038 | 38.188 | -4.62% |
| Printing | 9.582 | 9.630 | 9.766 | +1.41% |

Printing is 1.41% slower in this run. The other controls vary by at most 0.58%; the result supports a vector-growth improvement.

The C runtime previously allocated a zeroed replacement, copied the old contents, and freed the original for every large resize. It now uses `realloc` when both blocks are outside the small classes, repairs the instance allocation list if the address moves, and zeros newly exposed bytes. Failed allocation leaves the old block and accounting intact. Checked pointer retirement still happens on every successful resize, including growth at the same address. No checks were made mandatory and the ABI is unchanged.

The native static archives contain 96 members. A streaming comparison verified that only `zir_host_native.o` changed. Both executable hosts use identical backend flags and the same bundles; the frozen compiler/optimizer members are identical. Artifact, source, library, captured image and harness identities are in [the provenance receipt](native-resize-alpha-provenance.json).

Method: 11 shuffled matched rounds across C, before and after on all five existing kernels, on physical Alpha. Every run warms for at least 150 ms, calibrates its batch count, and collects 11 batch samples. The shared clock/sibling host gate applies to all three variants. The same inclusive interpolated 1.5×IQR rule filters each run and the run medians; raw samples, excluded indices, rejected gate attempts, execution order, checksums and stdout are retained in [the measurement receipt](native-resize-alpha-matched.json). Vectors returns `94905000` on every call. Each kernel agrees with the instruction engine and the C reference. Learning is off.

The existing small-tier native host retains its metered policy. These are in-process steady-state timings; startup, phone, saved-cache and unmetered standard-suite performance were not measured here.

The existing SIGPROF sampler captured 557 baseline samples and 493 candidate samples. Native-image share rises from 71.9% to 79.3%; host/libc share falls from 28.1% to 20.7%. Nearest exported libc names are approximate because libc uses internal/IFUNC implementations. These samples indicate where time moves; timing comes from the matched receipt. The captured loop instructions are unchanged. [Candidate profile](native-resize-alpha-profile-after.md). Baseline profile receipt: `8bae9eea959fa9cf837660d529e1301e2e2f`.

Focused local tests passed: `make check CHECK_FILTER=zx_lowered_parity` (18.9 s), `CHECK_FILTER=proven_pointer` (6.0 s), `CHECK_FILTER=checks_granularity` (1.2 s), and `CHECK_FILTER=machine_elf` (11.5 s), after three source checks. The heap fixture covers interleaved growth/free of several live buffers; the checked-pointer fixture now grows an already-large block. Focused receipt: `903a93b24d4b0da0389979acd150fcba10b7`.

The shared local suite executed all 273 retained tests: 272 passed on the full run, and the audio test passed its focused rerun after refreshing a stale CMake compiler path (cache-only repair; no source changes). Bootstrap matched. The allocator implementation and fixtures were unchanged throughout that gate.

The integration task installed the current working tree on Omega and verified all 11 installed executable hashes against `build/bin`; the installed compiler printed `Hello, World!`. The existing Harmony launcher was preserved. Installation receipt: `4e599324b48dc56562619c50362f20771b9d`. A coordinated commit and push remain with the integration task; this lane made no separate publication.

Reproduce using the frozen runtime hosts:

```sh
env -u DISPLAY -u WAYLAND_DISPLAY ZIRAN_LEARN=off python3 bench/native_resize_ab.py --before backup/native-float-calls/native-runner-before --after backup/native-float-calls/native-runner-after --output bench/results/native-resize-alpha-matched.json
```

The driver refuses to overwrite an existing receipt. Frozen hosts are private ignored recovery artifacts. The baseline archive was copied, its native C object rebuilt with the normal Makefile rule after the allocator-only source patch, and only that object replaced before relinking the candidate host.
