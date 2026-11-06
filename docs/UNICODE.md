# Portable Unicode text

Import `U :: #import "std/unicode";`. The checked Ziran implementation and
its generated tables run on native C, Go and the portable VM. There are no
GLib, ICU, locale, Python or platform Unicode dependencies.

Public procedures:

```zi
CaseFold :: (text: string) -> string
Lower :: (text: string) -> string
LowerInto :: (text: string, output: *Vec(u8)) -> bool
NFCInto :: (text: string, output: *Vec(u8)) -> bool
IsLetter :: (codepoint: u32) -> bool
IsNumber :: (codepoint: u32) -> bool
IsWhiteSpace :: (codepoint: u32) -> bool
Capitalize :: (text: string) -> string
IsWord :: (codepoint: u32) -> bool
IsDecimal :: (codepoint: u32) -> bool
IsPrintable :: (codepoint: u32) -> bool
IsSpace :: (codepoint: u32) -> bool
IsWhitespace :: (text: string) -> bool
SplitWhitespace :: (text: string) -> Vec(string)
```

The case procedures return immutable strings with independent storage.
`CaseFold` applies full default, non-Turkic folding: `Straße` and `STRASSE`
both become `strasse`, final and ordinary sigma both become `σ`, `İ` becomes
`i` followed by U+0307, and `ﬃ` becomes `ffi`. It does not normalize text.

`Lower` applies full default lowercase mappings. Greek U+03A3 becomes final
sigma only when preceded by a cased character and not followed by one,
ignoring Case_Ignorable characters on both sides. Context uses the original
input, including for `Capitalize`. Locale-specific Turkish, Azeri and
Lithuanian tailoring is excluded, matching Python's locale-independent string
methods.

`LowerInto` applies the same mappings and contextual final sigma rule, with
strict UTF-8 input validation. `NFCInto` applies canonical recursive
decomposition, stable canonical combining-class ordering, and blocked
composition with the full composition exclusions. Hangul decomposition and
composition are algorithmic. Compatibility mappings remain untouched: `ﬃ`
and `①` are unchanged. NFC does not case-fold, lowercase, trim or tailor text.

Both Into procedures replace the previous bytes of a caller-owned `Vec(u8)`.
They succeed on empty input and preserve embedded NUL. Malformed UTF-8 or
allocation/size failure returns false with output count zero; backing capacity
may remain for reuse. They check all vector growth and impose no fixed text
limit beyond representable storage and available allocation. Input must not
borrow the output buffer. Storage belongs to the caller and remains valid
until it mutates or releases the vector. Existing immutable-string procedures
retain their replacement-on-malformed-input behavior.

`IsLetter` matches exactly the Unicode General_Category aggregate L (Lu, Ll,
Lt, Lm, Lo). `IsNumber` matches exactly N (Nd, Nl, No), including fractions,
superscripts and Roman numerals. Neither includes combining marks or ASCII
underscore. `IsWhiteSpace` uses Unicode's White_Space property (25 scalars);
it excludes the U+001C..U+001F controls accepted by Python-style `IsSpace`.
All three return false for surrogates and values above U+10FFFF.

`Capitalize` titlecases the first scalar, including full expansions, and
lowercases subsequent scalars. Thus `ǆABC` becomes `ǅabc`, `ßABC` becomes
`Ssabc` and `ǰABC` becomes `J` + U+030C + `abc`. An initial combining mark,
punctuation or digit still occupies the first position; capitalization never
searches ahead for a letter. Empty input produces empty output. Invalid UTF-8
follows `std/utf8.Decode`: consume one malformed lead byte and emit U+FFFD.
Embedded NUL is ordinary data. Scanning uses 64-bit byte positions and bounded
UTF-8 windows.

`IsWord` matches Python Unicode regex `\w`: letters (categories Lu, Ll, Lt,
Lm and Lo), decimal/digit/numeric characters and ASCII underscore. It includes
superscripts, fractions, Roman numerals and non-ASCII scripts. Combining marks,
other connector punctuation, join controls and emoji do not qualify.

