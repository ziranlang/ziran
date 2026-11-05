# Checked source edits

`ziran edit --root DIR request.json` previews one canonical `.zi` document.
`--apply` validates and replaces it; the default preview leaves it untouched.
Both return schema-1 JSON containing `path`, `applied`, and canonical `source`.
Errors use the current diagnostic schema with `--diagnostics=json`.

```json
{"schema_version":1,"path":"main.zi","document_sha256":"<64 lowercase hex digits>",
 "edits":[{"start_byte":45,"end_byte":47,"original":"41","replacement":"42"}]}
```

Offsets are zero-based, half-open UTF-8 byte positions in the original document,
so separate edits do not need to account for each other's length changes. A
span must contain the exact `original` and end on UTF-8 boundaries. The hash is
SHA-256 of the entire original file, including its exact newlines. Requests are
limited to 1 MiB, 256 edits and a 16 MiB document/candidate. Unknown fields,
duplicate keys, non-integer offsets, unsupported versions, overlapping ranges,
colliding insertions, no-op changes and whitespace-only token changes fail.

Instead of offsets, an edit can supply a checked `semantic_id` from `ziran api`:

```json
{"semantic_id":"zi:<64 hex digits>",
 "original":"Answer :: () -> s32 { return 41 }",
 "replacement":"Answer :: () -> s32 { return 42 }"}
```

This replaces one complete public function, type, constant or global declaration
in the selected document. The checked signature, document hash and exact whole
declaration preimage must all match. The lexer finds the declaration terminator;
an API declaration anchor alone is not a body range. A trailing semicolon after
a closing declaration brace is outside the replacement. Semicolon-free scalar
declarations without an unambiguous terminator, private/generated declarations,
rewritten raw-string coordinates and ambiguous IDs are explicitly rejected.
Use a verified span for cases with no available declaration range. Span edits
can repair an initially invalid program; semantic edits require its initial
checked graph.

Validation uses an immutable in-memory source overlay and the ordinary module
loader, canonical checker and law gates. It validates both the edited input and the formatter's output.
Other files are read in place. Source paths are relative to the selected root;
absolute paths, dot/traversal components and symlinks are rejected. Imports may
read within the root, the toolchain standard library, or up to 32 explicit
`--module-path DIR` / `--module-path NAME=DIR` search roots. Project package
maps remain authoritative for name resolution; pass their external dependency
roots explicitly to authorize those source reads. Up to 32 `--define NAME` flags
select the same source conditions during validation; use the flags that produced
the API identity. Root-escaping file
imports, parse/type/name collisions and stale hashes fail before replacement.

Application writes a private sibling file, syncs it, preserves the original
mode, rechecks the original inode/mode/bytes and atomically renames the complete
result. Write, sync, close, validation or rename failure removes the sibling
and leaves the original intact. Preview creates no source file. This is a
single-document transaction, not a multi-file transaction or an atomic compare
and swap against unrelated concurrent writers. Coordinate writers externally
when an absolute cross-process compare-and-swap guarantee is required.
