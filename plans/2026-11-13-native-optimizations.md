# Native hello-world optimizations and `.lirb` vs `.zx`

Owner, 2026-11-13: "yes do all optmizations run a subataoks for it".
Source of the findings: the native ELF built from `examples/hello.zi` (objdump/nm) and its
lowered text. Baseline: 1,377 bytes of `.text`, 224 instructions,
a 4,840-byte file, and `main` is 1,342 bytes.

## Status

Items 1–3 and 5–7 and the one-pipeline unification were committed together on
2026-11-13 after `make check` (276/278 local tests; `zstd_go.sh` needs Go 1.25+ and
Go 1.24.4 is installed; `native_build_parallel.sh` timed out under full-suite load and
passes alone) and `make check-bootstrap`.

| # | Item | Owner of the work | State |
|---|------|-------------------|-------|
| 1 | Pointer lowering: replace per-element compare chains with base + index and one bounds check | native worker (`compiler_lowered_x64.zi`) | implemented; local regression tests pass |
| 2 | Symbol table: omit inlined services, retain distinct concrete functions with positive bounded extents | native worker | implemented; local no-undefined-symbol regression passes |
| 3 | LIR folds: element.address with constant 0, extract of a fresh aggregate, convert of a constant, dead values | LIR optimizer worker (`compiler_opt.zi`) | implemented; local optimizer tests pass |
| 4 | Native inspection on the M2 pipeline (machine IR + regalloc + encoder) instead of stack-slot `compiler_lowered_x64` | after hello-world lands | planned |
| 5 | Instruction selection: short `mov`/`xor` for small constants, no jump to the next block, no empty 0x50 frame | native worker | implemented; local byte/layout tests pass |
| 6 | ELF layout: `.rodata` right after `.text` with congruent offsets (no 4 KB padding); drop the empty `.data`; keep symbols | native worker | implemented; local ELF/layout tests pass |
| 7 | Smaller LIR: prune unreferenced types, intern duplicate origins | LIR optimizer worker | implemented; local optimizer tests pass |
| 8 | Smaller ZIR: drop per-node source text where the span already locates it | Claude task, after the integration commit | planned |

Item 8, owner 2026-11-13: "why in zir is print hello world repeatetd twice that dhosen seme to make senes".
`cmd/compiler_serial.zi` writes a `text` string for every statement (`Statement`, `item.text`) and
every expression (`Expression`, `Source(codec, item.text)`). It also writes each node's `span`.
Identical strings are interned, but each node's spelling is different, so hello's 223-byte ZIR
holds three copies of the literal:
- `print("Hello, World!\n");`: the statement
- `print("Hello, World!\n")`: the call expression
- `"Hello, World!\n"`: the literal argument
- `print`: the callee name
Nested expressions repeat their parents' text, so the size grows with nesting depth. Fix:
- Find every consumer of `ZirStmt.text` and `ZirExpr.text`.
- Keep only the text that carries meaning: literal values, names, and `#asm` variants.
- Rebuild the rest from the span and the source file when a diagnostic needs it.
- Bump the ZIR version and keep the C reader (`cmd/zir/zir_serial.c`) in step.

The small-hello target is achieved without claiming item 4: `main` contains the
inlined `write_all(1, &string.0, 14)` loop plus return, `.text` is 114 bytes,
and the file is under 1.5 KB. It runs with exact stdout and exit 0, uses
`write` syscall 1 and `exit_group` syscall 231, and has no interpreter,
dynamic segment, libc or CRT. Inlined services are omitted from ELF symbols;
`_start`, `_zir_trap` and `main` remain distinct bounded functions.

## Approved: one native pipeline with ZX as a stage

Owner approved, 2026-11-13: "so do that plesae". Implemented as item (d) by the native worker,
after items 1, 2, 5 and 6. `.lirb` is removed: no `--emit exec` or `--input exec`, and
`--emit lir` stays as a text dump. The VS Code Native view stage update follows once the
CLI commands land.

The CLI unification is implemented and locally tested. Source ELF creation
encodes and reads the same verified optimized ZX payload as saved-ZX input;
both routes produce byte-identical ELF files and maps. The portable interpreter
uses that same executable payload. Source LIR inspection can still show the
unoptimized form, while source `--opt --emit opt-report` reports transformations.
The current executable ZX payload strips source origins, so native maps retain
byte coordinates/instruction IDs but do not yet recover source origins.

The default `ziran`/`zi2zx` semantic bundle routes (revisions 26/27) and
semantic-ZIR Wasm lowering have not been cut over to revision-28 executable ZX;
these remain explicit gaps, not completed parts of the unification. The VS Code
Native stage migration is a subsequent change in `ziran-vscode`, not item 4.

Owner, 2026-11-13: "i'm ondireng if zx and native basicalyl use the same piplen if we just
unife it under native and show zx as a step in teh pielpn while ofcuesr being able to do zx
as teh portabel lower prat for all pltaforms or am i misisng something".

One pipeline: `.zi -> checked ZIR -> lowered LIR -> verify -> LIR optimizations -> .zx`
(portable, linked, verified). Then either the portable interpreter on any platform, or
native lowering (`.zx -> machine code -> ELF`). Points to keep:
- `.zx` stays a first-class output (ship it and run it anywhere), not only an internal step.
- Native lowers only from the verified `.zx` bytes, so one `.zx` always gives the same ELF.
- LIR-level optimizations run before `.zx` is written, so the interpreter and native both
  benefit; machine-level ones run after.
- Host capabilities are declared once in `.zx`. The interpreter binds them at run time;
  native binds them at lowering time to target veneers. Missing ones are hard errors.
- The VS Code Native view shows the stages ZIR -> LIR -> ZX -> asm -> ELF. C and Go stay
  separate generated routes, never "native".
- Check whether the Wasm route (`zir_zx_wasm`) already lowers from `.zx`; if it does, it is
  another lowering of the same pipeline.

## Earlier question: should `.lirb` just be `.zx`?

Before unification, ZX revision 28 stored the same deterministic lowered payload
as `.lirb`, inside a container with a 128-byte manifest, identity, entry and
capability bindings. The current payload is LIR schema 4 / internal codec 2;
the executable manifest's contract ID remains schema 3 (see `docs/ZX.md`).
`zi2asm` lowers `.zx` straight to ELF (`hello.zx -> hello-zx.elf`).

Historical proposal (approved and implemented above):
- Make `.zx` the only on-disk portable program format. Native lowers from `.zx`, and the
  interpreter runs `.zx`.
- Remove `.lirb` as a separate artifact (`--emit exec`/`--input exec`). Keep
  `--emit lir` as a text dump for inspection only.
- The VS Code Native view shows `.zx -> final.elf`.
