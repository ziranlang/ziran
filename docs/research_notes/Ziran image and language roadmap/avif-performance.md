
# AVIF decoder efficiency and Ziran ownership

## What is the current upstream landscape and what does a complete decode pipeline include?

### Takeaway
AVIF implementations have two distinct layers: an ISO-BMFF/HEIF-style image-container front end and one or more AV1 bitstream engines. libavif and libheif are primarily container/API layers, while libaom, dav1d, libgav1, and FFmpeg's AV1 decoder are the engines; a Ziran replacement must own both layers before it can be called a complete AVIF decoder.

### Cited Findings
- The current Kryon native path is locked libavif 1.3 with libaom, decodes only the first complete sequence frame to straight RGBA8, supports 8/10/12-bit and monochrome input, preserves alpha/clean aperture/rotation/mirror, and uses one CPU thread. It has a 256 MiB file limit, 32768-axis limit, and 67,108,864-pixel limit; ARMv7 disables libaom NEON because unaligned loads SIGBUS on real Android hardware. — [BACKENDS.md](/mnt/storage/Projects/kryonlabs/kryon/docs/BACKENDS.md)
- The current CMake forces libaom decode-only, disables multithreading, turns off libyuv/libsharpyuv, and turns off libavif's dav1d and libgav1 engines. On ARMv7 it explicitly selects portable libaom kernels. — [AVIF.cmake](/mnt/storage/Projects/kryonlabs/kryon/cmake/AVIF.cmake)
- libavif calls itself a portable C implementation of AVIF, supports all AV1-supported YUV formats and bit depths with alpha, and exposes `AVIF_CODEC_AOM` for encode/decode plus `AVIF_CODEC_DAV1D` and `AVIF_CODEC_LIBGAV1` for decode. No AV1 codec is enabled by default. — [libavif README](https://github.com/AOMediaCodec/libavif/blob/main/README.md)
- libheif is an ISO/IEC 23008-12 HEIF and AVIF decoder/encoder. Its AVIF decode choices include libaom, dav1d, and FFmpeg; codecs can be compiled as a subset or dynamically loaded plugins. — [libheif README](https://github.com/strukturag/libheif/blob/master/README.md)
- libgav1 is a C++ AV1 decoder for AV1 Main, High, and Professional profiles, with compile-time maximum bit depths of 8, 10, or 12. — [libgav1 README](https://chromium.googlesource.com/codecs/libgav1/+/refs/heads/main/README.md)
- FFmpeg documents both a native AV1 decoder and an optional `libdav1d` wrapper that lets libavcodec decode AV1. — [FFmpeg codecs](https://ffmpeg.org/ffmpeg-codecs.html); [FFmpeg libdav1d](https://github.com/FFmpeg/FFmpeg/blob/master/doc/decoders.texi)
- A complete AVIF path is container parse and limit validation; item/property interpretation; color and alpha OBU extraction; AV1 uncompressed headers; entropy/tile decoding; prediction, inverse transform, reconstruction, deblocking, CDEF, loop restoration and optional film grain; alpha application; orientation/crop; chroma upsampling; YUV-to-RGB conversion; and allocation of the requested RGBA surface. The AVIF specification defines the format as HEIF-conformant image items and sequences, including derived and tone-map item relationships. — [AVIF v1.2.0 specification](https://aomedia.org/docs/AV1%20Image%20File%20Format%20%28AVIF%29%20v1.2.0.pdf)
- dav1d's optimized engine stages are MSAC, inverse transform, motion compensation, intra prediction, loop filtering, loop restoration, CDEF, and film grain; entropy-related `decode_coef` and `ref_mv` remain major non-assembly stages. — [dAV1d FOSDEM 2020](https://archive.fosdem.org/2020/schedule/event/om_dav1d/attachments/slides/4188/export/events/attachments/om_dav1d/slides/4188/dav1d_1year.pdf)
- libavif's `maxThreads` limits concurrently active CPU-consuming threads, not merely sleeping worker threads. Its optional libyuv RGB/YUV path is faster but can be less precise than the built-in conversion; RGBA is the default RGB output and high-depth RGBA supports 10/12/16-bit containers. — [libavif API](https://github.com/AOMediaCodec/libavif/blob/main/include/avif/avif.h)

### Inferences
- The current bottleneck cannot be inferred without profiling, because the configured pipeline deliberately gives up three known optimization dimensions: dav1d's engine, libaom multithreading, and libyuv/fused conversion.
- Replacing only the AV1 engine does not replace the AVIF front end. To own the stack, Ziran needs both a container module and an AV1 engine, plus explicit color, alpha, orientation, allocation, and scheduler boundaries.
- A still-image benchmark must report the complete path through RGBA output. Benchmarking raw OBUs alone is useful for engine kernels but is not an AVIF decoder benchmark.

### Gaps
- No current source gives authoritative end-to-end time divided into ISOBMFF parse, AV1 decode, and RGB conversion across representative AVIF files; this must be measured locally with the corpus below.

## How fast are the leading software engines, and is 10x realistic?

### Takeaway
The 10x target is credible as a high-priority hypothesis against the present deliberately constrained libaom configuration if multithreading and conversion changes are allowed, but it is not credible as a promise to beat every decoder by 10x under a same-core CPU-time/energy metric. Mature dav1d already uses substantial hand-written assembly and published 2019–2020 evidence showed roughly 2–5x over libaom and 4–10x over then-current libgav1.

### Cited Findings
- dav1d states that it is focused on speed and correctness, production-ready, and portable; its goals include AVX2 desktop assembly, ARMv8 and ARMv7 mobile assembly, SSSE3+ fallback, high-bit-depth assembly, and improved threading. — [dav1d README](https://github.com/videolan/dav1d/blob/master/README.md)
- A 2019 AOMedia presentation claimed dav1d was 2–5x faster than libaom and 4–10x faster than libgav1, with 30–50% lower memory than libaom. Its chart showed dav1d rising from about 88 fps at one core to about 330 fps near six cores, while libaom rose from about 42 to about 100 fps. — [Two Orioles AOMedia Symposium 2019](https://downloads.aomedia.org/assets/pdf/symposium-2019/slides/RonaldBultje_TwoOrioles.pdf)
- FOSDEM 2020 reported 3–5x dav1d-versus-aomdec desktop throughput. Representative 1080p values included Morocco AVX2 at 86 versus 198 fps, Dua Lipa at 45 versus 164 fps, and Chimera at 49 versus 220 fps; ARMv8 multithread values included dav1d 52/54/70 fps versus libaom 16/15/29 fps. — [dAV1d FOSDEM 2020](https://archive.fosdem.org/2020/schedule/event/om_dav1d/attachments/slides/4188/export/events/attachments/om_dav1d/slides/4188/dav1d_1year.pdf)
- The same FOSDEM material explains dav1d's low-level strategy as C plus hand-written assembly and no intrinsics; at that point it had about 64 kLoC of assembly versus 25 kLoC of C. — [dAV1d FOSDEM 2020](https://archive.fosdem.org/2020/schedule/event/om_dav1d/attachments/slides/4188/export/events/attachments/om_dav1d/slides/4188/dav1d_1year.pdf)
- libaom's build exposes multithread support and NEON, SSE through SSE4.2, AVX, and AVX2 optimization controls. — [libaom CMake defaults](https://aomedia.googlesource.com/aom/+/refs/heads/master/build/cmake/aom_config_defaults.cmake)
- libgav1 exposes AVX2, NEON, and SSE4.1 optimization controls, a maximum of 128 threads by default, and frame-parallel use when threads exceed tile count times a configurable threshold. — [libgav1 README](https://chromium.googlesource.com/codecs/libgav1/+/refs/heads/main/README.md)
- Recent dav1d releases continue optimizing x86 SSE/AVX2/AVX-512, AArch64 NEON and Arm extensions, RISC-V, PowerPC, LoongArch, and tile scheduling. — [dav1d NEWS](https://github.com/videolan/dav1d/blob/master/NEWS)
- Fixed-function paths are a separate class of competitor, not just another software kernel. WebCodecs exposes `VideoDecoder` and a `hardwareAcceleration` preference, but the specification does not guarantee that an implementation uses fixed-function hardware or supports every AVIF feature that way. Android's current compatibility document requires AVIF baseline decoding and MediaCodec-exposed image codecs, and performance-class handheld hardware has explicit AV1 hardware-decoder requirements. — [WebCodecs Specification](https://www.w3.org/TR/webcodecs/#videodecoderconfig-dictionary); [Android 15 CDD](https://source.android.com/docs/compatibility/15/android-15-cdd)

### Inferences
- First expected gain: switching the engine and enabling a bounded worker pool is likely to produce a material speedup over the current one-thread libaom build, but local current-version measurements are mandatory because the cited 2–5x results are 2019–2020 video workloads, not 2026 AVIF still images on this hardware.
- Second expected gain: enable or reimplement fast YUV-to-RGB and alpha composition. This is especially relevant for simple, large, or already-parallelized streams where conversion can approach the remaining wall time.
- A 10x wall-time result can become easier by comparing a multithreaded Ziran path with a single-thread baseline, but that is not a 10x CPU-efficiency result. The primary success metric should therefore distinguish wall latency, throughput, CPU time per pixel, active cores, peak RSS, allocations, and energy where available.
- A blanket claim of 10x faster than “any decoder” is scientifically and competitively unsafe. Fixed-function hardware, browser implementations, and dav1d all have mature acceleration. The target should be staged: measure, beat the current path, match or approach current dav1d per-architecture, then invest in kernel breakthroughs with profile-gated evidence.
- Threading benefit depends on AVIF structure. Color and alpha can sometimes run concurrently, while tile parallelism requires enough independent tiles; a one-tile still image has less tile-level parallelism. Any speedup must therefore be reported by encoded structure.

### Gaps
- No fresh, reproducible benchmark of 2026 libavif+libaom versus libavif+dav1d and libgav1 on the target AVIF corpus and machines was found; historical charts cannot be substituted for current conclusions.
- No trustworthy cross-platform energy result suitable for a universal 10x claim was found.

## What exact benchmark design and commands should gate the work?

### Takeaway
Use a two-level benchmark: complete AVIF-to-RGBA8 latency/throughput and raw AV1/OBU engine time, with identical output bytes or declared tolerance, fixed cores, warm and cold-page modes, and CPU/memory counters. Do not claim a speedup from container CLI file-writing overhead alone.

### Cited Findings
- libavif's API owns the relevant complete-pipeline boundary, including `maxThreads`, RGB image setup, chroma upsampling choice, and libyuv versus built-in conversion. — [libavif API](https://github.com/AOMediaCodec/libavif/blob/main/include/avif/avif.h)
- libavif provides `avifdec` as its decoder command-line tool and supports choosing an AV1 codec. — [libavif README](https://github.com/AOMediaCodec/libavif/blob/main/README.md); [avifdec manual](https://aomedia.googlesource.com/libavif/+/HEAD/doc/avifdec.1.md)
- dav1d supports enabling test-data and conformance tests through Meson, after cloning `dav1d-test-data`, and running `meson test -v`. — [dav1d README](https://github.com/videolan/dav1d/blob/master/README.md)
- libavif has OSS-Fuzz instructions and local OSS-Fuzz build guidance. — [libavif OSS-Fuzz notes](https://aomedia.googlesource.com/libavif/+/HEAD/tests/oss-fuzz/README.md)

### Inferences
- The corpus should be versioned by hash and cover: 8/10/12-bit; 4:2:0/4:2:2/4:4:4/monochrome; alpha and no-alpha; tiny icons through the current 67,108,864-pixel limit; one-tile and multi-tile; lossless; screen-content tools; film grain; grids; crop/rotate/mirror; progressive/layered input; malformed and truncated files; and RGB/YUV limited/full-range combinations.
- For the existing product surface, correctness-equivalent output means first complete frame, straight non-premultiplied RGBA8, and the same orientation/crop behavior. A second native-depth/HDR mode must not silently reuse RGBA8 tolerances.
- Prefer a purpose-built harness around each library API with a caller-owned RGBA buffer and decode-object reuse, then use CLI tools only as smoke comparisons. Pin cores, fix frequency/governor where permitted, repeat enough trials to report p50/p95, separate warm cache from freshly loaded pages, and record compiler, ISA dispatch, OS, file hash, and engine version.
- A speed mode may use libyuv-equivalent conversion if its rounding differences are documented; a strict mode must retain the existing pixel goldens. The gate should compare both to prevent trading correctness for a misleading speed result.

### Proposed benchmark commands
These are unverified exploratory recipes, not executable acceptance gates and not commands run during this research-only task. In particular, `/dev/null` may be rejected by tools that probe or format their output; validate each command against the installed `--help` text and use real output paths in the established test layout before recording results.

```sh
# Complete-container smoke comparison, one and bounded-many active threads:
hyperfine --warmup 5 --runs 50 --export-json avif-aom-j1.json \
  'avifdec --jobs 1 --codec aom corpus/photo.avif "$AVIF_AOM_OUT"'
hyperfine --warmup 5 --runs 50 --export-json avif-dav1d-j1.json \
  'avifdec --jobs 1 --codec dav1d corpus/photo.avif "$AVIF_DAV1D_OUT"'
hyperfine --warmup 5 --runs 50 --export-json avif-dav1d-jN.json \
  'avifdec --jobs "$THREADS" --codec dav1d corpus/photo.avif "$AVIF_DAV1D_OUT"'

# CPU and memory envelope for the best candidates:
perf stat -e task-clock,cycles,instructions,branches,branch-misses,cache-misses,LLC-load-misses \
  -- avifdec --jobs "$THREADS" --codec dav1d corpus/photo.avif "$AVIF_DAV1D_OUT"
/usr/bin/time -v avifdec --jobs "$THREADS" --codec dav1d corpus/photo.avif "$AVIF_DAV1D_OUT"

# Engine-only OBU/IVF comparison where a matching elementary stream exists:
hyperfine --warmup 5 --runs 50 'dav1d --threads 1 --muxer md5 corpus/photo.obu'
hyperfine --warmup 5 --runs 50 'aomdec --threads=1 --rawvideo corpus/photo.ivf "$AOM_RAW_OUT"'
hyperfine --warmup 5 --runs 50 'gav1_decode --threads 1 --input corpus/photo.ivf'

# Pin one representative core family and disable inherited display/session variables:
env -u DISPLAY -u WAYLAND_DISPLAY taskset --cpu-list "$CPUS" \
  perf stat -e task-clock,cycles,instructions,cache-misses \
  -- ./build/avif-bench --engine ziran --threads "$THREADS" \
       --input corpus/photo.avif --output rgba --mode strict --report json
```

On Android, use the same corpus and a custom API harness under `simpleperf stat`, record `/proc/<pid>/status` peak values, and test each ABI separately. Do not launch or exercise a real desktop display.

### Gaps
- Exact `avifdec`, `dav1d`, `aomdec`, and `gav1_decode` option syntax can differ by release; the authoritative implementation task must confirm flags before recording headline numbers.

## What is the staged Ziran-owned modular implementation roadmap?

### Takeaway
Build a replaceable engine boundary first, prove correctness and measurement, then replace modules from the outside in: AVIF container, scheduler/allocation, color pipeline, AV1 headers/entropy, reconstruction, and finally ISA kernels. This supports existing-library bridging now without pretending that an FFI is ownership.

### Cited Findings
- libavif's CMake model already isolates engine choice behind common container code, allowing libaom, dav1d, or libgav1 as separate dependencies. — [libavif README](https://github.com/AOMediaCodec/libavif/blob/main/README.md)
- libheif similarly supports subset/plugin back ends, including AVIF decode through libaom, dav1d, or FFmpeg. — [libheif README](https://github.com/strukturag/libheif/blob/master/README.md)
- dav1d identifies MSAC, inverse transform, motion compensation, intra prediction, loopfilter, loop restoration, CDEF, and film grain as its assembly-aware stages, while `decode_coef` and `ref_mv` remain substantial non-assembly work. — [dAV1d FOSDEM 2020](https://archive.fosdem.org/2020/schedule/event/om_dav1d/attachments/slides/4188/export/events/attachments/om_dav1d/slides/4188/dav1d_1year.pdf)
- dav1d continues active work on x86 AVX2/AVX-512 and SSE, AArch64 NEON and newer Arm features, ARM32, RISC-V, PowerPC, and LoongArch, including threading and security fixes. — [dav1d NEWS](https://github.com/videolan/dav1d/blob/master/NEWS)
- The AVIF specification places AVIF in a HEIF-conformant item/sequence model and includes derived and tone-map relationships; decoder arithmetic must detect under/overflow and clamp as specified. — [AVIF v1.2.0 specification](https://aomedia.org/docs/AV1%20Image%20File%20Format%20%28AVIF%29%20v1.2.0.pdf)

### Inferences
- Phase 0 should be measurement-only: add a benchmark/corpus contract and profiling boundaries before changing decoders. This task intentionally stopped at research.
- Phase 1 should introduce a Ziran AVIF facade and `Engine` interface, initially binding libaom and dav1d. The facade must express input limits, thread budget, output color space, crop/orientation, allocation mode, diagnostics, and cancellation rather than exposing borrowed C structs.
- Phase 2 should own the AVIF container and color/alpha/orientation path while still delegating AV1. It should parse boxes defensively, decode only needed items, support caller-owned buffers, and provide strict versus fast conversion modes.
- Phase 3 should replace AV1 control flow: OBU parsing, headers, tile scheduling, reference/state ownership, and allocation. Tile and frame worker pools should have bounded queues, deterministic cancellation, no unbounded recursion, and explicit backpressure.
- Phase 4 should replace DSP kernels one family at a time. Every kernel needs an authored Ziran scalar/reference implementation, differential vectors, randomized malformed input, alignment/overlap contracts, checkasm-style bit-level tests, and profile evidence before ISA-specific code is accepted. Generated C remains compiler output; upstream C implementations are differential-test oracles only during migration.
- Phase 5 should own packaging/build integration and ABI stability. Ziran should export only its own decoder API and keep optional dav1d/libaom bridges isolated for conformance comparisons during migration.

### Per-ISA kernel roadmap
- x86-64: authored Ziran scalar correctness baseline, SSE2/SSSE3 compatibility path, AVX2 as the first priority, AVX-512 only where profiles show a durable win and code-size/downclock tradeoffs are acceptable. This follows upstream's continued investment in SSE/AVX2/AVX-512. — [dav1d NEWS](https://github.com/videolan/dav1d/blob/master/NEWS)
- AArch64: NEON baseline, then profile-gated DotProd/i8mm/SVE2 paths for prediction, convolution, and high bit depth; preserve unaligned-input-safe entry points and cache/TLB behavior. Upstream treats ARMv8 and newer Arm extensions as active optimization targets. — [dav1d README](https://github.com/videolan/dav1d/blob/master/README.md); [dav1d NEWS](https://github.com/videolan/dav1d/blob/master/NEWS)
- ARMv7: NEON is important, but the current real-device SIGBUS makes an explicit alignment/aliasing contract a release blocker. Start with safe loads, allocator/plane alignment, and a runtime dispatch test; do not re-enable the locked libaom path without real-device verification. — [AVIF.cmake](/mnt/storage/Projects/kryonlabs/kryon/cmake/AVIF.cmake); [BACKENDS.md](/mnt/storage/Projects/kryonlabs/kryon/docs/BACKENDS.md)
- RISC-V: implement RVV 1.0 after C/NEON paths are stable, with VLEN-agnostic code and separate low/high bit-depth kernels. Upstream continues active RISC-V assembly work. — [dav1d NEWS](https://github.com/videolan/dav1d/blob/master/NEWS)
- POWER/PowerPC64 and LoongArch: keep the authored Ziran scalar/reference implementation correct first and add profile-gated assembly after the core conformance suite is stable; upstream maintains these paths but they should not precede x86-64/AArch64/ARMv7/RISC-V. — [dav1d NEWS](https://github.com/videolan/dav1d/blob/master/NEWS)
- Hardware: expose a separate optional platform backend behind the same `Engine` interface where capabilities, memory layout, color output, HDR behavior, and fallback are explicit. Android MediaCodec and WebCodecs are not evidence that fixed-function hardware accelerates every AVIF feature or that a CPU engine can universally beat it. — [Android 15 CDD](https://source.android.com/docs/compatibility/15/android-15-cdd); [WebCodecs Specification](https://www.w3.org/TR/webcodecs/#videodecoderconfig-dictionary)

### Implementation Dependencies
- Ziran needs stable low-level module contracts for bounded buffers, integer arithmetic, alignment, slices/stride, explicit ownership, cancellation, runtime CPU dispatch, C ABI bridges, and deterministic cross-compilation. Compiler/LLM-language changes belong to the sibling tracks and must not be mixed into decoder migration.
- Optional external dependencies during migration are dav1d, current libavif/libaom/libgav1 only as comparison bridges, libyuv or an owned replacement, upstream conformance/fuzz corpora, `perf`/`hyperfine`/`simpleperf`, ASan/UBSan/MSan-compatible builds, and per-ISA cross compilers.
- No production implementation, build, language change, migration, commit, push, or child launch is authorized by this research task.

### Gaps
- The exact package names, version locks, and cross-compilation entry points for the Ziran bridge must be selected by the implementation task after the architecture migration and language-surface siblings define their boundaries.

## What correctness, color, memory, conformance, and malformed-input gates are mandatory?

### Takeaway
A conformant fast decoder requires bitstream conformance, pixel/color-vector tests, adversarial input tests, allocation-failure tests, sanitizer matrices, and per-ISA differential testing before any performance number is release-worthy.

### Cited Findings
- dav1d documents optional but encouraged decoder conformance tests, backed by `dav1d-test-data`, `-Dtestdata_tests=true`, and `meson test -v`. — [dav1d README](https://github.com/videolan/dav1d/blob/master/README.md)
- libavif documents OSS-Fuzz fuzzing and local OSS-Fuzz builds. — [libavif OSS-Fuzz notes](https://aomedia.googlesource.com/libavif/+/HEAD/tests/oss-fuzz/README.md)
- libgav1 builds unit tests with CTest, accepts an explicit test-data path, and supports Android test-data placement. — [libgav1 README](https://chromium.googlesource.com/codecs/libgav1/+/refs/heads/main/README.md)
- libavif explicitly warns that libyuv is faster but may be less precise, while its API exposes chroma-up/downsampling and RGB depth choices. — [libavif API](https://github.com/AOMediaCodec/libavif/blob/main/include/avif/avif.h)
- The AVIF specification requires decoder arithmetic to check underflow/overflow and clamp results. — [AVIF v1.2.0 specification](https://aomedia.org/docs/AV1%20Image%20File%20Format%20%28AVIF%29%20v1.2.0.pdf)
- The existing local surface already has bounded decode and rejection tests for invalid/truncated containers, invalid crops, and oversized images, plus ARMv7 real-device goldens. — [BACKENDS.md](/mnt/storage/Projects/kryonlabs/kryon/docs/BACKENDS.md)

### Inferences
- Gate 1, API/container semantics: every box combination, item type, property, crop, rotation, mirror, alpha item, grid, and limit rejection must have a deterministic result. No image may be returned for the existing invalid/oversized classes.
- Gate 2, AV1 conformance: run the full upstream AV1 vector suite per profile, bit depth, chroma mode, and film-grain mode; compare decoded planes before RGB conversion, not only final PNGs.
- Gate 3, color: use reference YUV values for 8/10/12-bit, full/limited range, all matrix/primaries/transfer combinations exposed by the API, monochrome, premultiplication, and strict versus fast chroma filters. Preserve current RGBA8 goldens unless a documented color-mode migration is explicitly approved.
- Gate 4, differential per-ISA: compare every assembly kernel against the authored Ziran scalar/reference implementation under random and boundary parameters, including unaligned strides, overlapping buffers, minimum/maximum dimensions, all transform sizes, and both bit-depth templates.
- Gate 5, malformed input and memory: fuzz container and OBU boundaries; use ASan, UBSan, MSan where available, valgrind or equivalent on releases, allocation-failure injection, cancellation, decode-object reuse, and multi-thread stress. Track peak RSS, resident planes, worker stacks, and temporaries as explicit budgets.
- Gate 6, stability: repeated decode, interruption at every stage, backend switching, and cross-ABI tests must not leak, retain buffers, deadlock, or permit stale callbacks. ARMv7 must include the real Android device suite before NEON dispatch is accepted.
- Performance regressions should be non-blocking only until correctness is achieved; after correctness, add p50/p95 latency, CPU time per pixel, throughput, peak RSS, allocation count, and binary-size budgets to the per-ISA CI matrix.

### Gaps
- A current authoritative count and downloadable index of all AV1/AVIF conformance vectors was not established here; the implementation task should pin exact upstream vector commits/hashes and make corpus acquisition reproducible without fetch-at-build-time behavior.
