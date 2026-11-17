# Self-hosting Ziran: retiring the hand-written compiler C

Analysis and migration plan, 2026-11-14. This plan follows the row 7 decision in
[2026-11-13-native-optimizations.md](2026-11-13-native-optimizations.md): owner,
verbatim, "and wyh arenty ou writing al of ziran in ziran so we can bootstarp it".
It changes no source. It maps every C file under the compiler directories to a
Ziran destination and defines session-sized steps. The eventual `bootstrap/`
state is generated C from the previous compiler plus the small OS/runtime C that
generated programs require; there is no second hand-written compiler.

Tool-name cutover note, 2026-11-14: the current-state measurements and cited
old paths below remain dated evidence. For implementation, translate
`cmd/zir-asm` to `cmd/zi2zx` for `zir_asm_lower`/`zx_exec_run`, and to
`cmd/zx2native` for the saved-ZX native driver; translate `cmd/zir-wasm` to
`cmd/zx2wasm`, `cmd/zir-c` to `cmd/zi2c`, `cmd/zir-go` to `cmd/zi2go`, and
`cmd/zir-zx` to the consolidated `cmd/zi2zx`. The deletion steps delete those
new live C paths, not already-removed compatibility directories.

## Why staged bootstrap is mandatory

The current compiler is already partly self-hosted: `zi2c` compiles
`cmd/compiler_*.zi` into generated C, but that generated C is linked into a C
driver (`cmd/zir`) and C tools. Replacing all remaining C at once would require
the new compiler to exist before it can compile itself. The stable way is to add
one Ziran module, use the existing compiler to generate its C, make the existing
tools call that module, prove behavior unchanged, and delete the replaced C.
After the final step, `zi2c` is a thin launcher over generated modules; a later
compiler rebuilds those modules from its own source and `update-bootstrap`
freezes that generation as the next generation's bootstrap.

## Source measurements

The owner-supplied measurements are the tracked-`HEAD` totals including `.c`
and `.h` files. `cmd/*.zi` is clean and identical in the working tree. Six
in-scope files have uncommitted Wasm/Asyncify work, so current plain sums differ:

| Scope | Tracked HEAD `.c` + `.h` lines | Current `.c` + `.h` |
|---|---:|---:|
| `cmd/*.zi` | 32,092 | 32,092 |
| `cmd/zir` | 68,387 | 68,429 |
| `cmd/zir-asm` | 6,481 | 6,542 |
| `cmd/zir-c` | 4,832 | 4,832 |
| `cmd/zir-wasm` | 4,586 | 4,806 |
| `cmd/zir-go` | 4,266 | 4,266 |
| `cmd/zir-zx` | 279 | 279 |

`git status --short -- cmd/zir-asm cmd/zir-wasm cmd/zir` shows the dirty files:
`cmd/zir-asm/zir_asm_lower.c`, `cmd/zir-wasm/zx_wasm_lower.c`,
`cmd/zir-wasm/zx_wasm_lower.h`, `cmd/zir/zir_zx_wasm.c`,
`cmd/zir/zir_zx_wasm_platform.h`, and `cmd/zir/zir_zx_wasm_web.c`. This plan
cites current working-tree lines; the first implementation session must refresh
them immediately before editing.

## Current state and boundary

The ordinary build generates compiler modules into `build`; `BOOTSTRAP=1` uses
the checked-in generated C. `Makefile:70-102` selects those source roots,
`Makefile:137` adds their include paths, and `Makefile:615-629` define
`check-bootstrap` and `update-bootstrap`. The bootstrap seed is built by the
hand-written frontend and C generator at `Makefile:482-509`; ordinary generated
module rules are at `Makefile:247-314`. The assembly tool links generated
Ziran modules in `assembly.mk:1-20`, while `assembly.mk:41-49` still links the
two hand-written lowering objects.

The checked-in bootstrap today contains only the existing generated modules,
including `bootstrap/compiler_ir/compiler_ir.c:1` and
`bootstrap/compiler_parse/compiler_parse.c:1`; it does not yet contain the
lowering, checker, emitter, serializers, host runtime, or tools.

## Ziran code that already replaces C routines

These generated modules are live, but the listed C wrappers and callers keep the
C ABI/storage boundary:

- Scanner: `cmd/compiler_scan.zi` supplies token recognition;
  `cmd/zir/zir_token.c:55`, `:62`, and `:69` call `compiler_scan_*`. Step S15
  absorbs the wrapper.
- Text/source spelling: `cmd/compiler_text.zi` supplies identifiers, spacing,
  trimming and escapes; `cmd/zir/zir_text.c:30`, `:39`, `:53`, `:76`, `:82`, and
  `:99` wrap it. Step S15 removes the wrapper.
- Source structure: `cmd/compiler_source.zi` supplies statement classification,
  control-block splitting and literal-safe scanning. Calls remain in
  `cmd/zir/zir_parse.c:159`, `:183`, `:189`, `:317`, `:482`, `:635`, `:687`,
  `:712`, and `:730`; step S13 makes them direct Ziran calls.
- Declarations: `cmd/compiler_declaration.zi` supplies procedure, import,
  foreign, result and rewrite grammar. Remaining C orchestration calls it at
  `cmd/zir/zir_parse_declaration.c:30`, `:89`, `:101`, `:134`, `:194`, `:521`,
  `:554`, `:583`, `:598`, `:638`, `:685`, `:731`, `:809`, `:869`, `:893`, and
  `:917`; step S14 absorbs the orchestration.
- Statements: `cmd/compiler_statement.zi` supplies loop/control parsing. C
  callers remain at `cmd/zir/zir_cleanup.c:53`, `:608`, `:641`, `:659`, `:960`,
  `:1227`, and `cmd/zir/zir_parse_declaration.c:750`, `:781`, `:787`, `:797`;
  steps S13/S14/S15 delete those C callers.
- Expressions: `cmd/compiler_expression.zi` and `cmd/compiler_parse.zi` parse
  expressions; `cmd/zir/zir_expr.c:275` and `:353` still invoke them from C.
  Step S14 replaces this adapter.
- Types/enums: `cmd/compiler_type.zi` and `cmd/compiler_enum.zi` implement the
  rules; C caching/adapters remain at `cmd/zir/zir.c:137`, `:145`, `:176`,
  `:193`; `cmd/zir/zir_type.c:278`, `:363`, `:395`, `:404`; and
  `cmd/zir/zir_enum.c:92`. Step S15 removes the adapters.
