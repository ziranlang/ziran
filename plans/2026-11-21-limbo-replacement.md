# Replacing Limbo and Dis with Ziran and ZX in TaijiOS

Analysis and implementation plan, 2026-11-21. No compiler or runtime source files were modified by this task; the only files added are this plan and the reproducible source scanner [`scripts/scan_limbo_migration.py`](../scripts/scan_limbo_migration.py).

> Owner direction (verbatim): "make zure ziran becomes a full replacemnt of limob that we are at least ass effiecn as that psobiley at the levl of c and betetr than ci si the goal"

## Baseline and repository state

- **Ziran** (`ziranlang/ziran`, working tree on dirty checkout; never reverted or staged): Source → ZIR → LIR → optimized LIR → revision-28 `.zx` → native (`x86_64`, `aarch64`, `armv7`), Wasm (`zx2wasm`), or instruction engine (`cmd/compiler_exec.zi`). Reference docs: [`docs/ZX.md`](../docs/ZX.md), [`docs/IMPLEMENTATION_STATUS.md`](../docs/IMPLEMENTATION_STATUS.md), [`docs/LANGUAGE_DIRECTION.md`](../docs/LANGUAGE_DIRECTION.md), [`docs/OWNED_VALUES.md`](../docs/OWNED_VALUES.md).
- **TaijiOS** (`taijiosnet/taijios`, HEAD `e3846de0`):
  - **946** `.b` Limbo application/library files under `appl/` (**471,331 lines**), plus **26** non-`appl` `.b` files (`os/init/*.b`: 19, `build/*/*.b`: 4, `os/ipaq1110/tstdraw.b`: 1, `module/srvrunt.b`: 1, `emu/port/zxbuiltin.b`: 1).
  - **180** `.m` module interfaces under `module/`.
  - **80** `.zi` Ziran files already present under `appl/` (including `cat.zi`, `ls.zi`, `mkdir.zi`, `tee.zi`, `uniq.zi`, `taijios_io.zi`, `taijios_args.zi`, `taijios_files.zi`, `taijios_wm.zi`, `appl/cmd/tcltk/*.zi`, and Kryon WM apps such as Stopwatch, Clock, Desktop Panel, and Rill).
  - The Limbo compiler (`taijios/limbo/*.c`, `limbo.y`) is C/yacc emitting Dis bytecode; `emu` hosts both the Dis interpreter/JIT (`taijios/libinterp/`, `emu/port/dis.c`, `emu/port/discall.c`) and the ZX engine (`emu/port/zx.c`, `emu/port/zxbuiltin.b`, currently running on `BUNDLE_BACKEND_ENGINE` in `Linux/386`).
- **Parallel helper coordination (referenced, not duplicated)**: Another helper is separately implementing the **i386 native tier** and optimizing **ZX instruction-engine hot paths**. This plan does not duplicate those tasks; it integrates them at the `emu` native-execution and benchmark gates (Section 4).

---

## 1. Per-feature gap table: Limbo/Dis vs. Ziran/ZX

Feature prevalence counts below are measured across all **946** `appl/**/*.b` files by `scripts/scan_limbo_migration.py` (with Limbo `#` comments and string literals stripped).

