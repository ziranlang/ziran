# Authoring workflow evidence

Canonical `.zi` is the only authoring source. The checked tools around it are:

1. Retrieve: `ziran api --json --module NAME --symbol NAME|ID --max-bytes N`
   ([API discovery](API_DISCOVERY.md)).
2. Diagnose: `ziran check --diagnostics=json` ([diagnostics](DIAGNOSTICS.md)).
3. Edit: `ziran edit --root DIR request.json` previews a change. `--apply`
   writes it atomically after the same checker and law gates
   ([checked source edits](SEMANTIC_EDIT.md)).
4. Optional transport: `ziran compact`/`expand --map` (an exact round trip,
   see the [compact transport](TRANSPORT_VIEW.md)).
5. Verify: `check`, `ir`, `bundle`, `run`, `zi2zx run --vm`, and native C/Go
   runs where the task asks for portable parity.

The lean suite checks these production commands directly:

```sh
build-queue -- sh tests/zir_api.sh build/bin/ziran
build-queue -- sh tests/semantic_edit.sh build/bin/ziran
build-queue -- sh tests/transport_view.sh build/bin/ziran
build-queue -- sh tests/tool_diagnostics.sh build/bin/ziran
```

The older corpus replay suites and their evaluator helpers were removed during
test pruning. The frozen L-01 corpus (`tests/llm_authoring/tasks.json`, version
1.0.0) remains as research data for the historical methodology and reports below.

## What each task checks

- **Retrieval:** a full API projection is checked twice for identical bytes.
  Every semantic ID must be unique, and every declaration must bind the
  SHA-256 and length of the bytes it was read from, with a source anchor that
  starts at its name. Bounded name lookups must stay within 16 symbols and
  4096 bytes, and looking a symbol up by its ID must return exactly that
  declaration. Saved IR must keep the same IDs and report no document identity.
- **Diagnostics:** schema 2. The report records each diagnostic's code,
  semantic ID, document binding and covered source text. For a span repair it
  also records whether the diagnostic overlaps the repaired span.
- **Edit:**
  - A preview returns the frozen solution bytes and leaves the file untouched.
  - A rollback candidate, which the checker or law gate rejects, leaves the
    file and directory unchanged.
  - Apply writes exactly the solution and keeps the file mode.
  - Replaying the request against its old hash is rejected as stale.
  - Afterwards, IDs stay stable, the edited document's version advances,
    unrelated documents keep theirs, and a replaced declaration's ID no
    longer resolves.
  - On a document that does not check, a semantic edit is rejected and the
    repair uses an exact span.
  - Creating a document is rejected, so the cross-file task is recorded as
    unsupported by a single-document transaction.
- **Transport:** the initial and final documents and the listed context
  modules must all round-trip byte for byte. Each
  map must cover every byte and bind both SHA-256 values, compaction must
  repeat exactly, and the expand map must equal the compact map. The restored
  failing sources must produce identical diagnostics, whose offsets map
  through the transport.
- **Execution:** the restored final bytes go through the L-01 checker, law,
  saved-IR, bundle, `zi2zx run --vm` and native parity pipeline.

The observation phase runs twice in fresh directories and must give identical
results.

## Report

The report has `schema: "ziran.authoring-workflows"` and `schema_version: 1`,
separate from the L-01 transcript schema. It records:

- the corpus version and manifest SHA-256, plus the plan and evaluator
  hashes;
- the `ziran --version` string and SHA-256 hashes of `ziran`, `zi-api`,
  `zi-edit`, `zi-fmt`, `zi2zir`, `zi2c`, `zi2go` and `zi2zx`;
- the API budgets used, each tool's requests and response sizes, and its
  results;
- `context_bytes` for each task. These are UTF-8 byte counts for:
  - the canonical source (task documents plus listed context modules);
  - the full and bounded API projections and the diagnostics;
  - edit requests and responses;
  - writing the whole document out instead;
  - the compact transport of what is read (initial documents and context
    modules) and what is written (final documents).

Bytes are never tokens. `tokenization.status` is `unavailable`. No exact
public tokenizer adapter is implemented. The report only lists whether the
`tiktoken`, `tokenizers`, `sentencepiece` or `transformers` packages are
installed; nothing is imported from them. Every corpus tokenizer family
reports `token_counts: null`. `llm_calls` is 0 and `model_results` is `null`.

## LLM-first view gate

A separate LLM-first source language or view, such as a “Zia” transpiler, is
deferred. `zia_gate` permits design work only when **all** of these are met:

- exact token measurement;
- a byte reduction across the corpus's final documents;
- correct round trips;
- measured model authoring quality;
- an independent human-readability review.

On corpus 1.0.0 the compact transport is larger than the canonical source for
every corpus document: it adds a 13-byte header and these files have no runs
of 8 or more identical whitespace bytes. Only the indented `std/text.zi`
context module gets smaller. On these small modules, scoped API JSON is often
larger than the source it describes. There are no exact tokenizer results and
no model or readability evidence, so the gate reports `deferred` and no new
syntax is proposed.
