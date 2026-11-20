# ZX checked library format, revision 1

`ziran pkg zx pack-ir` writes `.zx` libraries for use by the package compiler.
The package driver verifies the library and supplies its checked modules to
the compiler by package identity. ZX runtime modules and applications use the
[executable format](zx-executable-format.md).

All integers are unsigned little-endian. The 16-byte header has four fields.

| Offset | Width | Field |
| --- | --- | --- |
| 0 | 4 | Bytes `ZXLB` |
| 4 | 4 | Format revision, `1` |
| 8 | 4 | Section count, 3 to 4,098 |
| 12 | 4 | Exact file length, up to 268,435,456 bytes |

Sections follow the header in increasing bytewise order by path. Each record
has a four-byte path length, a four-byte payload length, a 32-byte raw SHA-256
of the payload, and then the path and payload bytes. Records are contiguous.
Paths are unique, relative, and 1 to 1,024 bytes long. Paths cannot contain
empty, `.` or `..` components, control bytes below 32, a backslash or a colon.
Payloads are nonempty. The final payload ends at the declared file length.

The required `checked-package.tsv` section is the literal `2`, package
identity, toolchain identity, and profile, separated by tabs and terminated
by one newline. Both identities have the form `p` followed by 16 lowercase
hexadecimal digits. The profile is a registry path segment of up to 128
ASCII letters, digits, hyphens, underscores, or periods, excluding `.` and
`..`. The required `ziran.toml` section contains the provider manifest. All
remaining sections hold serialized checked modules with `.zir` paths derived
from the provider's module paths. At least one module is required.

The registry descriptor repeats the metadata fields and adds the full
lowercase hexadecimal SHA-256 of the entire `.zx` file as a fifth field.
The package driver matches these pins to the package and toolchain in the
resolved dependency graph. The compiler checks module identities against
that graph when it opens their checked payloads.

Development libraries use `ZXLD` as their magic. Their metadata starts with
`dev` and adds a fifth field containing a lowercase hexadecimal source
fingerprint. It covers the provider manifest and lock, scanned module and
export source bytes, and feature defines. The writer checks the fingerprint
again before installing the completed file. Production package and runtime
readers reject this magic. A local development reader must be selected
explicitly and still checks the package and toolchain identities.