- Link rules: `cmd/compiler_link.zi` implements import linking and privacy;
  `cmd/zir/zir_check_link.c:874` and `cmd/zir/zir_check_program.c:520`, `:737`
  still call it. Step S12 absorbs the surrounding checks.
- ZX container rules: `cmd/compiler_bundle.zi` is used by
  `cmd/zir/zir_assets.c:84`, `cmd/zir/zir_host_exec.c:218`, `:350`, `:360`,
  `:432`, `cmd/zir-asm/main.c:684`, `cmd/zir-zx/main.c:2`, and
  `cmd/zir-wasm/zx_wasm_lower.c:4605`. Steps S2 and S16 remove the remaining C
  consumers.
- Services bridge: `cmd/compiler_services.zi:37-82` declares compiler services
  as C foreign procedures; `cmd/zir/zir_services.c:11`, `:20`, `:28`, `:33`,
  and `:42` implements string conversion and IR mutation. This is temporary
  glue. S15/S17 replace each hook; the final runtime keeps only unavoidable OS
  entry points, not compiler policy.

## Complete replacement map

### `cmd/zir-asm`

| Current C | Evidence / role | Ziran target | Delete |
|---|---|---|---|
| `zir_asm_lower.h` | Public ABI at `:9-17` | `cmd/compiler_zir_lower.zi` exports the same ABI | S1 |
| `zir_asm_lower.c` | `assembly_type_in:335`, `assembly_emit:631`, `assembly_address:1419`, `AssemblyLower:4918`, `AssemblyView:5039`, `AssemblyFree:5045`, `AssemblyHostInterface:5207`, `AssemblyPrintLir:5301` | `compiler_zir_lower` ZIR -> LIR/ZX lowering | S1 |
| `zx_exec_run.h` | API at `:18`, `:26` | `compiler_zx_run` | S16 |
| `zx_exec_run.c` | `ZxExecRunProgram:15`, `portable_entry:60`, `ZxExecBuild:83`; calls lowering at `:93`, `:95`, `:116` | `compiler_zx_run` | S16 |
| `main.c` | CLI and preflight `assembly_machine_gap:232`, read `:328`, report `:407`, `main:586`; lowering calls `:707`, `:741`, `:887` | `cmd/zi2zx.zi` driver | S17 |

### `cmd/zir-wasm`

| Current C | Evidence / role | Ziran target | Delete |
|---|---|---|---|
| `zx_wasm_lower.h` | Public ABI at `:108-131` | `cmd/compiler_zx_wasm.zi` | S2 |
| `zx_wasm_lower.c` | instruction lowering `lower_instruction:2598`, function body `lower_function:2980`, module `lower_module:3973`, public `ZxWasmLowerExecutable:4591`, `ZxWasmLowerProgram:4573`, `ZxWasmLowerProgramSuspendable:4582`, globals `:4521`, `:4551`, free `:4345` | `compiler_zx_wasm` | S2 |

### `cmd/zir-c`

| Current C | Evidence / role | Ziran target | Delete |
|---|---|---|---|
| `zir_c_lower.h` | `c_lower` declaration `:35` | `compiler_c_emit` + `compiler_c_backend` | S8 |
| `zir_c_lower.c` | ZIR-to-C backend; emits body helpers at `:2028`, `:2031`, bodies `:2615`, `:2621`, `:2636`; module contexts `c_lower_in_context:2773`, symbols `c_build_syms:2811`, `c_lower:2878` | `compiler_c_backend` | S8 |
| `zir_c_build.h` | lock/workers `:5`, `:9` | `compiler_backend_process` | S9 |
| `zir_c_build.c` | worker limit `c_worker_limit:53`, lock `c_build_lock:110`, compile/link orchestration from `:236` onward | `compiler_backend_process` | S9 |
| `zir_c_cache.h` | cache API `:6-15` | `compiler_backend_cache` | S9 |
| `zir_c_cache.c` | fingerprinting `:83-220`, open `c_cache_open:561`, hit `c_cache_hit:724`, record `c_cache_record:782`, close `:819` | `compiler_backend_cache` | S9 |
| `zir_c_process.h` | worker result ABI | internal to `compiler_backend_process` | S9 |
| `main.c` | entry wrapper `write_entry_main:56`, SIMD finish `:141`, lower workers `lower_programs:186`, `main:246`; calls `c_lower` at `:195`, `:216`, `:395` | `cmd/zi2c.zi` driver | S17 |

### `cmd/zir-go`

| Current C | Evidence / role | Ziran target | Delete |
|---|---|---|---|
| `zir_go_lower.h` | `go_lower` declaration `:14` | `compiler_go_backend` | S10 |
| `zir_go_lower.c` | generic body use `:1412`, `:1414`; buffers `:2116`; `go_lower:2127` and public `:2748`; opens assembly at `:2607` | `compiler_go_backend` | S10 |
| `zir_go_asm.h` | assembly bridge API `:7-11` | `compiler_go_asm` | S10 |
| `zir_go_asm.c` | signatures and direct assembly `GoAssemblyOpen:121`, `GoAssemblyFunction:242`, `GoAssemblyBlock:287`, C helper `:344`, CPU emission `:512`, `:520` | `compiler_go_asm` | S10 |
| `zir_go_amd64.h` | x86-64 assembly API `:7-14` | `compiler_go_asm_x64` | S10 |
| `zir_go_amd64.c` | operand plan `:104`, instruction translation `:349`, public `GoAmd64Prepare:478`, `GoAmd64Write:524`, `GoAmd64Cpu:563`, `GoAmd64Close:590` | `compiler_go_asm_x64` | S10 |
| `main.c` | usage `:20`, `main:31` | `cmd/zi2go.zi` driver | S17 |

### `cmd/zir-zx`

| Current C | Evidence / role | Ziran target | Delete |
|---|---|---|---|
| `main.c` | portable ZX CLI importing `compiler_bundle` at `:2` | `cmd/zi2zx.zi` or retained `zx` run command | S17 |

### `cmd/zir`: frontend, check and IR construction