| Feature | Prevalence (946 `.b` files) | Limbo / Dis behavior & evidence | Ziran / ZX current status & evidence | Gap & resolution path |
|---|---|---|---|---|
| **`alt` (multi-channel select)** | 184 (19.5%) | First-class statement selecting over ready send/recv/default channel ops (`taijios/limbo/limbo.y:70,897`; `taijios/libinterp/alt.c:19-265`). | Go-only reflection wrapper `std/select_go.zi:1-46`; no LIR op (`cmd/compiler_lowered_ir.zi:23-40`); no ZX host binding (`taijios/emu/port/zx.c:1697-1780`). | **Blocking for D3/D4.** Add typed `taijios_conc` host capability bridging Inferno `alt()` in `emu/port/zx.c` (T3/T8) and portable native/ZX channel runtime. |
| **Channels (`chan of T`, `<-c`, `c <-= v`)** | 419 (44.3%) | First-class typed synchronous/buffered channels (`taijios/limbo/limbo.y:68,435,1322-1328`; `taijios/libinterp/xec.c`, `heap.c:10`). | Go-only `std/channel_go.zi:1-31`; OS-pipe `std/process_channel_linux.zi`; single-threaded `std/queue.zi`. No portable ZX channel primitive. | **Blocking for D3/D4.** Expose handle-backed typed channels via `std/zx_conc.zi` + `taijios_conc` (T3/T8), using host-image serialization or fixed-width scalar fast paths. |
| **`spawn` (green threads / `Prog`)** | 361 (38.2%) | Lightweight cooperative/preemptive Inferno `Prog`s sharing module/channel state (`taijios/limbo/limbo.y:71,879`; `taijios/emu/port/dis.c`). | OS pthreads (`std/thread.zi`, `std/thread_linux.zi`) and data-parallel `#parallel for` (`docs/LANGUAGE_DIRECTION.md:71-137`, serialized on ZX engine). No `spawn` in ZX or `emu/port/zx.c`. | **Blocking for D3/D4.** Add `taijios_conc.Spawn` launching an Inferno `Prog` running a retained ZX entry/instance with shared channel handles (T3/T8). |
| **`adt` / `pick` (tagged sum types)** | `adt`: 273 (28.9%)<br>`pick`: 104 (11.0%) | ADTs with methods (`self`) and discriminated unions via `pick` (`taijios/limbo/limbo.y:68,70,225-232,307,903`; e.g. `Tmsg`/`Rmsg` in `taijios/module/styx.m:75-115`). | `struct` + `enum` + `if x == { case ... }` (`docs/LANGUAGE_DIRECTION.md:141-147`). `union` in ZX/LIR is **scalar-only** (`cmd/compiler_zir_lower.zi:855-875`: `"portable unions support scalar fields only"`). | **High-leverage gap.** Extend Ziran `union` (or tagged `union(Enum)`) in checker + `compiler_zir_lower.zi` to support record/slice payloads so `Styx` and ASTs port cleanly without heap indirection (T1). |
| **`ref` & `cyclic` (GC heap references)** | `ref`: 878 (92.8%)<br>`cyclic`: 60 (6.3%) | Reference-counted heap objects with cycle collector (`taijios/limbo/limbo.y:431,637,293`; `taijios/libinterp/heap.c:361-461`, `gc.c:36-299`). | Deterministic ownership (`Vec(T)`, `Text`), lexical drops, manual `New(T)`/`free` (`docs/OWNED_VALUES.md:1-174`; `cmd/compiler_zir_lower.zi:3267,3884`), checked-pointer engine (`cmd/compiler_exec.zi:3394`). No GC. | **Architectural translation.** Do not add tracing GC to Ziran. Provide `std/pool.zi` (typed arena/slot-handle pool) and `std/rc.zi` (explicit ref-counted handle) for graph/cyclic structures; convert most `ref T` to values or `*T`/`Vec(T)` (T6). |
| **Lists (`list of T`, `hd`, `tl`, `::`)** | 718 (75.9%) | Built-in immutable singly-linked lists (`taijios/limbo/limbo.y:68,450,1316`; `taijios/libinterp/heap.c:8`), heavily used for `args: list of string` and queues. | No built-in cons-list. `Vec(T)` and borrowed slices `[]T` (`std/vec.zi`, `docs/OWNED_VALUES.md:160-174`). | **Low friction.** Replace `list of T` with `Vec(T)`/`[]T` (faster, cache-local, zero per-node GC) and provide `std/list.zi` only where persistent tail-sharing is required (T6). |
| **Array slices (`a[i:j]`, `array of T`)** | 605 (64.0%) | GC-managed dynamic arrays with checked `[i:j]` sub-slicing (`taijios/limbo/limbo.y:68`; `taijios/libinterp/heap.c:7`). | `[]T` borrowed slices + `Vec(T)` growable arrays (`v[low:high]`, `VecSlice`, `.count`; `docs/LANGUAGE_DIRECTION.md:262-265`, `docs/OWNED_VALUES.md:131-148`). | **Mostly at parity.** Borrowed slice lifetime rules prevent returning slices of local `Vec`s; port returning-slice APIs to return `Vec(T)` or write into caller buffers. |
| **String ops (UTF-8 runes vs. bytes)** | Ubiquitous (`string.m`: 142) | Immutable Unicode strings indexed by **rune** (`s[i]`, `s[i:j]`, `len s`, `+`, `int s`, `array of byte s`; `taijios/libinterp/string.c:116-600`). | Immutable UTF-8 **byte**-indexed `string`, `s[low:high]` byte slice, owned `Text`/`BuilderAppend`/`BuilderFinish` (`docs/LANGUAGE_DIRECTION.md:262-272`, `docs/OWNED_VALUES.md:59-108`), `std/utf8.zi`, `std/text_tools.zi`, `appl/cmd/zx_runes.zi`. | **Semantic pitfall.** Add `std/rune_text.zi` (rune slicing, rune indexing, `string <-> []u8`, numeric parse/format helpers) so Limbo string ports don't silently corrupt multi-byte UTF-8 (T6). |
| **Tuples (`(a, b) := f()`, `(int, string)`)** | Ubiquitous | First-class anonymous product types usable in returns, channels, lists, arrays, and destructuring (`taijios/limbo/limbo.y`). | Multiple procedure return values `-> T, U` (`docs/LANGUAGE_DIRECTION.md:153-154`) and `std/pair.zi`; no anonymous tuple types in structs/channels/arrays. | **Moderate friction.** Keep multi-returns for functions; add anonymous tuple types or ergonomic `Tuple2`/`Tuple3` generics in `std/tuple.zi` (or compiler tuple lowering in T2) for channel/collection payloads. |
| **Modules & `.m` interface loading** | `load`: 828 (87.5%)<br>180 `.m` files | Dynamic `load Mod PATH` and `SELF` at runtime with structural/signature checking (`taijios/limbo/limbo.y:36,183,1140`; `taijios/libinterp/load.c:82-593`, `link.c`). | Static `#import` + dynamic `std/zx_module.zi:1-30` (`Load`/`Release`/`Replace`) with `--library`/`--require-module`/`--import` (`docs/ZX.md:11-100`; `taijios/emu/port/zx.c:1965-2055` searching `/dis/lib/<name>.zx`). | **Partial parity.** `zx_module` already runs inside `emu`! Remaining gaps: (a) dynamic path loading (`load Mod var_path` used by `sh`, `alphabet`, `wm`), (b) rich record/slice marshaling across module calls, and (c) `.m` → `.zi` interface generation (T5). |
| **`$Sys` (files, namespaces, net, proc)** | 840 (88.8%)<br>`ns_ops`: 339 (35.8%)<br>`file2chan`: 40 (4.2%) | Built-in `$Sys` (`taijios/module/sys.m:4-163`, `libinterp/runt.c:15`): open/read/write/stat/wstat/remove/seek, **plus** `dirread`, `pipe`, `dup`, `chdir`, `sleep`, `pctl`, `bind`, `mount`, `unmount`, `export`, `dial`, `announce`, `listen`, `file2chan`, `fversion`, `stream`, `pread`/`pwrite`. | `taijios/emu/port/zx.c:1701-1745` provides `taijios_io` (18 file ops: Open, Create, Read, Write, Seek, Stat, Close, Error, StatPacked, FstatPacked, WstatPacked, FdPath, CreateNew, Remove, Cwd, Home, Sync, RenameSameDir), `taijios_args`, `taijios_stdin`, `taijios_files`, `taijios_clock`, `taijios_tasks`. | **Blocking for W1/W2/W3.** Expand `taijios_sys` in `emu/port/zx.c` with the remaining `$Sys` calls (`dirread`, `pipe`, `dup`, `chdir`, `sleep`, `pctl`, `bind`/`mount`/`unmount`/`export`, `dial`/`announce`/`listen`, `pread`/`pwrite`, `werrstr`) and `file2chan` (T7, T8). |
| **`$Draw` & `$Tk` (graphics & UI)** | `inc_draw`: 687 (72.6%)<br>`draw_ctxt_only`: 545 (57.6%)<br>`real_draw`: 158 (16.7%)<br>`inc_tk`: 131 (13.8%) | Built-in `$Draw` (`libinterp/draw.c:118`, `module/draw.m`) and `$Tk` (`libinterp/tk.c:71`, `module/tk.m:1-25`) + `$Prefab` (`prefab.c:39`) + `$Freetype` (`freetype.c:33`). | `emu/port/zx.c:1699-1778` provides `zx_surface` (RGBA8 framebuffer `Open`/`Event`/`Present`/`PresentOpaqueRows`/`PresentOpaqueStrip`/`Close`/`SetTitle`), `taijios_wm`, `taijios_panel`, `taijios_desktop`, `taijios_wm_call`. Kryon renders widgets in portable Ziran (Stopwatch, Clock, Desktop Panel, Rill). | **Strategic choice, not full Tk clone.** 545 of 687 `draw.m` files only take `Draw->Context` in `init` and need **zero** graphics. For the 186 real GUI files: port core WM/apps to Kryon (`zx_surface`), and add a minimal `taijios_draw` bridge only if legacy Draw-direct tools (`acme`, image tools) require it (T16). |
| **`Styx` / 9P protocol** | 40 (4.2%) | Pure Limbo library (`appl/lib/styx.b`, `styxservers.b`, `module/styx.m:1-182`) using `pick` ADTs (`Tmsg`/`Rmsg`) and `Sys` FDs/channels. | No Ziran `styx` module yet. | **Unblocked by T1 + T7 + T8.** Port `styx.b` and `styxservers.b` to Ziran `/dis/lib/styx.zx` using tagged unions (`T1`) and `taijios_sys`/`taijios_conc` (T12). |
| **Other `$Builtin`s (`$Math`, `$Crypt`, `$Keyring`, `$IPints`, `$Loader`, `$Srv`)** | `keyring.m`: 81 (8.6%)<br>`math.m`: 27 (2.9%) | C builtins in `taijios/libinterp/{math,crypt,keyring,ipint,loader}.c` and `emu/port/srv.c:152`. | Ziran has `std/math.zi`, `std/constant_time.zi`, `std/zx_module.zi`; no `emu/port/zx.c` bindings for Inferno `$Crypt`/`$Keyring`/`$IPints`. | **Bounded gap.** Use pure Ziran `std/math.zi` for `$Math`; expose `taijios_crypto` host bindings (or pure Ziran crypto packages) for `$Crypt`/`$Keyring`/`$IPints` (T9). |
| **Exception handling (`exception`, `raise`)** | 484 (51.2%) | Structured `exception` / `raise` / `{ ... } exception e { ... }` unwinding (`taijios/limbo/limbo.y:52,71,694-706,885`; `libinterp/raise.c`; `module/exception.m:1-18`). | Explicit `Result(T, E)` and `Option(T)` (`docs/OWNED_VALUES.md:152-158`). `LirOp.Trap` (`cmd/compiler_lowered_ir.zi:33`) is unrecoverable; `Args.SetFailure` sets process exit status (`emu/port/zx.c:1734`). | **High prevalence, two distinct uses.** Most of the 484 occurrences are top-level `raise "fail:..."` exit statuses (already covered by `Args.SetFailure` + return non-zero). For in-process unwinding (parsers, `sh`, `acme`), refactor to `Result(T, E)` with owned error support (T4). |
| **GC & memory model** | 946 (100%) | Reference-counted heap + mark-and-sweep cycle collector in `main`/`heap` pool (`libinterp/heap.c`, `gc.c`). | Manual/owned storage (`Vec`/`Text` lexical drop, frame arena, `LirOp.Resize`), checked pointers in engine & `--checks` (`docs/ZX.md:117-129`, `cmd/compiler_exec.zi:3394`). `emu` runs ZX in a 32–128 MiB `main` pool (`taijios/README.md:18-21,53-60`). | **Different ownership model; performance unmeasured here.** Deterministic drops avoid tracing collection but do not establish lower memory use, latency, or CPU cost. Cyclic/shared graphs need explicit ownership designs and leak/peak-memory tests on each port (T6, T11–T17). |

