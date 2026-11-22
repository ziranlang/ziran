# LLM authoring audit: Ziran (2026-11-22)

Scope: the Ziran language, compiler messages, docs and tooling as seen by an LLM
agent never trained on Ziran. Kryon widgets/CSS are covered by a sibling audit.
Read-only: no source, `build/` or `cmd/` file was touched; the only written file
is this report. Scratch programs live in `/tmp/ziran-llm-audit`; `ziran` 0.8.0 is
on PATH (`which ziran` -> `/home/wao/.local/bin/ziran`). Each claim cites
`file:line` or the exact command and its output.

## 1. Measured evidence of where models fail

Sources: benchmark plan `harness/harmony/plans/2026-11-17-cheap-runtime-benchmark.md`
(rounds 1/2) and `harness/benchmarks/results/` (visual3d ratings, ranked runs).

| Task | Model (runtime) | Result | Failure cause |
| --- | --- | --- | --- |
| calc, semver, todo r1 | Gemini 4 Argon high (agy) | all built; 62/62, 108/108, 30/30; 9/10 quality (bench:31) | none; reported 3 toolchain traps: `MIN` prints `-`, `NAME :: cast(T)v` rejected, slice view via pointer param rejected (bench:58-60) |
| calc r1 | unshown (cmd) | does not compile (bench:32) | `for` over a Vec returned by a call (main.zi:29), named results used as locals (tokens.zi:35), duplicate `found` binding (version.zi:121), missing `;` (app.zi:19) (bench:68-72) |
| semver r1 | cmd | does not compile | same declaration-form guesses (bench:68-72) |
| todo r1 | cmd | does not compile | missing `;` on file-scope var; never got a queue build slot (bench:71-72,35-39) |
| calc r1 | glm-5.3-flash (Co-Z) | does not compile, 1 error; parser 60/62 via judge main (bench:33) | `input := ReadStandardInput()` then `TextView(input)` refused (main.zi:5-6) (bench:92-93) |
| semver r1 | Co-Z | does not compile, 1 error | `u64` index compared with `s64` count (version.zi:155-156) (bench:92-93) |
| todo r1 | Co-Z | builds; judge 16/30 | app never adds: text kept in `editor_bytes` (ui.zi) but Add reads `state.draft` (todo.zi:32-34); UI test used frame replay, failed on host capability (bench:93-101) |
| calc r2 | cmd | built; 62/62; 9/10 (bench:125) | none |
| semver r2 | cmd | built; 108/108; 9/10 (bench:125) | none |
| todo r2 | cmd | built; 30/30; 9/10 (bench:125) | none |
| calc r2 | Co-Z | built; 61/62; 8/10 (bench:126) | deep overflow edge: `MIN % -1` returns overflow instead of 0 (bench:126,149-150) |
| semver, todo r2 | Co-Z | 108/108; 30/30 (bench:126) | none |
| scene3d visual | deepseek-v4.1-flash, gemini-4-neon-xhigh, gpt-6.1-sol | 80/80 each; 1251s/687s/390s of 1800s budget; diagnostic status, unranked (SCORES.md:5-13, ratings.json rows) | none |

Frequency ranking of failure causes (rounds 1+2, 18 program slots):
1. Declaration/binding forms guessed from other languages (for-over-call result,
   named results as locals, duplicate binding, missing `;`): 4 of 6
   non-compiling round-1 programs (bench:68-72,92-93).
2. Ownership/move rules (unbound temporary, Text bind, Vec moves): 2 blocking
   errors plus the todo state bug (bench:92-96; SKILL.md:148-166).
3. Mixed integer widths needing `cast`: 1 blocking error (bench:92-93).
4. Wrong/wired state in UI code: 1 functional failure, 16/30 (bench:93-96).
5. Deep arithmetic semantics (`MIN % -1`, `MIN` printing): 1 hidden-case miss +
   3 toolchain traps (bench:58-59,126).
6. Process, not language: queue starvation, no report, shell/Python instead of
   Ziran tests, wrong README paths (bench:35-39,74-81,103).
Key result: with a prepared skill + skeleton, round-2 failures dropped to one
overflow edge (bench:140-151) - the gaps are learnable, so surfacing them in the
compiler/docs is the highest-value fix.

## 2. Pitfall inventory (SKILL.md, AUTHORING.md, USAGE.md)

Class: (a) removable by compiler/language fix, (b) should be an error/warning
with a fix hint, (c) inherent to a low-level language. "caught" = `ziran check`
errors on it today (verified in section 3 or cited).