| Current C | Evidence / role | Ziran target | Delete |
|---|---|---|---|
| `zir.c` / `zir.h` | target/host queries `CompilerHostOS:31`, target parsing `:87`, capability check `:151`; program/module construction `ProgramNew:1632`, `ModuleAddImport:1752` | `compiler_frontend_ir` + `compiler_target` | S15 |
| `zir_asm.c` / `zir_asm.h` | inline assembly parse/check and C/Go assembly emission; `CheckAssemblyBlock:998`, `CheckAssembly:1180`, `EmitAssembly:1826`, block helpers `:2094` | `compiler_inline_asm` | S17 |
| `zir_assets.c` / `zir_link.h` | asset collection `ZxAssetsCollect:113`; bundle rule call `:84` | `compiler_zx_assets` | S16 |
| `zir_body.c` / `.h` | portable-body eligibility `CanEmitBody:213`, public `:258` | `compiler_portable_body` | S16 |
| `zir_borrow.c` / `.h` | ownership/lifetime analysis; public `CheckSliceLifetimes:2001` | `compiler_borrow` | S12 |
| `zir_check.c` / `.h` | checker entry/lookup; first routines `:10`, file-private `:85` | `compiler_check` | S11/S12 |
| `zir_check_expr.c` | expression checking from `:26` | `compiler_check_expr` | S11 |
| `zir_check_function.c` | function checking `:202` | `compiler_check_function` | S12 |
| `zir_check_internal.h` | checker private ABI | private Ziran module state | S12 |
| `zir_check_link.c` | link/generic checks; foreign slices `:7`, generated-link call `:874` | `compiler_check_link` | S12 |
| `zir_check_program.c` | whole-program checker `CheckPrograms_with_buffers:76`, lint `LintPrograms:985` | `compiler_check_program` | S12 |
| `zir_check_simd.c` | SIMD check from `:14` | `compiler_check_simd` | S12 |
| `zir_check_statement.c` | statement checking from `:8` | `compiler_check_statement` | S11 |
| `zir_check_text.c` | fresh-text ownership from `:86` | `compiler_check_expr` | S11 |
| `zir_check_value.c` | lexical/value lookup from `:5` | `compiler_check_value` | S11 |
| `zir_cleanup.c` / `.h` | source normalization/lowering prep; loop rules at `:608`, `:641`, `:659`; public API `:6-8` | `compiler_ir_cleanup` | S14 |
| `zir_command.h` | option parsing and program merge `ReadCommandOptions:80`, `CommandMergePrograms:113` | `compiler_cli` | S17 |
| `zir_context.c` / `.h` | compiler contexts/storage `CompilerContextSelect:19`, storage `:31` | `compiler_context`; tiny context hooks may remain in runtime | S15 |
| `zir_diagnostic.c` / `.h` | diagnostics declarations `:26-37`, UTF-8 handling `:15` | `compiler_diagnostic` | S15 |
| `zir_document.c` / `.h` | source documents/spans/digests; record `:19`, offset `:25` | `compiler_document` | S15 |
| `zir_edit.c` / `.h` | editor service from `:35` onward | `compiler_edit` (already has `cmd/edit.zi` driver) | S17 |
| `zir_emit.c` / `.h` | generic emission setup and public emitters | `compiler_c_emit` | S4 |
| `zir_emit_call.c` | C/Go call emission from `:6` | `compiler_c_emit_call` | S6 |
| `zir_emit_expr.c` | C/Go expression emission from `:13` | `compiler_c_emit_expr` | S6 |
| `zir_emit_gpu.c` | OpenGL ES compute emission from `:50` | `compiler_gpu_emit` | S7 |
| `zir_emit_internal.h` | shared emitter state | private records in emitter modules | S7 |
| `zir_emit_range.c` | range facts/native ranges from `:147` | `compiler_c_emit_range` | S7 |
| `zir_emit_simd.c` | SIMD C emission `EmitUseSimdTier:14` | `compiler_c_emit_simd` | S7 |
| `zir_emit_simd_go.c` | Go SIMD emission from `:735`, generated file marker `:1188` | `compiler_go_simd` | S7 |
| `zir_emit_statement.c` | statement/body emission; `EmitBody:2068`, public scratch wrapper `:2115` | `compiler_c_emit_statement` | S5 |
| `zir_emit_value.c` | output/format controls from `:8` | `compiler_c_emit_value` | S5 |
| `zir_enum.c` | enum snapshot cache; generated arithmetic call `:92`; public `EnumMemberValue:113` | `compiler_frontend_ir` cache | S15 |
| `zir_exec_console.h` | portable failure text | `compiler_zx_run` | S16 |
| `zir_expr.c` / `.h` | expression IR construction/parser bridge `parse_expr:16`, generated calls `:275`, `:353` | `compiler_frontend_ir` + parser | S14 |
| `zir_files.c` / `.h` | filesystem/path policy `PathIsAbsolute:19`, status `:7` | `compiler_files` | S15 |
| `zir_host.c` | embedding API `BundleOpen:46`, instances `:335`, run `:348`, state/components `:653`, `:868` | `compiler_host_api` | S16 |
| `zir_host_exec.c` / `.h` | executable ZX engine facade; bundle open `ExecBundleOpen:421`, host instances `:1399`; no-engine stubs `:1396` onward | `compiler_zx_exec` | S16 |
| `zir_law.c` / `.h` | laws and parallel-region checks `CheckLawGates:877`, `CheckParallelRegions:1856` | `compiler_law_runtime` | S15 |
| `zir_link.c` | portable link/assets; `split_binding_name:27` | `compiler_zx_link` | S16 |
| `zir_load.c` / `.h` | tool discovery/loading; toolchain probe `:46`, `ProgramLoad` use `:309` | `compiler_load` | S16 |
| `zir_memory.c` / `.h` | compiler memory arenas `CompilerMemoryBegin:18` | Ziran allocator; only hooks remain in bootstrap runtime | S18 |
| `zir_native.c` / `.h` | collision-safe native names `PrepareNativeNames:11` | `compiler_native_names` | S17 |
| `zir_overlay.c` / `.h` | source overlay `SourceOverlayBegin:9` | `compiler_overlay` | S15 |
| `zir_packages.c` / `.h` | package manifest resolution from `:48` | `compiler_package_resolve` | S16 |
| `zir_parse.c` / `.h` | module/global parsing; source helpers `:159`, `:635`; globals `:246` | `compiler_frontend_parse` | S14 |
| `zir_parse_condition.c` | compile-time conditions `:11`, `:600` | `compiler_frontend_parse_condition` | S13 |
| `zir_parse_declaration.c` | declarations/imports/foreigns from `:26`, `:518`, `:575` | `compiler_frontend_parse_declaration` | S14 |
| `zir_parse_discover.c` | discovery/type headers from `:18`, enum backing `:1386`, C spelling call `:1400` | `compiler_frontend_parse_discover` | S13 |
| `zir_parse_eval.c` | compile-time evaluation `:6` | `compiler_frontend_parse_eval` | S14 |
| `zir_parse_internal.h` | parser private ABI | private Ziran records | S14 |
| `zir_parse_source.c` | file/import pipeline `parse_source:155`, `parse_file_with_imports:2176`, `parse_file:2270` | `compiler_frontend_parse_source` | S13 |
| `zir_parse_typed.c` | typed expression compilation from `:9`, cache `:1027` | `compiler_frontend_parse_typed` | S14 |
| `zir_profile.c` / `.h` | counters/RSS/time from `:35` | `compiler_profile`; OS callbacks may remain | S17 |
| `zir_proof.h` | proof ABI | covered by `compiler_proof`/`compiler_proof_kernel` | S15 |
| `zir_scalar.h` | scalar width/mask helpers `:8-41` | `compiler_scalar` | S4 |
| `zir_scratch.h` | recoverable scratch helper | Ziran allocator/exceptions; no separate module | S15 |
| `zir_semantic.c` / `.h` | semantic IDs `SemanticId:7`, function IDs `:22` | `compiler_semantic` | S15 |
| `zir_serial.c` / `.h` | saved ZIR codec and canonical checks `ProgramWrite:1777`, read `:1820`, load `:1905`, public read `:2125` | recreate `compiler_zir_serial` (the prior unused copy was removed, as recorded in `CHANGELOG.md:23`) | S16 |
| `zir_services.c` | generated-module service hooks `terminated:11`, keep `:28`, diagnose `:33`, add import `:42` | native service calls; final OS hooks only | S15/S18 |
| `zir_sha256.c` / `.h` | portable and hardware hashes from `:36`, `:74`, `:138` | `compiler_sha256` | S15 |
| `zir_simd.c` / `.h` | SIMD metadata/signatures from `:95` | `compiler_simd_metadata` | S7 |
| `zir_simd_go_bodies.h` | Go SIMD generated body text | data in `compiler_go_simd` | S7 |
| `zir_storage.c` | interned source strings `SourceFile_persistent:62`, span path `:114` | `compiler_source_store` | S15 |
| `zir_stream.h` | memory/text streams from `:19`, `:149` | standard Ziran slices/files | S15 |
| `zir_text.c` / `.h` | wrappers around generated text rules from `:18`, `:30` | direct `compiler_text` | S15 |
| `zir_thread.h` | thread/context scope API `:28-54` | `compiler_context` | S15 |
| `zir_token.c` / `.h` | lexer state over generated scanner `LexerInit:22`, `LexerNext:53` | `compiler_frontend_token` | S15 |
| `zir_type.c` | cached type queries from `:12`, generated calls `:278`, `:363`, `:395` | `compiler_frontend_ir` | S15 |
| `zir_zx_wasm.c` / `.h` | Wasm runtime provider/platform `ZxWasmInstallBundledPlatform:50`, host memory `:83`; lowering calls `:320`, `:443`, `:474` | `compiler_zx_wasm_host` | S16 |
| `zir_zx_wasm_platform.h` | provider ABI `:47`, set/active `:54`, `:55` | `compiler_zx_wasm_host` | S16 |
| `zir_zx_wasm_web.c` | Emscripten/JS bridge `EM_JS` entries from `:35`, `:85`, `:93` | `compiler_zx_wasm_web` behind provider ABI | S16 |