---

## 2. Classification of the 946 Limbo files

Derived empirically by running:

```sh
env -u DISPLAY -u WAYLAND_DISPLAY python3 scripts/scan_limbo_migration.py
```

The scanner walks `taijiosnet/taijios/appl/**/*.b`, verifies the exact count (`--expect-count 946`), strips Limbo `#` comments and string/character literals, and partitions all 946 files by **dependency wave** and by **difficulty bucket**. Both partitions sum strictly to **946 files (471,331 lines)**.

### 2.1 Top-level directory breakdown (`appl/`, 946 `.b` files)

| Directory | `.b` files | Lines | Dependency wave |
|---|---:|---:|---|
| `cmd` | 348 | 140,679 | W2: CLI Commands & Services |
| `lib` | 220 | 171,331 | W1: Core & Math Libraries |
| `alphabet` | 85 | 12,170 | W3: Shell/Pipeline & Distributed Frameworks |
| `wm` | 82 | 42,038 | W4: Window Manager, Editor & GUI Apps |
| `acme` | 50 | 24,380 | W4: Window Manager, Editor & GUI Apps |
| `spree` | 36 | 14,063 | W5: Games, Demos & Examples |
| `math` | 22 | 5,880 | W1: Core & Math Libraries |
| `svc` | 19 | 6,107 | W2: CLI Commands & Services |
| `charon` | 17 | 23,667 | W4: Window Manager, Editor & GUI Apps |
| `grid` | 17 | 8,285 | W3: Shell/Pipeline & Distributed Frameworks |
| `demo` | 14 | 8,789 | W5: Games, Demos & Examples |
| `collab` | 13 | 3,017 | W3: Shell/Pipeline & Distributed Frameworks |
| `ebook` | 11 | 5,034 | W4: Window Manager, Editor & GUI Apps |
| `examples` | 8 | 5,024 | W5: Games, Demos & Examples |
| `tiny` | 4 | 867 | W2: CLI Commands & Services |
| **Total (`appl/`)** | **946** | **471,331** | **Sum verified = 946** |

*Separate inventory outside `appl/`*: **180** `.m` interface files in `module/`; **80** existing `.zi` files in `appl/`; **26** non-`appl` `.b` files (`os/init/*.b`: 19, `build/*/*.b`: 4, `os/ipaq1110/tstdraw.b`: 1, `module/srvrunt.b`: 1, `emu/port/zxbuiltin.b`: 1).

### 2.2 By dependency order (libraries first, then cmd, then frameworks, then wm/acme, then spree/demos)

| Wave | Directories | Files | Lines | Share |
|---|---|---:|---:|---:|
| **W1: Core & Math Libraries** | `lib`, `math` | 242 | 177,211 | 25.6% |
| **W2: CLI Commands & Services** | `cmd`, `svc`, `tiny` | 371 | 147,653 | 39.2% |
| **W3: Frameworks & Distributed** | `alphabet`, `grid`, `collab` | 115 | 23,472 | 12.2% |
| **W4: WM, Editor & GUI Apps** | `wm`, `acme`, `charon`, `ebook` | 160 | 95,119 | 16.9% |
| **W5: Spree, Demos & Examples** | `spree`, `demo`, `examples` | 58 | 27,876 | 6.1% |
| **Total** | | **946** | **471,331** | **100.0%** |

### 2.3 By difficulty bucket