| Pitfall | Source | Class | caught |
| --- | --- | --- | --- |
| `for` over a call result must be bound first | SKILL.md:148; bench:68 | b | yes (m17) |
| Named results are not locals | SKILL.md:149; bench:69-70 | b | yes (bench) |
| Duplicate `:=` binding of same name | SKILL.md:150; bench:70-71 | b | yes (bench) |
| File-scope variables need trailing `;` | SKILL.md:151; bench:71 | b | yes (m04) |
| `NAME :: cast(T)literal` rejected where used | SKILL.md:152; bench:58-59 | a | yes (error, no hint) |
| `x := OwnedReader()` then `TextView(x)` refused; declare `x: Text` | SKILL.md:153; bench:92-93 | b | yes (type mismatch) |
| Integer widths never mix (`u64` vs `s64`) | SKILL.md:154; bench:92-93 | b | yes (type mismatch) |
| Store through pointer is `<<p = v`; `*p` not assignable | SKILL.md:155; m18 | b | error message is jargon (`statement is not supported ... <<p;`) |
| `VecSlice` through pointer parameter rejected | SKILL.md:156; bench:59-60 | b | yes (bench) |
| Slice of loop-local Vec dangles; store the Vec | SKILL.md:157-158 | b | yes (`check.slice_lifetime`, `ziran explain check.slice_lifetime` verified) |
| `BuilderPrint` format must be a literal (no `ifx`) | SKILL.md:159 | b | yes, but misleading (m14) |
| `BuilderPrint`/`FormatInteger` print `S64_MIN` as `-` | SKILL.md:160; bench:58 | a | no (silent wrong output) |
| `TextInteger` rejects >18 digits | SKILL.md:161 | b | likely (parse path); undocumented alternative |
| Division by zero aborts; `MIN / -1`, `MIN % -1` wrap | SKILL.md:162; bench:126,149-150 | c (opt-in `--checks` exists, USAGE.md:1331-1358) | no |
| Returning a Vec parameter moves/empties the caller binding | SKILL.md:163-164 | b | partially (lifetime family) |
| Call argument cannot move a global Vec | SKILL.md:165-166 | b | yes (move rules) |
| Write tests as Ziran programs; use PATH `ziran`, never toolchain paths | SKILL.md:167; bench:78-81,103 | process | n/a |
| Project build needs `ziran lock` first (`cannot read ziran.lock`) | SKILL.md:23; bench:23-25 | b | no hint naming `ziran lock` |
| Returned views must borrow caller-owned storage | USAGE.md:1298-1314 | c (checked today) | yes (`check.slice_lifetime`) |
| A view of a fresh call result is rejected; bind the owner first | USAGE.md:1592 | b | yes (m17 family) |
| Text result detach/keep rules (string result leaks, Text frees) | USAGE.md:1594-1626 | b (needs hint-level docs) | yes (checker enforces) |
| Shadowing allowed; two bindings in one block rejected | USAGE.md:1316-1321 | c | yes |
| `--checks` off = no runtime defense (manual safety, like C) | USAGE.md:1323-1358 | c | n/a |
| No C-style `for(;;)`, no `match`, no `switch`, no `goto` | LANGUAGE_DIRECTION.md:144-147 | b (targeted hint) | generic parse error only |
| Format is bare `%` only (no `%s`/`%d`) | format.zi:1-13; m14 | b | misleading message |
| `std/file` absent on Go target and portable bundles | USAGE.md:2242-2243 | c | target check |

`ziran check --lint` adds warnings for redundant casts, numeric char compares,
and int-as-bool locals (USAGE.md:327-334) - the only lints today; unused
local/import lints are planned but not started (lean-language plan:244-268,444).

## 3. Diagnostic quality on typical LLM mistakes

All commands: `ziran check --root . <file>` in `/tmp/ziran-llm-audit` (bare
`ziran check file.zi` outside `--root` prints only the zi2zir usage line).
JSON exists for every diagnostic via `--diagnostics=json` (DIAGNOSTICS.md:31-38);
verified record for m09 (schema v2, `code`, span, `cause.kind=unresolved_function`,
`alternatives:[]`). Text mode has no caret and drops expected/actual/quickfix
data that JSON carries (lean-tooling plan:135-137; fields DIAGNOSTICS.md:76-85,92-95).