Every file listed above is deleted by its step or by the final bootstrap step.
No in-scope C file remains after S18.

## Target design

1. **One IR ABI, owned by Ziran.** `cmd/compiler_ir.zi` already defines the C
  compatible ZIR records (`cmd/compiler_ir.zi:10-260`), including functions and
   proofs. New frontend modules manipulate those records directly. Generated
   exported functions use stable C names during migration so remaining C drivers
   can link without wrappers. Each deleted C header is replaced by the generated
   module header, not a permanent shim.
2. **Modules, not folders of globals.** Each new module owns its private state
   through explicit context records. Recoverable scratch and arenas become normal
   Ziran allocation with explicit limits. Diagnostics call the common
   `compiler_diagnostic` interface; no module writes stderr directly.
3. **Tools become Ziran drivers.** `cmd/zi2c.zi`, `cmd/zi2go.zi`, `cmd/zi2zx.zi`
   and the existing `ziran` package driver contain option parsing, process
   orchestration and output decisions. Build/cache/process support lives in
   reusable backend modules. Item 10 in the 2026-11-13 plan names the eventual
   folder/tool layout; this plan does not rename files early.
4. **Generated-code runtime boundary.** Generated compiler C still needs a small
   bootstrap runtime: process startup, the C ABI for OS calls unavailable in the
   seed, exact-width allocation, panics/recovery, and the existing generated
   `include/zir_vec.h` and SIMD headers. Policy and parsing may not move there.
   The current large runtime header blob is generated from source by
   `Makefile:455-466`; it remains generated data, not compiler C.
5. **Bootstrap model.** Each accepted module is added to `BOOTSTRAP_MODULES`,
   generated under `bootstrap/<module>/`, and linked by Makefile. `check-bootstrap`
   remains byte-for-byte generation equality. `update-bootstrap` refreshes only
   after local tests. Once the final `zi2c` driver is Ziran, its generated C is
   the previous compiler; regenerating all compiler modules with that binary and
   byte-comparing produces the next frozen bootstrap.
6. **Compatibility policy.** No flag preserves the C implementation. Each step
   removes the files it replaces in the same session. Diagnostics and generated
   bytes must remain identical except where a named, reviewed format bug is fixed.

### Driver paths

The tool-name cutover is complete. Translate the drivers at `cmd/zi2zir`,
`cmd/zi2c`, `cmd/zi2go`, `cmd/zi2zx`, `cmd/zx2wasm`, and `cmd/zx2native`.
The old paths in the map above are dated evidence; apply each named function
change at its permanent path.

## Ordered implementation steps

Each step is one implementation session. Every proof uses the managed build
queue and removes desktop display variables inside the queued process. Refresh
line references before editing because S1/S2 currently have uncommitted edits.
No step commits before its implementation and proof are complete.

### S1 — ZIR -> ZX lowering

- Add `cmd/compiler_zir_lower.zi`; port the public API and all `assembly_*`
  routines in `cmd/zir-asm/zir_asm_lower.c`, especially `AssemblyLower:4918`,
  `AssemblyView:5039`, `AssemblyFree:5045`, `AssemblyHostInterface:5207`, and
  `AssemblyPrintLir:5301`.