| Difficulty bucket | Criteria (scanned from source) | Files | Lines | Share |
|---|---|---:|---:|---:|
| **D1: Direct Port (Ready Now)** | No real Draw/Tk, no `spawn`/`chan`/`alt`/`file2chan`/`styx`, no `pick`, no `exception`/`raise`, no namespace/net ops, no crypto/math builtins, ≤ 400 LOC. | 174 | 24,329 | 18.4% |
| **D2: Sys & Stdlib Port** | Sequential (no Draw/Tk, no `spawn`/`chan`/`alt`/`file2chan`/`styx`); uses expanded `$Sys` (`dial`/`mount`/`bind`/`pctl`), crypto/math, `pick`, `exception`, or > 400 LOC. | 282 | 217,192 | 29.8% |
| **D3: Concurrent / 9P Protocol** | Non-GUI, but uses `spawn`, `chan`, `alt`, `file2chan`, or `styx`/`9p`. | 304 | 118,481 | 32.1% |
| **D4: Graphical / Draw / Tk** | Real `Draw` usage (beyond `Draw->Context` in `init`) or `Tk`/`tkclient`/`wmclient`/`prefab`/`freetype`/`dialog`. | 186 | 111,329 | 19.7% |
| **Total** | | **946** | **471,331** | **100.0%** |

### 2.4 Wave × Difficulty matrix

| Wave | D1 Direct | D2 Sys/Stdlib | D3 Concurrent/9P | D4 GUI/Draw/Tk | Row Total |
|---|---:|---:|---:|---:|---:|
| **W1: Libraries (`lib`, `math`)** | 85 | 68 | 52 | 37 | **242** |
| **W2: Cmd & Svc (`cmd`, `svc`, `tiny`)** | 42 | 177 | 124 | 28 | **371** |
| **W3: Frameworks (`alphabet`, `grid`, `collab`)** | 2 | 5 | 92 | 16 | **115** |
| **W4: GUI (`wm`, `acme`, `charon`, `ebook`)** | 34 | 17 | 22 | 87 | **160** |
| **W5: Spree & Demos (`spree`, `demo`, `examples`)** | 11 | 15 | 14 | 18 | **58** |
| **Column Total** | **174** | **282** | **304** | **186** | **946** |

Key takeaway from the scan: **545 files (57.6%)** include `draw.m` *only* for the `Draw->Context` parameter in `init`; only **186 files (19.7%)** are genuinely graphical. **456 files (48.2%, D1+D2)** need zero concurrency or GUI and can be ported as soon as `taijios_sys` + `pick` + `std/` utilities land.

---

## 3. Ordered parallelizable task list

### 3.0 Follow-up correction: first functional milestone and launch boundaries

This subsection supersedes the launch order and claims of disjoint ownership in
the broad T1–T21 backlog below. Those rows describe eventual capabilities; they
are not twenty-one ready assignments. The first new functional milestone is
**one additional streaming command, `strings.zx`, with differential parity**.
Do not require a full concurrency runtime, tagged-union extension, complete Sys
bridge, or GUI migration before this command can ship.

The follow-up local test ran
`env -u DISPLAY -u WAYLAND_DISPLAY python3 scripts/scan_limbo_migration.py --expect-count 946 --json`
through managed `desktop_run`, with exit 0. A separate validation of `scan_repo`
confirmed 946 distinct paths and sums of 946 for the wave, difficulty, and
wave-by-difficulty partitions. Counts remain W1–W5 = 242/371/115/160/58 and
D1–D4 = 174/282/304/186. The live shared tree at this verification had **471,337
Limbo lines and 83 application `.zi` files**; Section 2 records the earlier
471,331-line / 80-`.zi` snapshot. Source edits are ongoing in other lanes.

The scanner uses source-token heuristics and a directory-to-wave map. It does
**not** resolve transitive module dependencies, prove a file portable, distinguish
all identifiers from built-in calls, or verify that an existing `.zi` replaces
its `.b` counterpart. D1 means a small sequential candidate for review, not
unconditional readiness. For example, `cmd/limbo/optim.b` is a three-line compiler
stub, not a useful independent first port. Build a module dependency manifest
before assigning large waves; keep strongly connected module groups together.

Already implemented, not new work: `appl/lib/taijios_names.zi`,
`appl/cmd/echo.zi`, `basename.zi`, and `cleanname.zi`. Their build rules are in
`appl/cmd/mkfile:240-247`; existing Dis/ZX differential cases are in
`build/zx-smoke/run.py:520-553`, including Unicode device roots, suffix removal,
repeated options, empty operands, and usage errors. The baseline test is
`env -u DISPLAY -u WAYLAND_DISPLAY python3 -u build/zx-smoke/run.py Linux/386/bin/emu --stock-zx --cat --utilities`
from the TaijiOS root. It is an existing test command, **not run by this
analysis-only follow-up**. The utility comparisons explicitly normalize Dis
`usage` versus ZX `1`; this is not proof of exact guest failure-status parity.
Do not count source-file existence as passed tests or repeat their completed
implementation.

The next implementation contract is deliberately one worker with sequential
steps. All paths in the following table belong to `taijiosnet/taijios`; the
audit task itself does not implement or launch it.

| Order | Files reserved to that worker | Function | Exact future test | Definition of done |
|---|---|---|---|---|
| **M1a — oracle and bounded reader** | New `appl/lib/taijios_rune_reader.zi`; new `appl/cmd/strings_zx_test.py` | Implement buffered rune reads and byte offsets on existing `taijios_io` Open/Read/Close capabilities. Use `appl/lib/bufio.b` `getc`/`offset` as the semantic oracle. The durable test driver builds fixtures under existing `build/zx-smoke/`, launches only its own non-graphical emulator, and compares explicit Dis and ZX paths. | Define and implement `env -u DISPLAY -u WAYLAND_DISPLAY python3 appl/cmd/strings_zx_test.py --root "$PWD" --emu Linux/386/bin/emu --build --reader` | Fixed-size input buffering works across split UTF-8 sequences, malformed/truncated input, EOF, empty files, and read errors. Byte offsets and replacement behavior agree with Bufio; handles close on every path. This command is a proposed new test interface, not an existing passing test. |
| **M1b — first new command** | New `appl/cmd/strings.zi`; same reader and test driver, same worker after M1a | Port `init`, `stringit`, and `isprint` from `appl/cmd/strings.b:20-87`. Preserve six-rune minimum spans, 70-rune chunks, byte-offset formatting, file labels, stdin, diagnostics, and continuation after an open failure. Do not add compiler/runtime capabilities. | `env -u DISPLAY -u WAYLAND_DISPLAY python3 appl/cmd/strings_zx_test.py --root "$PWD" --emu Linux/386/bin/emu --build` | Exact stdout bytes, stderr bytes, and guest status match `/dis/strings.dis` for 5/6/69/70/71-rune boundaries, final unterminated spans, Unicode, binary data, multiple files, missing files, stdin, and buffers split at every UTF-8 byte. Large input has bounded live memory. Retain the Dis baseline. |
| **M1c — integration** | `appl/cmd/mkfile` `strings.zx` build/install rules; `README.md` command documentation; same worker after parent reserves these shared files | Install the tested bundle at its normal `/dis/strings.zx` path and make ordinary command resolution choose it, following the existing utility rules. No new command aliases or alternate executable paths. | M1b test plus the existing `--stock-zx --cat --utilities` baseline command above; the new driver also asserts `whatis strings` resolves to `/dis/strings.zx` and compares ordinary `strings` with explicit Dis execution | A rebuilt current-tree emulator runs the installed command through its guest namespace; all command parity cases pass. A build alone is not an installation test. Commit/push and installation remain the integrating parent's workflow. |

