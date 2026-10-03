# Diagnostics and explanations

Compiler failures use a diagnostic code, source span, and message. With
`--diagnostics=json`, supported compiler paths emit one JSON object per line:

```json
{"schema_version":1,"severity":"error","code":"check.type","message":"...","path":"main.zi",
 "line":1,"column":1,"end_line":1,"end_column":1}
```

The code identity is the stable part. Messages and spans can improve; do not
parse human-readable message text when a code is present. Code names use a
phase prefix such as `parse`, `check`, `emit`, `module`, `package`, `law`,
`parallel`, `gpu`, `zib`, `zir`, `vm`, or `host`.

Query remediation with:

```sh
ziran explain check.slice_lifetime
ziran explain zir.noncanonical --json
ziran explain --list
ziran explain --list --json
ziran check --target=zib --diagnostics=json --root . main.zi
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

Diagnostics use schema version 1 and `error` or `warning` severity. Initializer,
assignment, argument, and return mismatches include `expected_type` and
`actual_type`. A `related` array names
declarations with their paths, source spans, and messages. Target restrictions
can supply `target` and `capability` metadata. Missing-name diagnostics include
`suggested_name` when a close visible name exists, and an `edits` array only
when the source lexer confirms an unambiguous identifier on the reported line.
Each edit includes a span, `original`, `replacement`, and `message`. Consumers
must verify `original` against the current document before applying an edit.
Strings, comments, ambiguous occurrences, and source rewritten after checking
must never be changed by a guessed edit.

`check --target=c|cpp|go|rust|py|zib|plan9-c` checks target-specific foreign
identities/imports and Go/portable scalar-union restrictions before writing
output. Builds use the same preflight policy. Its failures include `target`
and a capability such as `ffi.go`, `types.py`, or `unions.scalar`; individual
lowerers and the portable verifier still check their remaining target limits.

Some loading and tool failures can still be plain text. Consumers should
reject unknown schema versions and preserve diagnostic ordering. Missing
optional metadata means it is unavailable; consumers must not guess it from
the human-readable message.
