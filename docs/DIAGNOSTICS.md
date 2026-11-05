# Diagnostics and explanations

Compiler failures use a diagnostic code, source span, and message. With
`--diagnostics=json`, compiler tools, the package launcher, and formatter emit
one JSON object per line on their ordinary failure paths:

```json
{"schema_version":2,"severity":"error","code":"check.type","message":"...","path":"main.zi",
 "line":1,"column":1,"end_line":1,"end_column":1}
```

The code identity is the stable part. Messages and spans can improve; do not
parse human-readable message text when a code is present. Code names use a
phase prefix such as `parse`, `check`, `emit`, `module`, `package`, `law`,
`parallel`, `gpu`, `zx`, `zir`, `vm`, or `host`.

Query remediation with:

```sh
ziran explain check.slice_lifetime
ziran explain zir.noncanonical --json
ziran explain --list
ziran explain --list --json
ziran check --target=zx --diagnostics=json --root . main.zi
```

A single explanation has schema version 1:

```json
{"schema_version":1,"stability":"stable-code","code":"...",
 "summary":"...","next_step":"..."}
```

The list output has the same schema version and stability field, with a
`codes` array containing every code currently emitted by the compiler. The
`tests/explain.sh` conformance test scans compiler diagnostic calls and fails
if a new code is missing from the registry. Unknown codes exit nonzero rather
than receiving a guessed explanation.

Diagnostics use only current schema version 2 and `error` or `warning`
severity. Explanation objects retain their independent schema version 1.
Each diagnostic is a complete UTF-8 JSON line of at most 32768 bytes. Messages
retain at most 2048 input bytes at a UTF-8 boundary; `truncation.message` reports
clipping. Optional metadata is omitted as a complete field or structure if it
would exceed the record budget; `truncation.metadata` reports that omission.
Paths over 1024 bytes become null rather than a misleading shortened path.

Initializer, assignment, argument, and return mismatches include checked
`expected_type` and `actual_type` facts. Target restrictions supply `target`
and `capability` when known. `semantic_id` identifies the enclosing checked
function using the same signature-based identity as `ziran api`; null means
unavailable. `document` is the raw source's captured SHA-256 digest, byte count,
and `coordinate_space` (`source` or `rewritten_source`), or null for unavailable
source identity, including saved IR. These facts are not inferred from messages. `cause` is null when unavailable;
otherwise its fixed `kind` is `type_mismatch`, `unresolved_name`,
`unresolved_function`, or `target_restriction`, with a checked `subject` when
known. Facts are omitted only with explicit metadata truncation.

`related_graph` contains record-local `nodes` and directed `edges`. A checked
type requirement has primary node 0, declaration node 1, and a `requires_type`
edge; other relations may be null when the checker has only a related location.
Empty graphs mean no related fact is available.

Missing-name diagnostics include `suggested_name` when a close visible name
exists. `alternatives` groups independent repairs, each with an ID, message,
and an array of exact, non-overlapping `edits`. The current checker offers at
most one `replace_name` alternative. Each edit supplies zero-based half-open
UTF-8 byte offsets, one-based line/byte columns, `document_sha256`, `original`,
and `replacement`. Consumers must verify both the hash and preimage. Edits
are emitted only after the lexer identifies one unambiguous identifier and
the reread source matches the captured document. Strings, comments, ambiguous
occurrences, rewritten source without a raw mapping, and saved IR receive no
speculative edits. Replacements and preimages are never clipped to fit a budget.

Schema-1 diagnostic consumers must explicitly migrate to version 2:
`related` is now `related_graph`, and top-level `edits` is now
`alternatives[].edits`. No legacy diagnostic mode is retained.

`check --target=c|go|py|zx` checks target-specific foreign
identities/imports and Go/portable scalar-union restrictions before writing
output. Builds use the same preflight policy. Its failures include `target`
and a capability such as `ffi.go`, `types.go`, or `unions.scalar`; individual
lowerers and the portable verifier still check their remaining target limits.

`ZIRAN_DIAGNOSTICS=json` selects the same format. Format flags are read before
argument validation, so an earlier invalid option still honors a later JSON
flag. Invalid UTF-8 bytes in paths and subprocess logs are escaped; valid
Unicode is retained. Native C compilation captures and drains both output
streams in JSON mode, bounds the retained log, and reports truncation with
`zir_c.toolchain`. Allocation failure reporting does not allocate another
diagnostic buffer. `tests/tool_diagnostics.sh` covers these paths, missing
launcher tools, invalid command arguments, and formatter errors.

Arbitrary external package tools and fatal process signals can still produce
unstructured output. Consumers should
reject unknown schema versions and preserve diagnostic ordering. Missing
optional metadata means it is unavailable; consumers must not guess it from
the human-readable message.