After M1, order the remaining work by demonstrated dependency:

1. Extend the same sequential I/O/stdlib lane with exact per-file manifests;
   consider `timestamp` only after its `/dev/time` positioned-read semantics
   have a test. Prefer existing capabilities when they reproduce the oracle.
2. Add the smallest missing Sys operation or ownership/module ABI feature
   required by the next selected real program. T1/T4/T5/T6 are conditional
   prerequisites, not a mandatory language redesign before porting commands.
3. Run channel/alt/spawn contracts T3→T8, then a small concurrent/9P library
   and command. Keep the host binding-table integration under one owner.
4. Port measured module dependency groups: libraries first, dependent commands,
   then wm/acme/alphabet/spree as their own prerequisites permit. Directory
   counts alone do not establish ordering between these consumers.
5. Follow the existing separate native/engine lane; compare x86-64, then
   AArch64, then i386 for `emu`, with Alpha physical-host receipts. No numerical
   speedup is established by this audit. Limbo/Dis deletion stays behind every
   parity gate in Section 5.

Observed active workers during this follow-up (managed sessions and bounded
live output, not inferred from dirty files):

| Existing worker | Observed surface / conflict boundary |
|---|---|
| `codex-ziran-owner-words-verbatim-typos-kept-make-zur-3` | Ziran `cmd/compiler_exec.zi`, `tests/lowered_exec_cases.zi`, `bench/rill_compact.zi` and shared compiler build outputs. Route engine/native follow-ups to the existing lane. |
| `codex-rill-owner-words-verbatim-typos-kept-pleas-ru-3` | TaijiOS `appl/cmd/zxrun.b`; Rill `app/taiji_host.zi`; Kryon `src/backend/zx_surface.zi`, its test and Makefile. Do not duplicate its idle-loop/host scheduling changes. |
| `codex-taijios-app-registry-icons-2` | Rill registry/platform integration and TaijiOS emulator build. Shares final Rill packaging and `emu` build products with other lanes. |
| `codex-rill-sweet-titlebar-panel-2` | Rill panel/theme and window chrome; TaijiOS `libtk/wmchrome.c`; Kryon chrome test outputs. Broad GUI migration must not overwrite this lane. |

No inspected active output assigned `strings.zi` or the proposed reader. That
is a snapshot, not an ownership reservation: the parent must check the live
task list when dispatching. Reserve `appl/cmd/mkfile` integration explicitly and
serialize conflicting emulator/compiler/package builds with their existing
owners. Do not parallelize T7/T8/T9 merely because their proposed new C files
differ: each needs shared `emu/port/zx.c` binding/build integration. T11/T12,
T13/T14, and T16/T17 also need exclusive source manifests rather than overlapping
directory globs. A check filter matching zero tests does not verify a feature;
register its test before accepting the result.

The following phases are a capability backlog. Section 3.0 governs dispatch: split broad rows into exact source manifests and reserve shared integration files before assigning independent workers.

### Phase 0 — Ziran language, stdlib & ZX module enablers (when a selected port requires them)

| ID | Scope & owner repo | Files (disjoint) | Function / capability | Exact test | Definition of done |
|---|---|---|---|---|---|
| **T1** | Tagged sum types (`ziran`) | `cmd/zir/zir_check_expr.c`, `cmd/compiler_zir_lower.zi` (`:855-875`, `:5778-5815`), `cmd/compiler_lowered_verify.zi`, `tests/spec/tagged_union.zi` | Support tagged `union` with record/slice/owned payloads in checker, ZIR→LIR lowering, verifier, and ZX engine (removing `"portable unions support scalar fields only"`). | `env -u DISPLAY -u WAYLAND_DISPLAY make check CHECK_FILTER=tagged_union && make check-bootstrap` | Tagged unions with record & `Vec` payloads lower to ZX, execute identically on C, native x86-64, Wasm, and engine with zero leaked heap blocks under `--heap-report`. |
| **T2** | Ergonomic tuples (`ziran`) | `std/tuple.zi`, `tests/spec/std_tuple.zi` | Add generic `Tuple2(A, B)`, `Tuple3(A, B, C)`, `Tuple4(A, B, C, D)` with move/drop support for `Vec`/`Text` fields (avoiding parser churn while multi-return procedures already work). | `env -u DISPLAY -u WAYLAND_DISPLAY make check CHECK_FILTER=std_tuple` | Generic tuples compile and drop owned members cleanly across C, Go, native, and ZX engine. |
| **T3** | Portable ZX concurrency ABI (`ziran`) | `std/zx_conc.zi`, `tests/spec/zx_conc_abi.zi` | Define typed `host_api` capability signatures for `ChanNew`, `ChanSend`, `ChanRecv`, `ChanClose`, `AltSelect`, `Spawn`, `File2Chan` so portable `.zx` bundles declare concurrency cleanly. | `env -u DISPLAY -u WAYLAND_DISPLAY make check CHECK_FILTER=zx_conc_abi` | `zi2zx` emits verified revision-28 `.zx` with exact capability ABI entries for `zx_conc`; mock host harness passes send/recv/alt/spawn round-trips. |
| **T4** | Owned errors in `Result` (`ziran`) | `std/result.zi`, `tests/spec/owned_result.zi` | Verify and support `Result(T, Text)` and `Result(T, OwnedError)` move/drop semantics across all backends (`docs/OWNED_VALUES.md:152-158`), replacing Limbo `exception`/`raise` patterns. | `env -u DISPLAY -u WAYLAND_DISPLAY make check CHECK_FILTER=owned_result` | Returning, branching, and dropping `Result(T, Text)` passes ASan in C and `--heap-report` (0 live blocks) in ZX engine. |
| **T5** | Dynamic ZX module parity & `.m` converter (`ziran`) | `cmd/compiler_exec_link.zi`, `include/ziran_zx_module.h`, `scripts/m2zi.py`, `tests/zx_module_runtime_cases.zi` | Allow runtime-computed logical paths in `std/zx_module.zi` (`LoadByPath` within configured roots) and add `scripts/m2zi.py` to scaffold Ziran module headers from `taijios/module/*.m`. | `env -u DISPLAY -u WAYLAND_DISPLAY make check CHECK_FILTER=zx_module` | Dynamic path loading verifies ABI and rejects path traversal; `m2zi.py` parses all 180 `taijios/module/*.m` files without error. |
| **T6** | Limbo-replacement stdlib primitives (`ziran`) | `std/pool.zi`, `std/rune_text.zi`, `std/list.zi`, `tests/spec/std_pool.zi`, `tests/spec/std_rune_text.zi` | Implement typed generational slot pool (`Pool(T)` for `ref`/`cyclic` graphs), rune-indexed UTF-8 helpers (`RuneAt`, `RuneSlice`, `RuneCount`, `BytesToString`), and optional singly-linked list helper. | `env -u DISPLAY -u WAYLAND_DISPLAY make check CHECK_FILTER=std_pool && make check CHECK_FILTER=std_rune_text` | Zero leaks under `--heap-report`, full UTF-8 edge-case coverage (1–4 byte runes, invalid bytes → `U+FFFD`), parity across C, native, and engine. |