| Mistake (file) | Exact first message | line+col? | fix hint? | did-you-mean? |
| --- | --- | --- | --- | --- |
| `let x = 1;` (m01) | `m01_let.zi:1:1: invalid top-level declaration` | yes | no | no |
| `fn main()` (m02) | `m02_fn.zi:1:1: invalid top-level declaration` | yes | no | no |
| `func main()` (m03) | `m03_func.zi:1:1: invalid top-level declaration` | yes | no | no |
| file-scope `x := 1` (m04) | `file-scope variable declaration needs ';'` | yes | partial | no |
| `for i in 0..n` (m05) | 12 cascading errors incl. `unresolved name: range_first_1 (did you mean range_last_1?)`, `unresolved name: it`, `expression is not supported by language checking: (i in 0) + (n)` | yes | no | yes, but for internal generated names |
| `&&`/`||`/`!` (m06) | none - valid Ziran | - | - | - |
| `nil` (m07) | `m07_nil.zi:1:45: nil is not Jai syntax; use null` | yes | yes (model answer) | - |
| wrong param type into Vec-typed value (m08) | `argument type mismatch: TextTrim (expected string, found __type_c53c2d7c4913dc02)` + note `text_tools.zi:55:1: The required type is declared here` | yes | partial | leaks internal type hash instead of `Vec(string)` |
| typo `TextLins` (m09) | `unresolved function: TextLins` then `unknown declaration type` | yes | no | no (`alternatives:[]` in JSON despite close `TextLines`) |
| arg count `TextFind("abc")` (m10) | `argument count mismatch: TextFind (a required argument is missing)` | yes | partial (no signature shown) | no |
| arg type `TextTrim(42)` (m11) | `argument type mismatch: TextTrim (expected string, found an integer literal)` + declaration note | yes | yes | - |
| `s.len()` (m12) | `unknown record field: len`, then `unresolved function: ` (empty name), `unknown declaration type` | yes | no | no (should suggest `.count`) |
| field named `bool` (m13) | none - compiles; `bool` is a legal field name | - | - | - |
| format `"%s\n"` (m14) | `BuilderPrint has more arguments than % in its format` | yes | misleading (counts match; `%s` is just an unsupported spelling) | no |
| `VecPush` without `#import "std/vec"` (m15) | none - `std/vec` auto-imports when `Vec(T)` is named (USAGE.md:1582-1584) | - | - | - |
| `TextSplit` without import (m16) | `unresolved function: TextSplit` + 3 cascade errors (`parts`, `.count`) | yes | no | no; no "did you forget an import" |
| `for TextLines("a b")` (m17) | `a Vec slice views a named Vec place: TextLines("a b")[:]`, then `temporary owned result must be bound or passed to an owning call: TextLines` + 2 cascades | yes | rule stated, jargon lead | no |

`ziran explain` registry: 198 codes (`ziran explain --list | wc -l` -> 198);
`ziran explain check.type` -> summary "The type checker rejected a declaration,
expression, signature, or effect" + next step advising `ziran api`.
Summary: span accuracy is good; fix hints exist only for `nil`; name suggestions
and parse recovery (one-error-per-round) are the weakest points - exactly what
round-1 contestants hit (bench:68-93).

## 4. Discoverability

| Need | Tool | Evidence |
| --- | --- | --- |
| List a module's public procedures + signatures | `ziran api [--json] --root DIR file.zi`, `--module NAME`, `--symbol NAME/ID`, `--imports-only` | API_DISCOVERY.md:3-11,109-119 |
| Sample output (5 of 11 lines): `ziran api --root .../std .../std/hash_map.zi` -> `module hash_map (hash_map.zi)` / `type HashMap(K,V) @ hash_map.zi:13` / `HashMapGet :: (map: *HashMap($K, $V), key: K, value: *V) -> bool [template] [effect: mutating] @ hash_map.zi:60` / `HashMapSet :: (...) -> void [template] [effect: pure] @ hash_map.zi:67` / `HashMapRemove :: (...) -> bool @ hash_map.zi:93` | verified run | exact output above |
| Search by name | `ziran api --symbol NAME` (overloads attach; IDs distinguish overloads) | API_DISCOVERY.md:109-119 |
| List std modules | no command; `ls std/*.zi` -> 134 files; `ziran api` loads per input file only | counted; API_DISCOVERY.md:38-43 |
| Builtin procedures invisible: `VecPush` is compiler-lowercased (bootstrap/compiler_zir_lower.c:17633), absent from `ziran api` output for a `std/vec` importer (verified run: only `Vec`, `Text`, `TextCopy`, `TextFromBuilder`, `TextDetach` listed) | gap | command shown |
| Short reference from the binary | `ziran guide` -> 60 lines incl. edit loop and `ziran api/features/capabilities` | USAGE.md:320-321; output captured |
| Explain codes | `ziran explain CODE [--json]`, `--list` -> 198 codes | DIAGNOSTICS.md:45-66 |
| Feature contracts | `ziran features --json` (16 contracts; "does not yet replace the language reference") | FEATURES.md:5-13,307-313 |
| Target limits | `ziran capabilities --target=go --json` | USAGE.md:322-323 |