- Change `cmd/zir-asm/main.c:707`, `:709`, `:741`, `:887`, `:916`, `:942` and
  `cmd/zir-asm/zx_exec_run.c:83`, `:93`, `:95`, `:116`, `:143` to the generated
  ABI.
- Change `assembly.mk:1-20` to generate the module; change `assembly.mk:41-49`
  and the `Makefile:477-480` `zi2zx` link to its object.
- Add `bootstrap/compiler_zir_lower`; add the module to `BOOTSTRAP_MODULES` at
  `Makefile:611` and to the bootstrap dependencies at `Makefile:485-506`.
- Delete `cmd/zir-asm/zir_asm_lower.c` and `cmd/zir-asm/zir_asm_lower.h`.
- Proof: `build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY make -j2 check CHECK_FILTER=zx_lowered_parity`.
- Done 2026-11-15: `cmd/compiler_zir_lower.zi` ports the full lowering
  (AssemblyLower/View/Free/HostInterface/PrintLir and every assembly_* helper);
  `cmd/zi2zx` and `cmd/zx2native` call the generated ABI; `assembly.mk` and the
  `zi2zx` link use the generated module; `bootstrap/compiler_zir_lower` and
  `bootstrap/compiler_lowered_ir` are checked in and verified by
  `check-bootstrap`; the old `cmd/zi2zx/zir_asm_lower.c/.h` are deleted.

### S2 — ZX -> WebAssembly lowering

- Add `cmd/compiler_zx_wasm.zi`; port `lower_module:3973`, public program/exec
  entry points `:4573`, `:4582`, `:4591`, global accessors `:4521`, `:4551`, and
  `ZxWasmImageFree:4345`.
- Change callers in `cmd/zir-asm/main.c:787`, `:791` and
  `cmd/zir/zir_zx_wasm.c:320`, `:321`, `:443`, `:474`, `:492` to the generated
  ABI.
- Change `Makefile:160-174` so `HOST_EXEC_SOURCES` no longer names the C
  lowering and the library links the generated object. Update bootstrap lists as
  in S1.
- Delete `cmd/zir-wasm/zx_wasm_lower.c` and `cmd/zir-wasm/zx_wasm_lower.h`.
- Proof: `build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY make -j2 check CHECK_FILTER=wasm`.
- Done 2026-11-17: `cmd/compiler_zx_wasm*.zi` (types, fn, plan, code, body,
  helpers and the module API) is part of the engine that `cmd/compiler_runtime.zi`
  names; `zir_zx_wasm.c` and `cmd/zx2wasm/main.c` call it under the engine's
  names; `cmd/zx2wasm/zx_wasm_lower.c` and `.h` are deleted. Every executable
  in the comparison set lowers to the same bytes, plain and suspendable.

### S4 — shared emitter core

- Add `cmd/compiler_c_emit.zi` and `cmd/compiler_scalar.zi`; port
  `cmd/zir/zir_emit.c`, `cmd/zir/zir_emit.h`, `cmd/zir/zir_scalar.h`, and
  `cmd/zir/zir_emit_internal.h` state/layout helpers. Preserve the generic
  `EmitBody` contract declared at `cmd/zir/zir_emit.h:182`.
- Change `cmd/zir-c/zir_c_lower.c` and `cmd/zir-go/zir_go_lower.c` to the
  generated generic ABI; add generated objects to both tool links at
  `Makefile:468-474`.
- Delete `cmd/zir/zir_emit.c`, `cmd/zir/zir_emit.h`, `cmd/zir/zir_scalar.h`,
  and `cmd/zir/zir_emit_internal.h`.
- Proof: `build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY make -j2 check CHECK_FILTER=c99_target`.

### S5 — C value and statement emitters

- Add `cmd/compiler_c_emit_value.zi` and `cmd/compiler_c_emit_statement.zi`;
  port `cmd/zir/zir_emit_value.c` and `cmd/zir/zir_emit_statement.c`, including
  `EmitBody_with_buffers:2073` and public wrapper `EmitBody:2115`.
- Change `compiler_c_emit` dispatch and remaining generated backend callers to
  these modules.
- Delete both source C files listed in S5.
- Proof: `build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY make -j2 check CHECK_FILTER=compile_values`.

### S6 — C expression and call emitters

- Add `cmd/compiler_c_emit_expr.zi` and `cmd/compiler_c_emit_call.zi`; port
  `cmd/zir/zir_emit_expr.c` and `cmd/zir/zir_emit_call.c`, including global
  wrapper naming at `zir_emit_call.c:6` and body eligibility uses at `:450`,
  `:778`.
- Update generated emitter dispatch only; no public tool ABI changes.
- Delete `cmd/zir/zir_emit_expr.c` and `cmd/zir/zir_emit_call.c`.
- Proof: `build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY make -j2 check CHECK_FILTER=c_warnings`.

### S7 — SIMD, range and GPU emission

- Add `cmd/compiler_c_emit_simd.zi`, `cmd/compiler_c_emit_range.zi`,
  `cmd/compiler_gpu_emit.zi`, `cmd/compiler_go_simd.zi`, and
  `cmd/compiler_simd_metadata.zi`; port `cmd/zir/zir_emit_simd.c`,
  `zir_emit_range.c`, `zir_emit_gpu.c`, `zir_emit_simd_go.c`,
  `zir_simd.c`, `zir_simd.h`, and `zir_simd_go_bodies.h`.
- Change generated C/Go emitters and `compiler_runtime_emit` to these modules;
  update `Makefile:468-474` and bootstrap lists.
- Delete all seven listed C files/headers.
- Proof: `build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY make -j2 check CHECK_FILTER=simd`.

### S8 — C backend lowering

- Add `cmd/compiler_c_backend.zi`; port `cmd/zir-c/zir_c_lower.c` and its
  public `c_lower:2878`, `c_lower_in_context:2773`, `c_build_syms:2811`, and all
  file/header/body writers.
- Change `cmd/zir-c/main.c:56`, `:186`, `:195`, `:216`, `:395`, `:417`, `:441`
  to call the generated backend. Keep the C build/cache for one more step.
- Delete `cmd/zir-c/zir_c_lower.c` and `cmd/zir-c/zir_c_lower.h`.
- Proof: `build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY make -j2 check CHECK_FILTER=c99_target`.

### S9 — C backend build/cache and workers