### Phase 1 — TaijiOS `emu` host-capability parity (depends on T3, T5; parallelizable across T7, T8, T9, T10)

| ID | Scope & owner repo | Files (disjoint) | Function / capability | Exact test | Definition of done |
|---|---|---|---|---|---|
| **T7** | Full `$Sys` host capability (`taijios`) | `emu/port/zx_sys.c`, `appl/cmd/taijios_sys.zi`, `build/zx-smoke/sys_parity.zi` | Implement `dirread`, `pipe`, `dup`, `chdir`, `sleep`, `pctl`, `bind`, `mount`, `unmount`, `export`, `dial`, `announce`, `listen`, `fversion`, `stream`, `pread`, `pwrite`, `werrstr`, `iounit` via kernel `k*` APIs, wired into `emu/port/zx.c:zxbindings`. | `Linux/386/bin/emu -r "$PWD" zxrun /dis/test/sys_parity.zx` | All 19 new `$Sys` operations match `$Sys` Dis behavior on files, pipes, binds, and loopback dial/listen within 32 MiB `main` pool. |
| **T8** | Concurrency & `file2chan` host bridge (`taijios`) | `emu/port/zx_conc.c`, `appl/cmd/taijios_conc.zi`, `build/zx-smoke/conc_parity.zi` | Bridge `std/zx_conc.zi` to Inferno `kproc`/scheduler, `Channel` (`cnew`/`csend`/`crecv`), `alt`, and `Sys->file2chan` (releasing/reacquiring interp lock cleanly around blocking ops). | `Linux/386/bin/emu -r "$PWD" zxrun /dis/test/conc_parity.zx` | 100-worker spawn + unbuffered/buffered chan + 4-way `alt` + `file2chan` synthetic file test passes without deadlock or memory leak. |
| **T9** | Crypto, Keyring & IPints bridge (`taijios`) | `emu/port/zx_crypto.c`, `appl/lib/taijios_crypto.zi`, `build/zx-smoke/crypto_parity.zi` | Expose `libsec`/`libmp` digest, cipher, HMAC, big-int, and auth primitives needed by the 81 `keyring.m`/`crypt.m`/`security.m` files. | `Linux/386/bin/emu -r "$PWD" zxrun /dis/test/crypto_parity.zx` | SHA-1/SHA-256/AES/RC4/MD5/IPint test vectors match Dis `$Crypt`/`$IPints` byte-for-byte. |
| **T10** | Direct ZX boot entry in `emu` (`taijios`) | `emu/port/zx_boot.c`, `emu/port/main.c` | Allow `emu` to launch a `.zx` init bundle directly (without `zxrun.dis` or `emuinit.dis`), decoupling ZX startup from `libinterp` and `zxbuiltin.b`. | `Linux/386/bin/emu -r "$PWD" -zx /dis/cmd/hello.zx` | `emu -zx` boots and runs a `.zx` program end-to-end with zero Dis modules loaded (`dis_loaded == 0`). |

### Phase 2 — Porting the 946 `appl/**/*.b` files in dependency order (parallelizable by directory/module once prerequisites land)

| ID | Wave & bucket | Dependencies | Files (disjoint) | Exact test | Definition of done |
|---|---|---|---|---|---|
| **T11** | W1A: Core & Math libs (153 files: W1 D1+D2) | T1, T2, T4, T6, T7 | `appl/lib/*.zi` (sequential libs: `bufio`, `arg`, `string`, `daytime`, `readdir`, `workdir`, `rand`, `sets`, `crc`, `encoding`, `cfg`, `attrdb`, `csv`, etc.) + `appl/math/*.zi` | Ziran unit test suite per library + `zi2zx --library` install to `/dis/lib/*.zx` | All 153 sequential library `.b` files have passing `.zx` replacements with zero heap leaks under `--heap-report`. |
| **T12** | W1B: Styx/9P & Concurrent libs (52 files: W1 D3) | T1, T7, T8, T9, T11 | `appl/lib/styx.zi`, `styxservers.zi`, `dial.zi`, `factotum.zi`, `auth9.zi`, `fslib.zi`, `sh.zi` + concurrent libs | 9P client/server loopback test + `styxservers` synthetic FS test in `emu` | All 52 concurrent/9P libraries pass parity tests against existing Dis clients and servers. |
| **T13** | W2A: Sequential CLI & Svc (219 files: W2 D1+D2) | T7, T11 | `appl/cmd/*.zi` (non-concurrent CLI: `cp`, `rm`, `mv`, `grep`, `sed`, `awk`, `wc`, `sort`, `diff`, `tar`, `gzip`, `sha1sum`, etc.), `appl/svc/*.zi`, `appl/tiny/*.zi` | Differential CLI test harness comparing stdout/stderr/exit-status of `.zx` vs `.dis` on fixtures | All 219 commands match Dis output and exit codes; `appl/cmd/mkfile` builds and installs `.zx` targets. |
| **T14** | W2B: Concurrent & 9P CLI/Svc (124 files: W2 D3) | T8, T12, T13 | `appl/cmd/sh/*.zi`, `mount.zi`, `export.zi`, `listen.zi`, `styxlisten.zi`, `ramfile.zi`, `appl/cmd/ip/*.zi`, `appl/cmd/ndb/*.zi`, `appl/svc/*.zi` | Interactive shell pipeline, `mount`/`export`, and `ramfile` integration tests in `emu` | `sh.zx` runs full TaijiOS boot scripts, pipes, redirections, and namespace mounts identically to `sh.dis`. |
| **T15** | W3: Frameworks (115 files: `alphabet`, `grid`, `collab`) | T5, T8, T12, T14 | `appl/alphabet/**/*.zi`, `appl/grid/**/*.zi`, `appl/collab/**/*.zi` | Alphabet pipeline tests + grid/collab loopback tests in `emu` | All 115 files ported, dynamic module loading via `zx_module` verified. |
| **T16** | W4: GUI, WM, Acme & Readers (160 files + 37 W1 D4 + 28 W2 D4 = 225 GUI files total) | T8, T12, T14 | `appl/wm/*.zi` (extending existing Kryon Rill, Stopwatch, Clock, Desktop Panel, `window_chrome`, `winplace`, `wm_policy`), `appl/acme/*.zi`, `appl/charon/*.zi`, `appl/ebook/*.zi`, `appl/lib/wmclient.zi` | Headless Xvfb `emu -g 800x600` smoke & screenshot/event tests (pattern of `appl/wm/clock_zx_test.py`) | Full graphical desktop boots with `wm.zx` + `rill.zx`, opens terminal/editor/apps, handles resize/input/clipboard within 128 MiB `main` pool (32 MiB for minimal toolbar). |
| **T17** | W5: Spree, Demos, Examples & `os/init` (58 `appl/` + 19 `os/init/*.b`) | T10, T16 | `appl/spree/**/*.zi`, `appl/demo/**/*.zi`, `appl/examples/**/*.zi`, `os/init/*.zi` | Spree engine smoke test + `os/init` boot test | Zero remaining production `.b` files needed for hosted or native TaijiOS boot. |

