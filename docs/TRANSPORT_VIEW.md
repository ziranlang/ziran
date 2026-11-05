# Optional compact transport

`ziran compact [--map FILE.json] canonical.zi` writes an optional reversible
transport to stdout. `ziran expand [--map FILE.json] input.zic` writes the exact
canonical source bytes. Normal `ziran fmt` and the `.zi` grammar are unchanged;
compiler inputs never accept this transport. Keep `.zi` as reviewable authority.

The encoding starts with `ZI-COMPACT-1` and a newline. Literal `~` is `~~`.
Runs of eight or more identical spaces, tabs, LF or CR bytes become
`~sCOUNT;`, `~tCOUNT;`, `~nCOUNT;` or `~rCOUNT;`. Other UTF-8 bytes are literal.
The representation is deterministic and lossless, including comments, strings,
raw strings, payload whitespace and newlines. Expansion rejects unknown
versions/tags, leading-zero or short/noncanonical runs, split runs, incomplete
markers, count overflow and expansion above 16 MiB. Source must already equal
the ordinary formatter's output. Inputs are regular UTF-8 files without null
bytes; compact input is bounded at 32 MiB plus framing. No source is replaced.

The optional schema-1 map binds both documents by SHA-256, identifies UTF-8 byte
coordinates, and covers every source byte with consecutive half-open segments.
`literal` segments map offsets linearly. `escape` maps one tilde to two bytes.
`repeat` maps the complete canonical run to its marker; retain the offset within
the canonical run when mapping a diagnostic back, since a compressed marker
cannot physically contain a separate position for each expanded byte. Header
bytes have no source counterpart. Statements, declarations, expressions,
literals and trivia are all covered; expansion recovers original byte/line
coordinates exactly. Maps are bounded to 65536 segments and are written
atomically; linked/nonregular destinations are rejected.

Compact size is not a token count. Framing and escaped tildes can make some
files larger. No model tokenizer or model calls are involved, and no measured
token saving or authoring-quality improvement is claimed. A new concise source
language remains deferred pending the roadmap's measured quality/token gates.
