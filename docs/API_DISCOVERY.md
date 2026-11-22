# Checked API discovery

`ziran api --json --root DIR file.zi` loads and checks the input and its
transitive imports, then prints one bounded JSON object describing visible declarations
in each loaded module. The same command accepts saved `.zir` inputs. Use
`--module-path DIR` for import search roots and `--define NAME` for the same
conditional source selection as `ziran check`. A project
can use `ziran api --project --locked --json` to query its entry and imports.
Without `--json`, the command prints concise text signatures with source
locations. A failed load or check exits nonzero and does not print an API
object.

Use `--target c`, `--target go`, or `--target zx` to select and check the
API for a specific backend. `--target-os OS` and `--target-cpu CPU` set the
same conditional source facts as `ziran check`. Without an explicit
target, queries keep the portable ZX source configuration.

Projects use their pinned compiler by default. `ziran api --project --locked
--current-toolchain --target c --json` checks the locked source graph with
the API tool beside the current launcher. This allows documentation builds
to use the current API query options for packages with older toolchain pins.

The JSON object has `schema_version: 1` and a `modules` array. Each module has
`name`, `source`, `imports`, `constants`, `globals`, `types`, and `functions`.
Imports include their kind, alias/name, target, foreign target and symbol when
present, signature, and source location. Constants expose their checked value
text; globals expose their type and initializer text. A type includes its name,
checked body text, type parameters, record-template, enum, and procedure flags,
a procedure type's `return_type` (empty for other types), and source path
and line. A function includes its name, checked parameter signature, return
type, declaration defaults, inferred effect, `must_use`, template and host-use
flags, and source path and line. Empty `defaults` and `effect` strings mean the
corresponding information is absent. Consumers should reject unknown schema
versions and ignore new fields within a known version.

Only declarations visible across module boundaries appear. File/module-private
declarations, generated specializations, and compiler-generated default
argument helpers are omitted. The output lists each loaded module separately;
it does not flatten import names into the entry module. For example:

```sh
ziran api --json --root std std/json_scan.zi
```

The additive schema-1 retrieval fields include document identities, semantic
symbol IDs, checked ownership, module laws, exact known source anchors and
checked usage examples when supplied by a module. Use `ziran capabilities
--json` for target-wide limits and [`ziran explain`](DIAGNOSTICS.md) to decode
diagnostic codes.

## Checked usage examples

Put `library.zi.examples` beside the source module `library.zi`. The first
line is `ziran-api-examples-v1`. Each subsequent row has three fields separated
by literal tabs. Blank rows and rows beginning with `#` are ignored. LF and
CRLF are accepted.

```text
ziran-api-examples-v1
function	Scale	scale_usage.zi
type	Size	scale_usage.zi
```

The first field is the API `symbol_kind`. The second is the exact public name
or semantic `id` from the API. A name attaches to all its overloads. Use an
`id` for a specific overload. The last field is a sibling source basename
with a Ziran identifier as its stem, such as `scale_usage.zi`. That file must
directly import the documented module using its normal module import and
provide `#program_export` followed by `main :: () -> s32`. For example,
`scale_usage.zi` could contain:

```jai
#import "library"

#program_export
main :: () -> s32 {
    return Scale(2) - 4
}
```

An attached symbol in schema 1 has an `examples` array. Read entries in sidecar
order and interpret `path` relative to the physical directory of the module's
source. The same file appears once per symbol, even if both a name and an ID
match it. `entry` is the resolved `MODULE:main` entry, and `source` contains
the original UTF-8 source bytes.

```json
{"examples":[{"schema_version":1,"path":"scale_usage.zi","entry":"scale_usage:main","source":"#import \"library\"\n\n#program_export\nmain :: () -> s32 {\n    return Scale(2) - 4\n}\n"}]}
```

An absent or null `examples` field means no example was returned for that
symbol. Text, saved IR and `--imports-only` queries do not discover sidecars.
Ziran checks selected example sources with the query's module paths, package
map, target and defines, using the normal source ownership rules. It checks
the entry and its imports without running `main`.
To run the original source, pass `entry` to `ziran run --entry` and keep the
same project or import settings.