### Phase 3 — Efficiency program & native tiers (parallelizable with Phase 1/2)

| ID | Scope & owner repo | Files (disjoint) | Function / capability | Exact test | Definition of done |
|---|---|---|---|---|---|
| **T18** | Tri-way benchmark suite: Ziran vs C vs Limbo (`ziran`) | `bench/limbo_compare.py`, `bench/limbo_compare/` (`.c`, `.zi`, `.b`), `bench/bench.zi` (register driver) | Run identical workloads in C (`-O2`), Ziran native ELF, Ziran ZX engine, and Limbo/Dis (interpreted + Dis JIT) with output checksum verification, 11+ interleaved samples, symmetric 1.5×IQR outlier removal, raw samples retained in `bench/results/limbo-compare-alpha-*.json`. | `build/bin/ziran run --target=c bench/bench.zi -- --local limbo-compare --check` (local validation); official measurement on **Alpha physical host only** | Registered in `bench/bench.zi`, `bench/bench.zi -- check` passes, Alpha physical receipt committed with raw samples, excluded indices, and SHA-256 hashes. |
| **T19** | x86-64 native C-parity & beat-C (`ziran`) | `cmd/compiler_opt.zi`, `cmd/compiler_opt_rotate.zi`, `cmd/compiler_regalloc.zi`, `cmd/compiler_machine_select.zi`, `cmd/compiler_x86_64.zi`, `cmd/compiler_machine_elf.zi` | Complete remaining `plans/2026-11-15-native-speed.md` & `c_teacher` items: loop-invariant code motion (LICM), induction-variable simplification, loop rotation with 16/32B loop-head alignment, regalloc lifetime holes (eliminating retry fallback), reusable native allocator, line-buffered console, SIMD vectorization. | `env -u DISPLAY -u WAYLAND_DISPLAY make check && make check-bootstrap` + Alpha `limbo-compare` & `standard-ab` receipts | Ziran x86-64 native beats Limbo/Dis JIT by ≥ 5× on all compute kernels, reaches ≤ 1.10× C geomean, and beats C (`< 1.0× C`) on bounds-proved/inlined/SIMD kernels. |
| **T20** | AArch64 native optimization parity (`ziran`) | `cmd/compiler_aarch64.zi`, `tests/aarch64_encoder_cases.zi` | Bring `compiler_aarch64.zi` to parity with x86-64 aggregate coalescing, direct proven-pointer addressing, immediate/flag-branch selection, and reusable heap veneer. | `env -u DISPLAY -u WAYLAND_DISPLAY make check CHECK_FILTER=aarch64` + ARM64 physical host/device parity | All `zx_lowered_parity` cases execute identically on AArch64 native ELF; no `assembly.gap` regressions. |
| *(ref)* | **i386 native tier & ZX engine hot paths** | *(owned by separate helper lane)* | Direct i386 native lowering for `emu/Linux` (32-bit x86) + ZX instruction-engine hot-path dispatch/memory optimizations. | Helper's own i386/engine parity & Alpha receipts | Referenced here; once landed, `emu/port/zx.c` switches from `BUNDLE_BACKEND_ENGINE` to native i386 execution for `.zx` bundles, matching/beating Dis's `libinterp/comp-386.c` JIT. |

### Phase 4 — Decommissioning (only after Section 5 gates pass)

| ID | Scope & owner repo | Files | Function / capability | Exact test | Definition of done |
|---|---|---|---|---|---|
| **T21** | Delete Limbo compiler & Dis interpreter (`taijios`) | `limbo/`, `libinterp/`, `emu/port/dis.c`, `emu/port/discall.c`, `emu/port/zxbuiltin.b`, `appl/**/*.b`, `module/*.m`, `dis/**/*.dis`, `emu/Linux/mkfile` | Remove Limbo compiler, Dis VM/JIT, `.b`/`.m`/`.dis` trees, and `limbo -a/-t` rules from `mkfile`s. | Clean full build of `emu` + boot desktop + full CLI/9P/WM test suite with zero `.b`/`.dis` files present | `git ls-files '*.b' '*.m' '*.dis'` is empty; `emu` builds without `libinterp.a` or `limbo` on `PATH` and passes all parity & benchmark gates. |

---

## 4. Efficiency program: Ziran vs. C vs. Limbo

### 4.1 Measurement rules (binding from `AGENTS.md`)

1. **Physical Alpha only for benchmarks of record**: All benchmark compilation and measurement of record run directly on Alpha's physical host (`systemd-detect-virt == none`; virtualized execution is rejected). Never fall back locally to Omega for before/after comparisons. Record exact host, CPU, load averages, compiler revision, and artifact SHA-256 digests.
2. **Environment hygiene**: Run via `bench/bench.zi` with `ZIRAN_LEARN=off`, `DISPLAY`/`WAYLAND_DISPLAY` unset, and `ZIRAN_PROFILE` unset.
3. **Statistical discipline**: Minimum 11 interleaved samples per target; exclude anomalies using the **symmetric 1.5×IQR rule with inclusive interpolated quartiles** across all targets and platforms. Retain **all raw samples** and **excluded sample indices** in tracked `bench/results/` JSON receipts; never silently discard measurements.

