# Offline Ziran authoring evaluator (L-01)

This is a hand-authored regression corpus, not a model benchmark result or a
private holdout. All product source and replacement artifacts are `.zi` files.
No model, remote service, tokenizer package or download is used. Go execution
uses the installed toolchain with `GOPROXY=off`, `GOSUMDB=off` and
`GOTOOLCHAIN=local`. C and Go toolchains must already be installed.
The toolchain must provide the explicit portable interpreter `zi2zx-vm` beside
`ziran`; a missing interpreter is an infrastructure error.

From the repository root:

```sh
build-queue -- sh tests/llm_authoring.sh build/bin/ziran
build-queue -- python3 tests/run_check.py --filter llm_authoring --jobs 1
python3 tests/llm_authoring.py --validate-only
build-queue -- sh tests/llm_authoring.sh build/bin/ziran \
  --transcript tests/llm_authoring/fixtures/replay.json
```

The shell script is automatically discovered by `tests/run_check.py`; no
Makefile registration is needed. The default command runs the full evaluator
regression suite. `--transcript` evaluates one supplied record instead.
`--validate-only` validates the corpus and any supplied record without invoking
the compiler. Exit codes are 0 for pass, 1 for a valid replay that fails its
goal, and 2 for schema, infrastructure or regression-suite errors. Results are
JSON on stdout; schema/infrastructure errors are JSON on stderr.

Compiler invocations run with display, authentication and desktop-session
variables scrubbed, desktop recovery disabled and cache/temp paths inside the
owning repository's ignored `build/scratch/llm-authoring/`. The evaluator checks
that ignore before writing transcript-derived source. It deletes only its own
scratch directory. It never edits the corpus, working source or installed build
outputs. If retaining a private supplied transcript or report, first verify its
chosen path with `git check-ignore`; do not add private evaluations to fixtures.
Source-derived compiler output is bounded to 4 MiB per stream. Compiler
invocations time out after 90 seconds; no timings are used as benchmark data.

## Corpus interface

`tasks.json` has `schema_version: 1` and a separately versioned
`corpus_version: "1.0.0"`. Any task, fixture, expected result or prompt change
requires a corpus-version change before collecting comparable evaluations.
The checked-in SHA-256 values freeze the initial and solution source bytes;
the report records SHA-256 for the actual launcher, backend compiler tools and
`zi2zx-vm` interpreter. This does not
claim that uncommitted compiler sources match that executable.

Each task fixes its prompt, permitted `context` module paths, initial/solution
file maps, entry symbol, retry limit, repair budget and expected initial/final
outcomes. Each file-map value contains `fixture` (relative to `fixtures/`) and
`sha256`. Expected checker results include exact exit code, complete named law
status map and, where applicable, diagnostic field matches. `run_stdout` fixes
the exported entry's portable result. The repair budget counts attempts after
the first, as does the retry limit; both limits apply.

The seven tasks exercise pure procedure/law creation, a JSON type diagnostic,
existing `std/text` API use, cross-file import/refactoring, C-only capability
rejection and portable replacement, disproved-law repair and exact byte edits.
Initial and solution programs run through the actual `ziran check` with the
recorded target expectations. Successful programs also run through `.zi` to
`.zir`, saved-IR checking, source/saved `.zx` byte equality, ordinary bundle
execution through `ziran run`, and explicit source/saved execution through
`zi2zx-vm run`. Ordinary bundle execution can select the native tier; the
`source_vm` and `saved_vm` results specifically exercise the portable
interpreter. Tasks marked `portable_parity` execute their solutions on native C
and Go as well. Native C uses `ziran run`; Go uses the existing generated
executable-package path and `go run .`, because `ziran run --target=go` is not a
current CLI operation. These are fixture-specific results, not general backend
equivalence proofs or semantic `.zir` equality claims.

## Transcript interface

A record has exactly these required top-level fields:

- `schema_version`, `corpus_version`, `task_id`, `experiment`;
- `provenance`: `{kind, source, locator?}`;
- `attempts`: a nonempty array with zero-based consecutive indices.

Optional top-level `model` is `{provider, name, version, temperature?, seed?}`.
Unknown identity fields are `null`; omit the object when the source supplies no
model identity. Provenance kinds are
`synthetic_fixture`, `local_fixture`, `user_supplied` or `provider_export`.
They describe the source of data, never verification by this evaluator.
`fixtures/replay.json` is an executable, synthetic example without token counts.

Each attempt contains `index`, `events`, `edits`, optional `usage` and optional
`provider_usage`. An event is `{kind, text, provenance?}`; kinds are `prompt`,
`tool`, `completion` and `reasoning`. Preserve every supplied event in order,
including retry prompts/tool responses. Do not add reasoning text or counts
that the source did not supply. An empty event list is allowed for edit-only
records; it makes no full-transcript claim. Tool events are inert text and never
run as commands. Replay executes only the evaluator's fixed compiler pipeline.