`IsDecimal` matches Python Unicode regex `\d`: only General_Category Nd.
ASCII, Arabic-Indic, fullwidth and mathematical decimal digits qualify;
superscript digits, fractions and Roman numerals do not.

`IsPrintable` uses Python's printability rule with the pinned Unicode 17.0.0
database: categories L (letter), M (mark), N (number), P (punctuation) and
S (symbol), plus ASCII U+0020 space. It preserves combining marks and emoji.
Other Z (separator) and all C (other) categories are excluded, including NBSP,
soft hyphen, zero-width joiner, bidi controls, line/paragraph separators,
private-use, unassigned values, noncharacters and surrogates. Values above
U+10FFFF return false. This is a text property, independent of font support.
Python versions with older Unicode assignments can differ; equivalence to
their historical assigned-character repertoire is not claimed.

A consumer that filters text and applies a length limit should test
`IsPrintable` first and count retained Unicode scalars. For example, `é🙂x`
with limit 2 produces `é🙂`; removing U+200D and U+00A0 from `a\u200db\u00a0c` produces
`abc`; `a` + U+0301 with limit 1 produces `a`. Byte lengths and grapheme
counts give different results. The focused suite tests this composition.

`IsSpace` matches Python `str.isspace` and Unicode regex `\s`: general category
Zs or bidirectional class WS, B or S. This includes U+001C through U+001F in
addition to Unicode's White_Space property. U+180E, U+200B and U+FEFF are not
whitespace. `IsWhitespace` requires nonempty text consisting entirely of such
scalars; invalid UTF-8 returns false. Property predicates return false for
surrogates and values above U+10FFFF.

`SplitWhitespace` matches unlimited Python `str.split()` with no separator.
It collapses runs, omits leading/trailing empty fields, and returns no fields
for empty or all-whitespace input. The returned vector owns its descriptors;
each string borrows the original input bytes. Keep that input alive. These
operations leave the existing ASCII helpers in `std/text` and `std/text_tools`
with their documented byte semantics.

## Unicode version and regeneration

Behavior uses Unicode **17.0.0**, the same version as Ziran's grapheme tables.
Python releases using a different Unicode database can differ for newly
assigned or updated characters. UTF-8 strings contain scalars, so Python's
isolated-surrogate string behavior is outside this API's input model.

The canonical tables live in `std/unicode_data.zi`. Regenerate them and the
unpacked expected-value corpus with the native Ziran generator:

```sh
build-queue -- build/bin/ziran run --target=c scripts/unicode_data.zi -- \
    build/unicode-ucd std/unicode_data.zi tests/spec/unicode_conformance.zi
```

Supply these official files from
<https://www.unicode.org/Public/17.0.0/ucd/> in the input directory. Builds and
tests use the checked-in outputs and require no downloads. Unicode data is
distributed under [Unicode License V3](../LICENSE-Unicode).

```text
UnicodeData.txt          2e1efc1dcb59c575eedf5ccae60f95229f706ee6d031835247d843c11d96470c
SpecialCasing.txt        efc25faf19de21b92c1194c111c932e03d2a5eaf18194e33f1156e96de4c9588
CaseFolding.txt          ff8d8fefbf123574205085d6714c36149eb946d717a0c585c27f0f4ef58c4183
DerivedCoreProperties.txt 24c7fed1195c482faaefd5c1e7eb821c5ee1fb6de07ecdbaa64b56a99da22c08
```

The generator reads simple UnicodeData mappings, unconditional SpecialCasing
overrides, C/F CaseFolding entries, and Cased/Case_Ignorable properties.
Final_Sigma is implemented in the portable algorithm. Compact fixed-width
base-128 records keep generated literals valid UTF-8 and allow binary search;
each literal contains at most 448 bytes. Generated tests store expected UTF-8
strings independently of that packed representation.