- Add `cmd/compiler_backend_process.zi` and `cmd/compiler_backend_cache.zi`;
  port `cmd/zir-c/zir_c_build.c` (`c_worker_limit:53`, `c_build_lock:110`,
  compile/link orchestration from `:236`) and `zir_c_cache.c` (`c_cache_open:561`,
  `c_cache_hit:724`, `c_cache_record:782`, `c_cache_close:819`).
- Rewrite `cmd/zir-c/main.c` calls to the generated modules; add them to the
  `Makefile:468-471` link and bootstrap lists.
- Delete `cmd/zir-c/zir_c_build.c`, `zir_c_build.h`, `zir_c_cache.c`,
  `zir_c_cache.h`, and `zir_c_process.h`.
- Proof: `build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY make -j2 check CHECK_FILTER=native_cache`.

### S10 — Go backend

- Add `cmd/compiler_go_backend.zi`, `cmd/compiler_go_asm.zi`, and
  `cmd/compiler_go_asm_x64.zi`; port all files currently in `cmd/zir-go`, using
  the public entry points `go_lower:2127`, `go_lower:2748`,
  `GoAssemblyOpen:121`, `GoAssemblyFunction:242`, `GoAssemblyBlock:287`,
  `GoAmd64Prepare:478`, `GoAmd64Write:524`, `GoAmd64Cpu:563`, and
  `GoAmd64Close:590`.
- Change `cmd/zir-go/main.c:31` and Makefile tool link `Makefile:473-474`; add
  bootstrap modules.
- Delete the entire `cmd/zir-go` folder.
- Proof: `build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY make -j2 check CHECK_FILTER=go_fields`.

### S11 — checker value, statement and expression passes

- Add `cmd/compiler_check_value.zi`, `cmd/compiler_check_statement.zi`,
  `cmd/compiler_check_expr.zi`, and `cmd/compiler_check.zi`; port
  `cmd/zir/zir_check_value.c`, `zir_check_statement.c`, `zir_check_expr.c`,
  `zir_check_text.c`, and `zir_check.c`. Preserve the `Checker` record ABI
  declared by `cmd/zir/zir_check_internal.h` while C callers remain.
- Change S12/S13 callers to the generated checker interfaces.
- Delete `zir_check.c`, `zir_check.h`, `zir_check_value.c`,
  `zir_check_statement.c`, `zir_check_expr.c`, and `zir_check_text.c`.
- Proof: `build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY make -j2 check CHECK_FILTER=safe_inference`.

### S12 — checker functions, programs, links, borrow and SIMD

- Add `cmd/compiler_check_function.zi`, `cmd/compiler_check_program.zi`,
  `cmd/compiler_check_link.zi`, `cmd/compiler_check_simd.zi`, and
  `cmd/compiler_borrow.zi`; port `CheckPrograms_with_buffers:76`,
  `LintPrograms:985`, generated-link calls `:520`, `:737`,
  `compiler_link` bridge `zir_check_link.c:874`, SIMD checks from
  `zir_check_simd.c:14`, and `CheckSliceLifetimes:2001`.
- Change parser/tool callers to the new whole-program API; remove the merged
  C object special cases in `Makefile:320-365` as their last consumers vanish.
- Delete `zir_check_function.c`, `zir_check_program.c`, `zir_check_link.c`,
  `zir_check_simd.c`, `zir_check_internal.h`, `zir_borrow.c`, and
  `zir_borrow.h`.
- Proof: `build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY make -j2 check CHECK_FILTER=parser_recovery`.

### S13 — source/discovery parser

- Add `cmd/compiler_frontend_parse_source.zi`,
  `cmd/compiler_frontend_parse_condition.zi`, and
  `cmd/compiler_frontend_parse_discover.zi`; port `zir_parse_source.c`
  (`parse_source:155`, `parse_file_with_imports:2176`, `parse_file:2270`),
  `zir_parse_condition.c`, and `zir_parse_discover.c`. Call generated source,
  text and type modules directly.
- Change S14 consumers to the new file/program API.
- Delete `zir_parse_source.c`, `zir_parse_condition.c`, and
  `zir_parse_discover.c`.
- Proof: `build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY make -j2 check CHECK_FILTER=parser`.

### S14 — declaration/typed parser and IR cleanup

- Add `cmd/compiler_frontend_parse_declaration.zi`,
  `cmd/compiler_frontend_parse_eval.zi`,
  `cmd/compiler_frontend_parse_typed.zi`, `cmd/compiler_frontend_parse.zi`,
  `cmd/compiler_ir_cleanup.zi`, and `cmd/compiler_expr_ir.zi`; port
  `zir_parse_declaration.c`, `zir_parse_eval.c`, `zir_parse_typed.c`,
  `zir_parse.c`, `zir_cleanup.c`, `zir_expr.c`, and their headers/internal ABI.
- Change tools to the frontend parse API; remove generated parse/check/emit
  merge rules only when their C parts disappear (defer to S15/S16 if the checker
  or emitter object remains).
- Delete all parser files listed in S14 plus `zir_cleanup.c/.h`, `zir_expr.c/.h`,
  and `zir_parse_internal.h`.
- Proof: `build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY make -j2 check CHECK_FILTER=compiler_declaration`.

### S15 — frontend IR, context and support runtime

- Add `cmd/compiler_frontend_ir.zi`, `cmd/compiler_frontend_token.zi`,
  `cmd/compiler_context.zi`, `cmd/compiler_document.zi`,
  `cmd/compiler_diagnostic.zi`, `cmd/compiler_files.zi`,
  `cmd/compiler_source_store.zi`, `cmd/compiler_overlay.zi`,
  `cmd/compiler_semantic.zi`, `cmd/compiler_sha256.zi`,
  `cmd/compiler_law_runtime.zi`, and `cmd/compiler_target_bridge.zi`; port all
  files in the map rows for S15, replacing wrappers around scan/text/type/enum
  with direct imports.
- Replace `compiler_services` C hooks one by one; retain only `exit`,
  file/process callbacks and diagnostic sink in the small runtime.
- Delete every C/header file listed in the S15 rows, including `zir.c`,
  `zir.h`, `zir_text`, `zir_token`, `zir_type`, `zir_enum`, `zir_storage`,
  `zir_services.c`, and context/diagnostic/files support.
- Proof: `build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY make -j2 check CHECK_FILTER=compiler_type`.

### S16 — portable ZX, host API and persistence