Doc sizes (`wc -l`): USAGE.md 2489, AUTHORING.md 28, DIAGNOSTICS.md 94,
API_DISCOVERY.md 177, LANGUAGE_DIRECTION.md 452. Compact prompt reference:
LANGUAGE_DIRECTION.md (452) is under 600 lines but its syntax half (139-452) is
dense prose mixed with laws/parallel topics; there is no prompt-shaped cheatsheet
and no `llms.txt` (`find site web -maxdepth 1 -iname '*llms*'` -> no match).
Site mirrors docs as HTML (site/guide/*.html, per-module site/std/*.html).
Runnable examples: examples/ has 16 files (hello, loops, structs, text_basics,
...); `ziran check --root examples examples/loops.zi` (and text_basics.zi,
structs.zi) all pass clean. templates/cli and templates/lib provide project
skeletons (USAGE.md:320-321). USAGE.md opens with inline assembly (line 5), not
basics - bad first page for an agent reading top-down.

## 5. Syntax distance (vs C, Go, Odin/Jai)

| Area | Ziran | Nearest analogue |
| --- | --- | --- |
| Declarations | `Name :: (a: s32) -> s32 { }`; constants `X :: 1;`; locals `x := 1` | Jai/Odin (no C/Go analogue) |
| Types | postfix `name: Type`; generics `Vec($T)` / `Box :: struct($T: Type)`; `cast(T)v` | Jai `$T`; Go `T(v)` |
| Control flow | `if`/`while`; `for i: 0..n` (inclusive); `for x {}` binds `it`, `it_index`; `for *`, `for <` reverse; labeled break | none mainstream (Go `range` closest) |
| Multiple results | `-> (value: s64, ok: bool)`; call-site `v, ok := F()`; named results are NOT locals | Go returns; Jai multivalues |
| Errors | value+ok pairs, no exceptions; `#must` forces use | Go if-err; no analogue for `#must` |
| Strings | `string` (borrowed view) vs `Text` (owned Vec bytes); `.count` not `.len()`; `#char "A"` byte literal | Odin `string`; Rust String/&str split |
| Memory | manual, moves/drops like C++ but silent; Vec/Text auto-cleanup on scope exit; no GC except Go target | Odin/Jai allocator style |
| Pointers | store via `<<p = v`; `*p` not assignable; fields auto-deref `p.f` | none (unique `<<`) |
| Modules | `#import "std/vec"`; `M :: #import "m";` namespaced; `#import, file/string/dir`; `#load` | none this flexible |
| Compile-time | `#run`, `#if/#ifx ... then`, `#insert`, `#module_parameters`, `#string END...END;` | Jai metaprogramming |
No-analogue guesses an LLM makes wrong: `::`/`:=` split, `it`/`it_index`, `<<`,
named-result semantics, Text move/detach rules, `ifx ... then`, `T.{field=v}`,
`.Member` enums needing `using Enum;` (LANGUAGE_DIRECTION.md:203-208), `#char`,
`for i: first..last` inclusive ranges (LANGUAGE_DIRECTION.md:236-249).

## 6. Standard library shape for app work

| Area | Import | Key signatures (std source) | USAGE.md example? | Gap |
| --- | --- | --- | --- | --- |
| Strings | `std/text_tools` | `TextFind(text,needle,from:=0)->s64` (text_tools.zi:13), `TextTrim(text)->string` (:55), `TextSplit(text,sep)->Vec(string)` (:72) | yes, 143-145,1704-1757 | no regex; `SplitWhitespace` in `std/unicode` |
| Formatting | `std/format` (auto with BuilderPrint) | `Append(*Vec(u8), string/s64/...)` (format.zi:16-31), `BuilderPrint(*b,"% of %\n",a,b)`, `BuilderFinish(b)` (format.zi:1-13) | yes 1570-1578 | bare `%` only; 6 float digits; `S64_MIN` prints `-` |
| Lists/maps | `std/vec` (auto), `std/hash_map` | `Vec(T)` struct (vec.zi:14); `HashMapSet/Get/Remove(*map,...)` (USAGE.md:1690-1702) | yes 1394-1495,1682-1702 | `VecPush` etc. builtins invisible to `ziran api` |
| JSON | `std/json_document`, `std/json_scan` | `JsonMember/JsonElement/JsonString/JsonWrite` tree (USAGE.md:1859-1893); scan offsets (1835-1857) | yes 1833-1907 | two-module split (scan vs tree) needs guidance |
| Files | `std/file` | `ReadEntireFile(path)->(text,ok)`, `WriteEntireFile(path,text)->bool` (USAGE.md:2234-2239) | yes 2221-2243 | absent on Go/portable (2242) |
| HTTP | `std/net_http` capability; working impl `std/http_get_linux` | `GetHttps(url, output: []u8) -> CaptureResult` (http_get_linux.zi:16) | capability only (2204-2209) | no portable/Go HTTP; TLS via host |
| Time | `std/date_time_linux`, `std/calendar` | `UnixNow()`, `LocalAt(seconds)`, `FormatDateTime(reading,pattern,out)` (USAGE.md:2438-2456) | yes 2436-2463 | Linux/Win native only |
| Threads/async | `std/thread_linux` | `ThreadStart(*Thread, procedure, ...)` (thread_linux.zi:91), `ThreadFinished/ThreadWait` (USAGE.md:2268-2284) | yes 2268-2284 | no async/await; channels only Go target (`std/channel_go`, USAGE.md:1016-1022) |

Awkward for LLMs: ownership-split modules (`*_linux` vs portable capability),
target-dependent availability, and move-on-return Vec semantics (SKILL.md:163-166).

## 7. What is already planned

- LANGUAGE_DIRECTION.md:7-20 makes LLM-repairable diagnostics (law name +
  smallest span, JSON results) an explicit requirement; law gates are implemented
  (LANGUAGE_DIRECTION.md:54-56).
- lean-language plan (2026-11-18): P2 multi-error parse recovery (M), P3 typed
  diagnostics fields + registry since/stability (S), P6 unused local/import lints
  (S), P7 ambiguity candidates, P8 literal-default table (summary table :13-21).
  Done: P5 reserved-name guard, P7, P8-documented; P3-since deferred; P6 and
  P1/P2/P4 not started (:441-445).
- lean-tooling plan: P1 persistent `ziran serve` session, P2 digest check cache,
  P3 cancellation, P4 editor diagnostics parity, P5 `ziran test` + unused-import
  lint (:41-171). Done: P4 slice 1 - edit result carries a diagnostics array;
  text-mode caret/expected/actual and P1/P2/P3/P5 remain future (:230-231).
DIAGNOSTICS.md:76-85 confirms JSON already carries expected/actual types, cause
kinds and `suggested_name` - but section 3 shows suggestions rarely fire; wiring
them up is the open half.

## Top gaps

Landed: 1 (docs/REFERENCE.md, site/llms.txt), 8 (its "Moves and views" section),
3 (foreign-syntax hints), 4 and 10 (name and #import suggestions), most of 2 and 7.

2. Multi-error parse recovery (lean-language P2) is still open: a parse error
   stops the file, so a second mistake shows only on the next run. The
   for-in, loop-over-call and failed-declaration cascades are fixed.
   Found while landing gap 3: a file-scope initializer whose string literal
   contains `::` is misread as a declaration ("invalid file-scope constant");
   tests/tool_diagnostics_cases.zi keeps its hint table local for that reason.
5. Compiler builtins (`VecPush`, `VecClear`) invisible to `ziran api` (section 4)
   - expose builtins in api/features output; files: cmd/zir api tooling, docs;
   test: api schema golden.
6. Text-mode diagnostics drop expected/actual and quickfix (lean-tooling:135-137)
   - finish P4: caret line + expected/actual suffix + fix in the same message;
   files: cmd/zir/zir_diagnostic.c; test: diagnostic_format.sh goldens.
7. The pointer-store message for a bare `<<p;` statement still says
   "statement is not supported ... <<p;"; files: compiler_check_function.zi.
9. Silent wrong-output edges: `S64_MIN` prints `-`, `MIN % -1` wraps (SKILL.md:160,162;
   bench:126) - keep low level: fix `FormatInteger`/`BuilderPrint` spelling and add
   a `check --lint` warning on `/`/`%` with constant -1; files: number_text.zi,
   zir_check_program.c; test: std_float_text-style edge suite.
11. Project workflow errors lack the fix command (`cannot read ziran.lock`,
    SKILL.md:23; bench:23-25) - message should say "run `ziran lock`"; files:
    cmd/package.zi; test: package launcher cases.
12. No command lists std modules and doc site is HTML-only for agents (section 4)
    - add `ziran std --list [--json]` (names + one-line purpose) and llms.txt
    mirrors; files: cmd/package.zi, site; test: schema golden + count = 134.

Evidence commands (sections 3/4) were run against `ziran 0.8.0` with scratch
files in `/tmp/ziran-llm-audit`; no repository file other than this report was
created or modified.