### 4.2 Tri-way benchmark matrix (`bench/limbo_compare.py`)

Every workload is implemented identically in **C (`-O2`)**, **Ziran native ELF**, **Ziran ZX engine**, **Limbo/Dis (interpreted `emu -c0`)**, and **Limbo/Dis (JIT `emu -c1`)**, with verified output checksums:

- **Compute & memory kernels** (extending `bench/wasm_tier.py` & `bench/c_teacher.zi`):
  1. `sieve` (tight byte-array loop, bounds-check elision)
  2. `records` (64-particle struct-array update, aggregate coalescing)
  3. `hash` (FNV-1a over byte slice, LICM + register pressure)
  4. `vectors` (dynamic `Vec` / slice growth & sum, allocator reuse)
  5. `calls_runtime` (recursive Fibonacci with **runtime-supplied** `n` via argv/stdin so neither C nor Ziran constant-folds it, fixing the folded `Calls` caveated in `plans/2026-11-15-native-speed.md:6,153-161`)
  6. `printing` (100-line formatted output, line-buffered syscall coalescing)
  7. `json_scan` & `utf8_runes` (real-world text/UTF-8 scanning)
- **TaijiOS OS & concurrency workloads**:
  8. `styx_pack_unpack` (9P `Tmsg`/`Rmsg` encode/decode throughput)
  9. `chan_pingpong` & `alt_fanin` (channel & `alt` throughput vs. Dis `alt.c`)
  10. `pipe_filter` (`cat`/`wc` 10 MiB stream through guest namespace)

### 4.3 Target efficiency milestones

- **Milestone E1 (Beat Limbo/Dis everywhere)**: Ziran ZX engine (with helper's hot-path work) is ≥ 1× Dis interpreter; Ziran native ELF (x86-64, AArch64, and helper's i386 tier) is **≥ 3–5× faster than Dis JIT** on all compute and 9P kernels, and within ≤ 1.5× of Dis on channel/alt IPC.
- **Milestone E2 (C-level parity)**: Ziran x86-64 native reaches **≤ 1.10× C (`-O2`) geomean** across the 8 compute/text kernels via T19 (`c_teacher`-guided LICM, loop rotation + alignment, regalloc lifetime holes, reusable allocator, line-buffered console).
- **Milestone E3 (Beat C where Ziran has structural advantages)**: Achieve **< 1.00× C (`-O2`)** on workloads where Ziran's whole-program linked `.zx` boundary, non-aliasing slice proofs, static monomorphization, and explicit SIMD (`std/simd.zi`) outperform separate-compilation C (specifically `json_scan`, `utf8_runes`, `records`, and `styx_pack_unpack`).

### 4.4 Ordered native tier roadmap

1. **x86-64 (`cmd/compiler_x86_64.zi`) — Tier 1 (Primary)**: Drives Alpha benchmarks of record, `c_teacher` learning loop, and 64-bit Linux/Windows hosts. All shared LIR/regalloc optimizations (`compiler_opt.zi`, `compiler_opt_rotate.zi`, `compiler_regalloc.zi`, `compiler_machine_aggregate.zi`) land here first and automatically benefit lower tiers.
2. **AArch64 (`cmd/compiler_aarch64.zi`) — Tier 2**: Already present in `cmd/compiler_aarch64.zi`; next after x86-64 so ARM64 Linux servers, macOS/ARM64, and Android devices share the same native speed and parity.
3. **i386 (`emu/Linux` 32-bit) — Tier 3 (*referenced, owned by separate helper*)**: Critical for TaijiOS `emu/Linux` (`Linux/386`, `emu/Linux/mkfile:1-68`) to replace Dis's `libinterp/comp-386.c` JIT with native `.zx` execution instead of `BUNDLE_BACKEND_ENGINE`. Once the helper lands i386 native lowering, `emu/port/zx.c` enables native execution by default on `Linux/386` (with `--backend engine` / `BUNDLE_BACKEND_ENGINE` retained only for testing/fallback verification). *(Note: ARMv7 already has `cmd/compiler_armv7.zi` for 32-bit ARM targets.)*

---

## 5. Removal criteria for Limbo compiler and Dis interpreter

Neither `taijios/limbo/` nor `taijios/libinterp/` (`emu/port/dis.c`, `discall.c`, `zxbuiltin.b`) may be deleted until **all seven** quantitative gates below pass simultaneously:

1. **100% `appl/` coverage**: Every one of the **946** `appl/**/*.b` files is either replaced by a verified `.zx` equivalent passing its functional parity suite or explicitly retired in a reviewed retirement manifest (e.g., obsolete hardware demos), with **0** remaining `.dis` dependencies in `/dis/`.
2. **Self-booting ZX `emu`**: `emu` boots directly into a `.zx` init, runs `sh.zx`, mounts/exports 9P namespaces (`styx.zx`), and launches the full graphical desktop (`wm.zx` + `rill.zx` + terminal + editor + apps) with `libinterp` disabled at compile time (`#ifndef NO_DIS`).
3. **Zero `limbo` build-tool dependency**: `emu/Linux/mkfile` no longer invokes `limbo -a` or `limbo -t` (`zxbuiltin.b`, `zxrunt.h`, `zxmod.h` removed); `mk` builds the entire TaijiOS tree on a clean host with no `limbo` binary on `PATH`.
4. **Memory & stability parity**: Hosted ZX desktop + Task Manager + shell runs within the 32 MiB (`wm/toolbar`) and 128 MiB (`rill.zx`) `main` pool budgets (`taijios/README.md:18-21,104,173`), with zero leaked heap blocks (`--heap-report`) on unit suites and no step-budget exhaustion under interactive stress.
5. **Performance gate on physical Alpha**: Tracked Alpha receipt (`bench/results/limbo-compare-alpha-final.json`, 1.5×IQR, raw samples retained) demonstrates Ziran native ≥ Limbo/Dis JIT on 100% of compute/9P/CLI workloads, ≤ 1.10× C geomean on x86-64, and i386 `emu` native `.zx` ≥ Dis `comp-386.c` JIT.
6. **Ziran repository health**: `env -u DISPLAY -u WAYLAND_DISPLAY make check` and `make check-bootstrap` pass in `ziranlang/ziran` with all new language/stdlib/optimizer tests retained.
7. **Clean removal verification**: After deleting `limbo/`, `libinterp/`, `emu/port/dis.c`, `discall.c`, `zxbuiltin.b`, `appl/**/*.b`, `module/*.m`, and `dis/**/*.dis`, `git ls-files '*.b' '*.m' '*.dis'` returns 0 files and the full TaijiOS build + headless Xvfb desktop & CLI test suites pass green.