An edit contains:

- `path`: relative `.zi` destination listed in the task's initial or solution map;
- `base_sha256`: SHA-256 of the complete pre-attempt file, or `null` for creation;
- `start_byte`, `end_byte`: zero-based, half-open UTF-8 byte offsets;
- exactly one of `replacement` (UTF-8 text, possibly empty) or
  `replacement_file` (a `.zi` artifact relative to `--artifacts`).

For deletion, use only `{path, base_sha256, delete: true}`. Artifacts default to
the fixture directory; supply `--artifacts PATH` for a separately supplied
transcript's `.zi` artifacts. Artifact paths cannot escape that root. Absolute
paths, traversal and symlinks are rejected. All spans in an attempt refer to
the same pre-attempt source. Stale hashes, overlaps, duplicate insertion
positions, out-of-range spans and split UTF-8 sequences reject the whole
attempt before changing any scratch file. Edits to unlisted files are rejected;
the final source file set must match the task goal. Non-overlapping adjacent spans work.

Accepted edits update only the scratch candidate, even if the compiler later
rejects it. A repair attempt must name the candidate's new digest. Rejected
edits leave every file unchanged. This is a local evaluation transport, not a
production transactional compiler-edit command. Recorded attempts are all
replayed; final success requires the last candidate to pass. The first passing
attempt is reported separately. Two replays of the same record and toolchain
produce identical JSON; scratch paths are normalized and no timestamps appear.

## Usage and result schema

`usage` contains only explicitly supplied per-attempt category observations:
`{category, count, unit, tokenizer, provenance, cache_policy}`.

- Categories: `prompt`, `cached_prompt`, `tool`, `completion`, `reasoning`,
  `total`. A category occurs at most once per attempt.
- `count` is a nonnegative integer and `unit` must be `tokens`.
- `tokenizer` is `{family, name, version}`; each unknown value is explicitly
  `null`. Never guess a tokenizer from a model name. Preserve supplied versions.
- Usage provenance must be `provider_export`, `user_supplied` or
  `synthetic_fixture`, with a source and optional locator.
- `cache_policy` is `included`, `excluded` or `not_reported` as supplied.

Optional `provider_usage` is `{provenance, payload}`. Its object payload preserves
the original provider usage fields verbatim, including vendor-specific details.
No unspecified cache, reasoning or usage fields are inferred from it.

The result includes schema/corpus versions, task and experiment identity,
provenance, supplied model/experiment metadata, compiler identity, `passed`,
ordered `attempts`, `usage` and `metrics`. Each attempt preserves events, edits
and raw provider usage, records edit acceptance or its error, the current file
digests, changed file list, and actual checker/law/diagnostic/execution observations. Reports may
contain supplied private text; stdout is the caller's responsibility to retain.

Usage observations retain the count, tokenizer and provenance unchanged.
Categories absent from supplied data and absent tokenizer families explicitly
report `unmeasured`. The family list is an inventory, not a claim to support or
have installed those tokenizers. `local_tokenization` is always `not_run`.
Synthetic fixture counts used inside validator regression tests never become
measurements. No token total is invented: categories may overlap, provider
reasoning may already be part of completion, and cache accounting varies.
`total_tokens` remains `null`; supplied per-attempt totals remain observations.

Metrics include attempt/retry count, first correct attempt (zero-based), attempts
before first pass, first-attempt edit acceptance, budget exhaustion and actual
compiler/tool invocation count. Event size is explicitly `utf8_bytes`, never
LLM tokens. Repair tokens, context quality and independent readability remain
`null` when unmeasured. This evaluator measures compiler correctness and replay
reliability; it does not claim model pass rates, cost savings or complete
transcript capture when the source did not supply them.

## Tool workflows and the frozen arms

Corpus 1.0.0 still declares `compact` and `semantic-edit` as `planned` transcript
arms, and replay accepts only the canonical baseline. Enabling them needs a
corpus revision with recorded model transcripts. The reserved optional
`experiment_metadata` fields `canonical_sha256`, `round_trip`, `source_maps`,
`document_versions` and `semantic_ids` are caller observations, not
compiler-verified results, and cannot enable an arm.

The compiler tools themselves are implemented: scoped `ziran api` retrieval,
schema-2 diagnostics, `ziran edit` and `ziran compact`/`expand`.
`tests/authoring_workflows.sh` runs them on these frozen tasks and writes a
separate report. See [`docs/AUTHORING.md`](../../docs/AUTHORING.md). This
evaluator's `local_tokenization` stays `not_run`. Do not count baseline or
fixture bytes as token savings.