- Add `cmd/compiler_zx_link.zi`, `cmd/compiler_zx_assets.zi`,
  `cmd/compiler_zir_serial.zi`, `cmd/compiler_zx_run.zi`,
  `cmd/compiler_zx_exec.zi`, `cmd/compiler_host_api.zi`,
  `cmd/compiler_zx_wasm_host.zi`, `cmd/compiler_zx_wasm_web.zi`,
  `cmd/compiler_load.zi`, and `cmd/compiler_package_resolve.zi`; port the S16
  map rows, preserving the public embedding functions in `zir_host.c:45-998`
  and executable APIs in `zir_host_exec.c:214-1455`.
- Change Kryon-facing headers only if the C ABI names remain identical; update
  `Makefile:144-182`, `Makefile:633-645`, and tests that link `libziran.a` to
  generated objects. Add modules to bootstrap.
- Delete `zir_link.c`, `zir_assets.c`, `zir_serial.c`, `zir_body.c`,
  `zir_host.c`, `zir_host_exec.c`, `zir_load.c`, `zir_packages.c`,
  `zir_zx_wasm.c`, `zir_zx_wasm_web.c`, their headers/platform headers,
  `zir_exec_console.h`, and `cmd/zir-asm/zx_exec_run.c/.h`.
- Proof: `build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY make -j2 check CHECK_FILTER=bundle`.

### S17 — inline assembly, editor, native names and tool drivers

- Add `cmd/compiler_inline_asm.zi`, `cmd/compiler_native_names.zi`,
  `cmd/compiler_profile.zi`, `cmd/compiler_cli.zi`, `cmd/compiler_edit.zi`,
  `cmd/zi2c.zi`, `cmd/zi2go.zi`, and `cmd/zi2zx.zi`; port
  `zir_asm.c`, `zir_native.c`, `zir_profile.c`, `zir_command.h`, `zir_edit.c`,
  and the remaining `main.c` drivers. Keep the existing tool names and flags
  until the separate naming-cutover item lands.
- Change Makefile tool targets and editor links to Ziran drivers. The `ziran`
  package driver is already Ziran; no change beyond imports.
- Delete `cmd/zir/zir_asm.c/.h`, `zir_native.c/.h`, `zir_profile.c/.h`,
  `zir_command.h`, `zir_edit.c/.h`, and all `main.c` files in `cmd/zir-asm`,
  `cmd/zir-c`, `cmd/zir-go`, and `cmd/zir-zx`.
- Proof: `build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY make -j2 check CHECK_FILTER=tool_diagnostics`.

### S18 — bootstrap convergence

- Add `bootstrap/manifest.txt`, listing every generated compiler root in
  dependency order and the expected generated `.c/.h` paths. Add
  `scripts/bootstrap_fixed_point.py` with `read_manifest`, `generate_stage`,
  `compile_stage`, `compare_stage`, `validate_runtime_boundary`, and
  `atomic_update`.
- Add `bootstrap/runtime/README.md`,
  `bootstrap/runtime/ziran_bootstrap_runtime.c/.h`, and
  `bootstrap/runtime/ziran_bootstrap_entry.c`. The only permitted functions are
  process/argument startup, checked allocation/reallocation/free, panic/recovery
  transfer, file/environment/process/time/thread/dynamic-library callbacks, and
  the call into the generated `zi2c` root. No parser, type, checker, lowering,
  emission, cache policy, package policy, or diagnostic formatting is allowed.
  Replace any remaining compiler-policy callback with a generated Ziran module;
  if one remains, this step is not done.
- Rewrite `Makefile:46-182`, `:455-509`, and `:607-629`: every compiler object
  comes from a checked-in generated module generated by the previous compiler;
  ordinary and bootstrap builds differ only in optimization and the seed binary.
  `check-bootstrap` calls the fixed-point script in `check` mode;
  `update-bootstrap` calls it in `update` mode and refuses to copy unless its
  own invocation has built both stages and run `make check` successfully.
- Delete any file left under `cmd/zir`, `cmd/zir-asm`, `cmd/zir-c`,
  `cmd/zir-wasm`, `cmd/zir-go`, and `cmd/zir-zx`; their directories disappear.
- Proof: `build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY make check check-bootstrap JOBS=2 CHECK_JOBS=2`.

## Dependency order

1. S1 and S2 are first because both consume stable ZX/LIR and feed the one
   portable format.
2. S3-S9 establish C emission because `zir_c_lower.c:2028`, `:2031`, `:2615`,
   `:2621`, and `:2636` directly use the shared emitter; the C backend cannot be
   ported before its emitter.
3. S10 reuses the generic emitter boundary for Go.
4. S11-S14 replace checker and parser orchestration. This is after codegen
   because generated C modules can safely consume the stable IR while the old C
   frontend still constructs it.
5. S15 removes the IR/context glue only when no hand-written compiler consumer
   needs the old C structs.
6. S16 moves the public embedding and portable runtime last among libraries
   because external hosts and tests depend on its ABI.
7. S17 removes drivers and inline assembly only when every backend exists.
8. S18 freezes the generated compiler and shrinks bootstrap to generated C plus
   the documented runtime.

This intentionally refines the broad 2026-11-13 row order: C emission precedes
the checker because the C generator directly imports shared emitter routines.

## Test-file ownership by session

The command in each session is the acceptance command; these are the exact
test files changed to make that command prove the port rather than merely
compile it. Preserve C-vs-Ziran differential fixtures until the relevant C file
is deleted, then keep their golden hashes/bytes as regression fixtures.

