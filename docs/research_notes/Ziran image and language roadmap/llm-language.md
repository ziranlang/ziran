# LLM-native Ziran authoring and token efficiency

Research date: 2026-11-04. Scope: authoring and AI/tooling ergonomics only. This
is a design note, not an implemented feature and not a claim that Ziran currently
improves model accuracy or inference cost.

## Key Question 1: What direct Ziran improvements make authoring most LLM-native?

### Takeaway

Make `.zi` remain the canonical, human-readable source and make the toolchain
transport the smallest checked semantic units, precise failures, and safe edits.
This is higher value than making the grammar smaller: it changes what context an
agent must retrieve, how few tokens it must emit, and whether malformed or
semantically invalid output can be rejected before it reaches a repository.

### Cited Findings

- Ziran already has the right central invariant: `.zi` parses and checks into
  `.zir`, native backends consume the same checked model, and `.zx` links from
  it; backend incompatibility cannot silently redefine language behavior
  ([ARCHITECTURE](../../ARCHITECTURE.md)).
- Existing laws are named, span-bound, deterministic build gates with JSON
  results and evidence; unsupported or unknown checks are failures unless
  explicitly waived ([LANGUAGE_DIRECTION](../../LANGUAGE_DIRECTION.md),
  [LAWS](../../LAWS.md)).
- Existing JSON diagnostics carry schema version, severity, stable code, span,
  and end span; detailed diagnostics can include expected/actual types, related
  spans, targets, capabilities, suggested names, and one exact replacement edit
  ([`cmd/zir/zir_diagnostic.c`](../../../cmd/zir/zir_diagnostic.c),
  [`tests/tool_diagnostics.sh`](../../../tests/tool_diagnostics.sh)).
- The current formatter has a stable human-readable output and a check mode, so
  a separate model-only formatting experiment can be added without replacing
  that interface ([`tests/tool_diagnostics.sh`](../../../tests/tool_diagnostics.sh)).
- Real module code is already deliberately explicit about ownership and views:
  `TextFind` and `TextSplit` expose exact byte behavior, while `TextReplace`
  states its output allocation in source
  ([`std/text_tools.zi`](../../../std/text_tools.zi)). These are useful evaluation
  subjects because they contain common loops, slices, vectors, and ownership.