## Local tests

Canonical normalization and exact L/N/White_Space use the separately generated
`std/unicode_normalization_data.zi`; the existing case table/generator is
unchanged. Regenerate with:

```sh
build-queue -- build/bin/ziran run --target=c scripts/unicode_normalization_data.zi -- \
    build/unicode-ucd std/unicode_normalization_data.zi tests/spec/unicode_normalization_cases.zi
```

The additional official Unicode 17.0.0 inputs and SHA-256 digests are:

```text
CompositionExclusions.txt 2f239196ef3b5b61db5cc476e9bd80f534d15aa1b74e1be1dea5d042a344c85f
PropList.txt              130dcddcaadaf071008bdfce1e7743e04fdfbc910886f017d9f9ac931d8c64dd
NormalizationTest.txt     5019ffd530751a741900c849c0e010332f142a3612234639bd200b82138a87db
```

The generated test corpus preserves all five UTF-8 columns of the primary
[NormalizationTest](https://www.unicode.org/Public/17.0.0/ucd/NormalizationTest.txt).
The normalization suite tests its five NFC invariants for every row and
identity normalization for every assigned scalar absent from Part 1, as
required by UAX #15. Source and saved IR execute on C, Go and both portable
bundle runners. Focused cases cover strict malformed UTF-8, empty/reused
outputs, NUL, recursive/blocked composition, Hangul, lowercase context and
the distinction between Python whitespace and Unicode White_Space.

```sh
build-queue -- sh tests/std_unicode_normalization.sh build/bin/ziran
```

Set `ZIRAN_UNICODE_UCD=build/unicode-ucd` to also regenerate and compare both
normalization outputs byte for byte. Normal tests need no network access.

```sh
build-queue -- sh tests/std_unicode.sh build/bin/ziran
build-queue -- sh tests/unicode_data.sh build/bin/ziran
```

The Unicode suite executes source and saved IR on native C, Go, the portable
VM (`zi2zx run --vm`) and the native bundle runner, comparing source/saved bundles.
It covers every changed fold/lower/title mapping and every word, space, decimal
and printability range's endpoints (8,323 generated assertions), plus contextual
sigma, combining and titlecase expansions, supplementary scalars, splitting, malformed UTF-8,
embedded NUL and result lifetimes. The generator suite uses small official-format
fixtures without downloads. With `ZIRAN_UNICODE_UCD=build/unicode-ucd`, it also
regenerates both canonical outputs and compares them byte for byte.

`std/utf8.DecodeReplace(source: []u8) -> string` is a separate whole-input
UTF-8 replacement decoder. It substitutes U+FFFD once per maximal valid prefix
of an ill-formed sequence, as Python UTF-8 decoding with `errors='replace'`
does. A truncated `E2 82` or `F0 9F 92` yields one replacement; `E1 80 41`
yields replacement + `A`. Invalid starts or forbidden second bytes are
replaced separately: `C0 AF` yields two replacements, `ED A0 80` and
`E0 80 80` yield three, and `F4 90 80 80` yields four. The immutable result
has independent storage, is always valid UTF-8, and preserves valid scalars
and embedded NUL. This does not alter the existing `Decode` or bounded `Copy`
one-malformed-lead-byte contracts. Run `build-queue -- sh tests/std_utf8.sh
build/bin/ziran` and `build-queue -- sh tests/utf8_copy_valid.sh
build/bin/ziran` for the new replacement matrix and the original repair-copy
regression suite.

Mapping semantics follow [Unicode SpecialCasing](https://www.unicode.org/Public/17.0.0/ucd/SpecialCasing.txt)
and [CaseFolding](https://www.unicode.org/Public/17.0.0/ucd/CaseFolding.txt).
The Python equivalents are documented in
[string methods](https://docs.python.org/3/library/stdtypes.html#string-methods)
and [Unicode regular expression escapes](https://docs.python.org/3/library/re.html#regular-expression-syntax).