Version 1 allows up to 64 attachment rows per query and up to 16 KiB per
sidecar or example source. Sidecars in selected source modules must have valid
headers and selectors for the current checked configuration. Invalid sidecars
or failed checks of retrieved examples cause a nonzero exit with no API JSON.
Examples share their symbol's `--max-bytes` budget. If a symbol together with
its examples does not fit, the entire symbol is omitted and the result reports
truncation.

## Scoped retrieval and budgets

Use `--module NAME` to select one loaded module, `--symbol NAME` to select an
exact public declaration name (including all matching overloads), or
`--symbol ID` to retrieve its opaque semantic `id`. Both filters compose.
`--imports-only` returns the checked module import graph without declarations
or function bodies; `resolved_module` is the actual resolved module when known.
Compile-time file composition is not reconstructed as a separate graph when
ZIR has no import edge for it. Package module imports have one slash,
`#import "package/Module"`; relative source composition uses the explicit
`#import, file "path.zi"` form. Private helpers need not become public to use
that file form.
These symbol/import filters require `--json`.

```sh
ziran api --json --module json_scan --symbol JsonNext --max-bytes 8192 \
  --root std std/json_scan.zi
```

`--max-symbols N` defaults to 256 and accepts 1–4096. `--max-bytes N` defaults
to 65536 and accepts 1024–1048576. The byte limit includes JSON framing,
query and truncation metadata, and the final newline. Whole declarations are
omitted when needed; strings and JSON objects are never cut. An insufficient
budget for the module framing rejects with no API output; narrow `--module`
or increase the budget. Text output also respects the byte budget and rejects
rather than publishing a partial projection. Output byte counts are not token
counts.

`query` records the exact filter and requested limits. `truncation` reports
`matched_symbols`, `emitted_symbols`, `complete`, `symbol_limit` and `byte_limit`.
Imports, declarations and laws count as symbols. A missing name returns zero
matches; truncation never implies the omitted declarations do not exist.
Modules sort by checked module identity; declaration order within each kind
follows ZIR. Repeated equivalent invocations produce identical bytes.

Each declaration's `id` hashes a versioned, NUL-delimited checked identity:
module, declaration kind, name and relevant checked signature/type parameters.
Function bodies, document versions and incidental line positions do not change
that identity. Overloads have distinct signature identities. These IDs are
opaque retrieval keys, not compatibility promises across semantic signature
changes. Source and saved IR preserve the same semantic IDs and coverage.

`document` records `available`, `sha256` and `bytes` for the raw bytes captured
**before** source rewriting; the API does not reread files after checking.
Saved IR does not retain the raw source identity: its fields are false/null,
even when a same-named source file happens to exist. Hashes and line-offset
tables belong to the compiler context and are freed with it. Offset retention
is capped at 1 MiB per document and 8 MiB per context; a large document retains
its hash/length while its byte offsets remain null. Contexts hold at most 4096
distinct document versions; exceeding that limit rejects the parse explicitly.
Raw source bodies are not duplicated in this registry.

`span.kind: "declaration_anchor"` identifies the exact extent the checked
parser retained, which may be only a declaration header. It never claims a
whole body. Coordinates are one-based UTF-8 byte columns and half-open end
positions. If source rewriting changed byte positions (including `#string`),
`coordinate_space` is `rewritten_source` and raw byte offsets are explicitly
unavailable; no rewrite map is guessed. Otherwise it is `source`.
`start_byte`/`end_byte` are zero-based offsets, or null when source
identity/offsets are unavailable. The existing `path` and `line` fields remain.

`ownership` is derived from checked type identity. Function parameters and
results report `move` for owned storage, `borrow` for pointers/slices, `copy`
for copyable values, and `none` for void. Unresolved template ownership is
null. Type declarations report the corresponding value mode. Module `laws`
include checked claims and their evaluated status when available; unavailable
status is null. They explicitly carry `visibility: "module_contract"`: checked
IR has no separate law visibility field, so the API preserves those module
contracts without guessing public/private status from source. This is distinct
from exporting a procedure. Private declarations and generated helpers remain
omitted. Every entry also records its `symbol_kind`.