- Editing is a distinct capability from whole-function generation. The
  CodeEditorBench abstract explicitly covers debugging, translation, polishing,
  and requirement switching, and reports prompt sensitivity across models
  ([arXiv:2404.03543](https://arxiv.org/abs/2404.03543), v3 dated 2025-04-08).
- Structure-aware agent actions beat brittle text matching in recent evidence:
  CodeStruct's syntactic read/edit actions improved SWE-Bench Verified Pass@1 by
  1.2–5.0% and reduced token consumption by 12–38% for most tested models, with
  the largest gain for a model prone to empty text patches
  ([ACL 2026](https://aclanthology.org/2026.acl-long.607/)).
- Type information is actionable during generation: type-constrained decoding
  reduced compilation errors by more than half and improved functional
  correctness across model families in HumanEval and MBPP experiments
  ([arXiv:2504.09246](https://arxiv.org/abs/2504.09246), v2 dated 2025-05-08).
- Project-specific context and compiler feedback materially matter. CoCoGen
  used static analysis and repository context to improve context-dependent
  generation by over 80% for the evaluated models
  ([ACL 2024](https://aclanthology.org/2024.findings-acl.138/)). INTERVENOR
  reported roughly 18% generation improvement for GPT-3.5 using compiler feedback
  ([ACL 2024](https://aclanthology.org/2024.findings-acl.124/)).
- Repository retrieval is itself a measurable task: RepoBench separates cross-file
  retrieval, completion, and the combined pipeline
  ([ICLR 2024](https://proceedings.iclr.cc/paper_files/paper/2024/hash/d191ba4c8923ed8fd8935b7c98658b5f-Abstract-Conference.html)).
  RepoCoder's iterative retrieval/generation exceeded in-file completion by over
  10% in its reported settings ([EMNLP 2023](https://aclanthology.org/2023.emnlp-main.151/)).

### Inferences

The first five workstreams should be tool and IR extensions, not a second
language semantics:

1. **Stable checked identity.** Give every reachable declaration, field,
   parameter, law, and diagnostic edit a stable module-local semantic ID in
   `.zir`. Text paths and spans remain present, but an edit protocol should not
   rely on substring matching. IDs need not be secret or global: a digest of
   module identity plus declaration path is enough when paired with a document
   version and compiler validation.
2. **API context bundles.** Add a machine-readable `ziran api`/`ziran context`
   projection derived from `.zir`. For each requested symbol it should
   include only the canonical signature, parameter/result types, ownership and
   effect class, law status, relevant records/enums, imports, a bounded source
   excerpt, and call-site examples. This addresses the common failure where a
   model invents a plausible API rather than retrieving the actual one.
3. **Diagnostic protocol v2.** Extend the existing JSON shape with a bounded
   cause taxonomy, minimum reproducible facts, expected/actual semantic
   information, zero or more non-overlapping edit alternatives, and confidence
   only where deterministic. Keep stable codes and schema v1 compatibility while
   tests enforce that every new field is bounded.
4. **Semantic edit protocol.** Add an apply/preview command accepting exact span
   edits or semantic declaration edits. It should reject stale versions,
   overlapping edits, parse failures, formatting-only collisions, and edits that
   do not type-check. This lets a model emit a small replacement rather than a
   whole file while the compiler remains the authority.
5. **Transport-only compact view.** Add a lossless compact serialization of
   canonical `.zi` for model requests and completions, then deterministically
   expand it through the existing formatter. Keep it out of commits unless an
   explicit owner decision changes source policy. A human-readable canonical
   formatter is the stable diff and review interface.

Compiler inference should remove mechanical boilerplate, not semantic choices:

- Preserve explicit public signatures, effects or capability declarations, and
  imports at the API boundary. They are checked contracts and retrieval material.
- Infer local temporary types and deterministic record construction only when
  there is one checked answer and the diagnostic names the inferred type.
- Never infer an import by fuzzy name. Instead, diagnostics may offer an exact
  module/symbol edit after the checker resolves it; ambiguity remains an error.
- Do not shorten names by default. Descriptive names carry repository meaning
  and can tokenize differently across models; renaming is a human/API decision.

Representative current source, taken from
[`std/text_tools.zi`](../../../std/text_tools.zi), should remain readable:

```zi
TextReplace :: (text: string, old: string, new: string) -> string {
    if old.count == 0 { return text }
    at := TextFind(text, old, 0)
    if at < 0 { return text }
    result: Vec(u8)
    start: s64 = 0
    while at >= 0 {
        BuilderAppend(result, text[start:at])
        BuilderAppend(result, new)
        start = at + old.count
        at = TextFind(text, old, start)
    }
    BuilderAppend(result, text[start:text.count])
    return BuilderFinish(result)
}
```

The proposed model-facing improvement is not shorter spelling of that function.
It is a bounded checked-context response shaped like this conceptual record:

```json
{
  "module": "text_tools",
  "symbol": "TextReplace",
  "kind": "procedure",
  "signature": "(text: string, old: string, new: string) -> string",
  "effect": "pure",
  "borrowed": ["text", "old", "new"],
  "owned_result": true,
  "laws": [],
  "excerpt_span": {"line": 109, "column": 1, "end_line": 123, "end_column": 2},
  "uses": ["TextFind", "BuilderAppend", "BuilderFinish"]
}
```

The exact field names are implementation decisions. The critical contract is:
derive it from `.zir`, include spans and exact imports, and make its
byte count deterministic for a given compiler and source version.

### Gaps

- No tokenizer executable or Python tokenizer package is installed in this
  environment. Therefore no token saving from API bundles, compact views, or any
  proposed syntax is measured here. The only honest current claim is that they
  can reduce the amount of retrieved text and emitted edit text by construction;
  they must still prove full-task token and correctness effects.
- No Ziran-specific human study has been run. Human readability and edit-review
  speed must be measured before any model-only source format becomes persistent.

## Key Question 2: Is a concise LLM-first source face justified now?

### Takeaway

Do not build a second language yet. Treat a concise face—whether eventually
called Zia or another name—as an optional, reversible transport/compiler front
end behind the same checker and `.zir`. General-purpose models have no prior for
a new Ziran shorthand, so syntax novelty can cost more tokens through repairs and
ambiguity than it saves on source bytes.

### Cited Findings

- Tokens are model/encoding dependent: OpenAI states that the same text can
  produce different counts depending on model, encoding, and language
  ([OpenAI Help, updated 2026-11-03](https://help.openai.com/en/articles/4936856-understanding-and-counting-tokens)).
- Tokenizer choice can affect model behavior in controlled comparisons. TokSuite
  controls architecture, dataset, training budget, and initialization across 14
  otherwise identical pretrained models with different tokenizers
  ([ICML 2026](https://proceedings.mlr.press/v306/altintas26a.html)).
- Grammar and subword boundaries can misalign even for semantically identical
  code; TokDrift found behavior shifts from formatting and identifier changes
  across nine code models and argues for grammar-aware tokenization
  ([ACL 2026](https://aclanthology.org/2026.acl-long.2199/)).
- Shorthand is not automatically useful to untrained models. Token Sugar reduced
  up to 15.1% of source tokens and up to 11.2% of generated tokens, but its
  evaluated models were trained on transformed code and retained near-identical
  Pass@1 ([arXiv:2512.08266](https://arxiv.org/abs/2512.08266), dated 2025-12-09).
  That evidence does not justify arbitrary shorthand for ordinary hosted models.
- Formatting removal has stronger immediate evidence: across four languages and
  ten LLMs, removing nonessential formatting reduced input tokens by an average
  of 24.5% while maintaining measured performance; the authors propose a
  bidirectional transformation to preserve human readability
  ([arXiv:2508.13666](https://arxiv.org/abs/2508.13666), dated 2025-08-19).
- Even constrained generation has a subtlety beyond grammar size: grammar-aligned
  decoding addresses greedy token-level restriction that can distort the model's
  conditional distribution ([arXiv:2405.21047](https://arxiv.org/abs/2405.21047)).
  Thus “the parser rejects it” improves safety but does not guarantee better or
  cheaper generations.
- Source maps already provide an interoperable way to associate generated and
  original positions, including generated position, original source, and optional
  name fields ([ECMA-426 source map specification](https://tc39.es/ecma426/)).

### Inferences

A concise front end should have four hard architectural rules if it is ever
enabled:

1. **One semantic authority.** It either emits canonical formatted `.zi` before
   parsing/checking, or shares the existing parser/checker representation and
   lowers through the same `.zir`. There must be no second type system, effect
   system, ownership model, package resolver, or backend.
2. **Canonical source remains truth.** Expansion must be deterministic and
   idempotent. VCS diffs, human review, tutorials, and diagnostics reference
   canonical `.zi`; a concise face is a transport or authoring mode, not a fork.
3. **Start with formatting, not new semantics.** The first experiment should be
   a reversible compact formatting view of existing `.zi`, because it changes no
   grammar or names. Only after that should project-specific reversible forms be
   considered, and each must expand to ordinary `.zi`.
4. **Source maps at every boundary.** If a new syntax emits canonical `.zi`, its
   compact-to-canonical mapping must cover declaration, expression, statement,
   and diagnostic spans. Compiler diagnostics must map back to the user's actual
   authoring surface, not merely line 1 of generated text.

A decision gate, rather than enthusiasm for terseness, should decide the new
face. A prototype is justified only if the direct improvements leave a measured
full-task bottleneck in output or context tokens that formatting cannot remove,
and the new form retains human review, repair success, and cross-target parity.

### Tradeoffs

| Option | Token promise | Correctness/readability risk | Decision |
| --- | --- | --- | --- |
| Canonical `.zi` only | None by itself | Lowest; existing humans and tooling | Required baseline |
| Compact transport formatting | Removes nonsemantic whitespace and delimiters when legal | Can impair human review if persisted; possible tokenizer drift | Evaluate first |
| Project-local reversible macros/patterns | Replaces repeated checked patterns | Unknown to general models unless documented; can hide effects | Only checker-visible and project-scoped |
| New concise language face | Potentially removes declaration boilerplate | Novel grammar, ambiguity, training mismatch, second toolchain drift | Gate after direct toolchain work |
| Direct `.zir` generation | Very compact | Unsafe as an open editing surface; loses human-readable authority and invites checker bypass | Not recommended for agents |

### Gaps

- No hosted model evaluations and no tokenizer measurements were run. The lack
  of a local authorized tokenizer was verified; counts remain unmeasured rather
  than estimated.
- Published shorthand and formatting studies cover other languages and models.
  They motivate experiments but cannot be transferred numerically to Ziran.

## Key Question 3: How should Ziran evaluate and implement this without token-count theater?

### Takeaway

Measure complete agent tasks, not isolated file lengths: prompt, retrieved
context, visible completion, hidden reasoning where reported, repairs, failed
tool calls, and cached versus uncached input. Require law checks, tests, exact
edit success, round-trip stability, source-map coverage, and C/Go/portable
semantic parity before any syntax or persistence decision.

### Cited Findings

- OpenAI distinguishes prompt, output/cached input, and reasoning tokens, warns
  that request structure, tools, schemas, images, and files affect the full
  count, and notes that reasoning can make total output exceed visible text
  ([OpenAI Help, updated 2026-11-03](https://help.openai.com/en/articles/4936856-understanding-and-counting-tokens)).
  Current API pricing is expressed per token category
  ([OpenAI API pricing, retrieved 2026-11-04](https://developers.openai.com/api/docs/pricing)).
- LSP is an existing interoperable model for precise diagnostics and text edits.
  Its diagnostics include range, severity, code, source, message, and related
  locations; text edits represent ranges and edits must not overlap
  ([LSP 3.14 specification](https://raw.githubusercontent.com/microsoft/language-server-protocol/gh-pages/_specifications/specification-3-14.md)).
- LLVM demonstrates why a typed, verifiable IR is useful: it has typed SSA forms,
  distinguishes parser acceptance from well-formedness, and runs a verifier after
  parsing and before output ([LLVM Language Reference](https://www.llvm.org/docs/LangRef.html)).
- Deterministic canonical formatting is a proven review interface: Go's `gofmt`
  specifies tabs, recursion over files, check behavior, and in-place formatting
  with restoration on overwrite errors ([Go `gofmt` documentation](https://pkg.go.dev/cmd/gofmt)).
- Benchmark contamination is a present concern. LiveCodeBench was designed to
  collect new problems over time and evaluates self-repair and execution as well
  as generation ([arXiv:2403.07974](https://arxiv.org/abs/2403.07974), v2 dated
  2024-06-06). OpenAI stopped using SWE-bench Verified for frontier coding claims
  after finding exposure to open repositories, fixes, and problem details
  ([OpenAI, 2026-02-23](https://openai.com/index/why-we-no-longer-evaluate-swe-bench-verified/)).

### Evaluation Design

Build a private, versioned Ziran authoring benchmark before changing the source
surface. Freeze task statements, starting revisions, permitted context, models,
tokenizers, temperature, retry limit, and repair budget. Never publish the
holdout set or its exact patches.

Use real tasks from these existing modules:

- [`std/text_tools.zi`](../../../std/text_tools.zi): bounded byte operations, views,
  vectors, and allocation behavior.
- [`std/image.zi`](../../../std/image.zi): loops, records, buffers, and numerical
  edge cases.
- [`std/json_scan.zi`](../../../std/json_scan.zi): parser state machines and
  malformed-input handling.
- [`cmd/compiler_source.zi`](../../../cmd/compiler_source.zi): source rewriting,
  spans, comments, strings, and formatter behavior.
- Law modules from [`docs/LAWS.md`](../../LAWS.md): scalar proofs, finite
  domains, waivers, and certificate steps.

Task genres:

1. Add a small pure procedure plus law.
2. Repair a type, ownership, effect, bounds, or law failure from JSON only.
3. Modify a call site to use an existing API without inventing names.
4. Refactor repeated code without changing `.zir` behavior.
5. Introduce a platform capability and prove portable/native parity.
6. Complete a declaration, body, or cross-file edit without whole-file output.

Metrics:

- **Full token economics:** uncached prompt, cached prompt, tool output,
  completion, reasoning when the API reports it, every retry, and total. Record
  tokenizer family/version and whether cache hits are excluded. Static source
  token counts are diagnostic only, never the headline.
- **Correctness:** `ziran check`, law closure status, module tests, and no
  unwaived unknowns. For code changes, no unrelated behavior change.
- **Edit reliability:** exact edit accepted on first application, no overlap,
  no stale-version rejection after expected rebase, and no unintended files.
- **Repair efficiency:** failures to law/test pass, number of compiler/tool
  turns, and total tokens consumed by repair.
- **Context quality:** number of semantic units and bytes retrieved, retrieval
  precision/recall against actual required declarations, and missing imports.
- **Round trip:** `expand(compact(canonical)) == canonical`, repeated compact is
  stable, formatter is idempotent, and semantic `.zir` comparison is unchanged
  except for intended effects.
- **Source maps:** every diagnostic in a compact or concise face maps to the
  user-facing span and at least one canonical span; unresolved mapping is an
  error in tests.
- **Human readability:** independent reviewers perform comprehension and defect
  finding on canonical and expanded output under fixed time; report completion,
  errors, confidence, and preference. No external study is sent during local
  implementation.
- **Ambiguity:** parser count for each compact/concise construct, rejection
  cases, and diagnostic quality. A construct that commonly needs whitespace or
  punctuation repair is not accepted merely because it is short.
- **Cross-target parity:** identical supported behavior on C, Go, and `.zx`;
  platform capability differences must be explicit, deterministic results.
- **Leakage control:** source repository, task creation date, model training
  cutoff, nearest public code match, and holdout status are recorded. Public
  benchmark tasks are adaptation-only, never primary success claims.

Tokenizers, when locally available and authorized, must span at least three
families: byte-pair encoding, SentencePiece/Unigram, and WordPiece or a similar
subword family. Do not download large models for this evaluation. If fewer are
installed, report “unmeasured for family X,” not an average extrapolated from
other families.

Decision gates:

- **Gate A, tooling baseline:** semantic API context and diagnostics do not
  regress any existing check, formatter, native target, or portable bundle test.
- **Gate B, context:** retrieval returns fewer semantic units/bytes at equal or
  better first-edit correctness; no decrease in law/test correctness.
- **Gate C, repair:** structured diagnostics/edit protocol reduces total repair
  tokens at equal or better pass rate versus current JSON and full-file edits.
- **Gate D, compact transport:** at least 15% median full-task total-token
  reduction across available tokenizer families, no statistically meaningful
  correctness loss, exact round trip, and no human-review regression. This is a
  proposed decision threshold, not a predicted result.
- **Gate E, concise face:** at least 20% median full-task reduction beyond the
  accepted compact view, strong edit reliability, near-perfect deterministic
  expansion, mapped diagnostics, unchanged laws and cross-target parity, and a
  human-review win. Only then should a named language face move from prototype
  to product design.

### Staged Implementation Tasks

No implementation is authorized by this note. These are the proposed contracts:

| Stage | Proposed files/functions | Required behavior | Tests and definition of done |
| --- | --- | --- | --- |
| 0. Evaluation harness | `tests/llm_authoring/tasks.json`, `tests/llm_authoring_cases.zi`, `tests/llm_authoring.sh` | Versioned private task manifest, expected law/test outcomes, transcript/token schema, no model calls or downloads by default | `sh tests/llm_authoring.sh build/bin/ziran` validates manifests, fixture integrity, output schema, and can replay supplied transcripts |
| 1. Checked API projection | `cmd/zir/zir_api.c`, `cmd/zir/zir_api.h`, `cmd/api.zi`, docs | Stable JSON for modules, declarations, signatures, types, effects, ownership, imports, laws, spans, and bounded examples | New `tests/zir_api.sh` checks deterministic bytes, imports-only graph, exact spans, effect/law data, and no source-only inference |
| 2. Diagnostic v2 | extend `cmd/zir/zir_diagnostic.c/.h`, `cmd/diagnostics.zi` | Add bounded cause facts, semantic IDs, related graph, alternatives, and exact non-overlapping edits while preserving v1 | Extend `tests/tool_diagnostics.sh` and `tests/diagnostic_details.sh`; schema-v1 and v2 consumers both pass |
| 3. Semantic edit command | `cmd/zir/zir_edit.c/.h`, `cmd/edit.zi`, docs | Preview/apply span or declaration edits with document version, overlap rejection, parse/check, formatter, and atomic file replacement | New `tests/semantic_edit.sh` covers stale versions, collisions, imports, malformed edits, formatting-only noise, and rollback |
| 4. Compact transport | extend `cmd/compiler_source.zi`, `cmd/format.zi`; add mapping writer | Lossless compact/expand of existing syntax only, stable mapping for statements and diagnostics, never required by `ziran fmt` | New `tests/transport_view.sh`: two-way round trip, idempotence, malformed recovery, source-map coverage, formatter equivalence |
| 5. Safe inference | `cmd/compiler_declaration.zi`, `cmd/compiler_expression.zi`, checker diagnostics | Infer only unique local types and deterministic construction; public signatures and imports remain explicit; offer exact import edits only | Extend declaration/expression tests for ambiguity, recursive inference, foreign/owned types, and diagnostic boundedness |
| 6. Optional concise prototype | `cmd/compiler_concise.zi`, parser adapter, mapping tests | Emits canonical `.zi` and uses one checker; no backend branch; all diagnostics map back | New `tests/concise_face.sh`; Gate E plus native/portable parity and law tables must pass before product exposure |

Stage 0 must land first. Stages 1–3 can proceed independently once their tests
exist, but no stage may weaken the law gate or portable verifier. Stage 6 stays
experimental until every earlier gate and the explicit owner decision are
recorded.

### Gaps

- The proposed command/file names are planning targets; actual implementation
  must re-check the current parser/API split before editing.
- No statistical power analysis or human-review panel has been established.
  Gate thresholds should be revisited with observed variance before they become
  release policy.
- No GitHub checks are relevant to this research-only note. Local validation is
  limited to the documentation existence and repository diff checks performed in
  this task; no build or compiler suite was run.