| Step | Test files to add/change |
|---|---|
| S1 | Change `tests/zx_lowered_parity.sh` and `tests/zx_lowered_parity_cases.zi`; add C/Ziran lowering byte and diagnostic parity cases. |
| S2 | Change `tests/zx_wasm.sh` and `tests/spec/zx_wasm_test.zi`; compare module bytes, globals, suspension and executable entry output. |
| S3 | Change `tests/compiler_runtime_windows.sh`, `tests/native_runtime_headers.sh`, and `tests/simd.sh`; compare every emitted runtime/header variant byte-for-byte. |
| S4 | Change `tests/c99_target.sh` and `tests/c_warnings.sh`; add shared emitter layout and escaping cases. |
| S5 | Change `tests/compile_values.sh`; add every value/statement shape and diagnostic path. |
| S6 | Change `tests/c_warnings.sh`; add call ABI, nested expression, evaluation-order and ownership cases. |
| S7 | Change `tests/simd.sh`, `tests/simd_go.sh`, `tests/simd_native.sh`, and `tests/simd_target.sh`; retain C/Go/GPU text goldens. |
| S8 | Change `tests/c99_target.sh`, `tests/compile_targets.sh`, and `tests/compile_values.sh`; compare complete generated trees. |
| S9 | Change `tests/native_build_cache.sh`, `tests/native_build_cache_cases.zi`, `tests/native_build_parallel.sh`, and `tests/native_build_parallel_cases.zi`; cover hits, corrupt entries, locks and worker failure. |
| S10 | Change `tests/go_fields.sh`, `tests/simd_go.sh`, and Go assembly fixtures under `tests/fixtures/go_assembly/`; compare Go and assembly bytes/text. |
| S11 | Change `tests/safe_inference.sh` and `tests/safe_inference.zi`; add checker result/diagnostic/source-span parity. |
| S12 | Change `tests/parser_recovery.sh`, `tests/parser_recovery_test.c`, `tests/borrow_import_graph.sh`, and assembly/SIMD checker cases; prove whole-program/link/borrow behavior. |
| S13 | Change `tests/compiler_source.sh`, `tests/compiler_source_test.c`, and `tests/parser_recovery.sh`; cover imports, conditions and discovery. |
| S14 | Change `tests/compiler_declaration.sh`, `tests/compiler_declaration_test.c`, `tests/compiler_expression.sh`, and `tests/compiler_expression_test.c`; cover declaration/typed/eval/cleanup parity. |
| S15 | Change `tests/compiler_scan.sh`, `tests/compiler_text.sh`, `tests/compiler_enum.sh`, `tests/compiler_type.sh`, and their existing C harnesses; add service-free lifetime/context assertions. |
| S16 | Change `tests/executable_bundle.sh`, `tests/bundle_assets.sh`, `tests/bundle_runtime_provider.sh`, `tests/host_capability.sh`, and `tests/host_slices.sh`; preserve embedding ABI and serialized-byte parity. |
| S17 | Change `tests/tool_diagnostics.sh`, `tests/tool_diagnostics_cases.zi`, `tests/assembly.sh`, and editor/profile CLI tests; execute every Ziran driver. |
| S18 | Add `tests/bootstrap_fixed_point.sh`; test a clean match, stage-2 mismatch, stale generated file, forbidden runtime function, interrupted update and atomic recovery. Register it in `tests/check_plan.zi`. |

## Bootstrap mechanics evolution

- During S1-S17, `update-bootstrap` adds each new module and keeps all existing
  generated modules. A module is added only after its deletion step passes.
- `BOOTSTRAP_MODULES` moves from a list of helper modules to a complete compiler
  manifest. The list lives beside `Makefile:611`; generated dependency order is
  validated by a clean `BOOTSTRAP=1` build.
- `check-bootstrap` continues byte comparison (`Makefile:615-621`) during the
  migration. In S18 it performs four explicit gates: build stage 0 from the
  checked-in generated tree plus the reviewed runtime; generate stage 1 into
  `build/bootstrap-stage1`; compile stage 1 and generate stage 2 into
  `build/bootstrap-stage2`; compare stage 1 to stage 2 and stage 1 to the
  checked-in tree. It also rejects unlisted files and compiler policy in the
  runtime. Any difference proves non-self-hosting.
- `update-bootstrap` (`Makefile:623-629`) runs the same stage-0/stage-1/stage-2
  fixed point and the full local `make check` inside one target invocation. It
  stages the complete replacement under `build/bootstrap-update`, deletes
  generated files absent from the manifest, fsyncs files/directories, and only
  then atomically replaces the checked-in generated tree. Failure leaves the
  previous bootstrap byte-for-byte intact; there is no partial refresh.
- The seed's small runtime supplies only startup, allocator/recovery traps,
  OS callbacks, and generated support headers. It is fingerprinted and compared
  like generated code.

## Risks

- **ABI drift in generated wrappers.** Remaining C callers depend on exact
  struct layouts and symbol names. Mitigation: keep `compiler_ir.zi` as the only
  layout source, export unchanged C names during migration, and run byte/golden
  output checks before deletion.
- **Recovery semantics.** C uses longjmp-style recoverable scratch and arena
  pause/resume. A direct translation can leak ownership or lose diagnostics.
  Mitigation: introduce explicit result/recovery contexts per module, not global
  jumps; keep sanitizer runs on parser/checker steps.
- **Interning and lifetime.** `KeepText`, source storage and enum/type caches
  intentionally immortalize strings (`zir_services.c:20`, `zir_storage.c:62`,
  `zir_enum.c:62`). A naive Ziran copy can invalidate pointers. Mitigation: keep
  one owned source-store module and borrow only from it.
- **Output drift.** C/Go/SIMD generated text is format-sensitive and downstream
  tests inspect comments and headers. Mitigation: port emitters in small groups
  and compare generated files byte-for-byte before deleting each C file.
- **Host/embedding compatibility.** `zir_host.c` and `zir_host_exec.c` support
  external C hosts and web providers. Mitigation: generated `compiler_host_api`
  must retain every public symbol and struct ABI; run all bundle/host checks.
- **Bootstrap cycle.** Adding a module the seed compiler cannot yet parse or
  lower strands the build. Mitigation: never use a new language feature in a
  compiler module until it exists in the frozen seed and passes
  `check-bootstrap`.
- **Uncommitted Wasm work.** S1/S2 line numbers and behavior are moving. The
  owner should decide whether to land or review that work before implementation.

## Open questions for the owner

No blocking product question remains. The plan makes these explicit decisions:

1. The active Wasm/Asyncify owners finish or hand off their overlapping files
   before S1/S2; self-hosting never overwrites their dirty work.
2. Generated output remains byte-for-byte identical unless a separately named,
   reviewed format correction changes its golden fixture in the same session.
3. Public symbols in `include/ziran_host.h` keep their C ABI names indefinitely
   for Kryon and other embedders even though generated Ziran implements them.
4. `cmd/zir-zx/main.c` is absorbed by final `zi2zx` as specified by the sibling
   naming plan; there is no separate compatibility runner.
5. `update-bootstrap` runs whenever a compiler module or bootstrap-runtime file
   changes. Releases verify that state but do not regenerate unchanged bytes.
6. “Bootstrapped” permits only the tiny reviewed OS/startup runtime described in
   S18; `zi2c` has no hand-written compiler driver or compiler-policy `main`.

## Validation of this planning document

This plan was produced without implementation, builds or commits. The intended
scoped validation is:

```sh
git diff --check -- plans/2026-11-14-self-hosting.md && test -s plans/2026-11-14-self-hosting.md
```
