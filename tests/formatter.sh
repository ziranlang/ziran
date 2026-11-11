#!/bin/sh
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$repo/build"
work=$(mktemp -d "$repo/build/formatter.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM

${CC:-cc} -std=c11 -D_GNU_SOURCE -I"$repo/include" -I"$repo/cmd/zir" \
    -I"$repo/build/compiler-ir" "$repo/tests/law_format_test.c" \
    "$repo/build/libziran.a" -o "$work/law-format-test" -lm -pthread -ldl

"$ziran" run --target=c --root "$repo/tests" "$repo/tests/formatter_cases.zi" -- "$ziran" "$work"

# The scanner runs from source and saved IR with no native host imports.
cat > "$work/scanner.zi" <<'ZI'
#import "format_source"
#import "std/vec"

Same :: (source: string, expected: string) -> bool {
    formatted := FormatSource(source)
    result := TextView(VecSlice(formatted, 0, formatted.count)) == expected
    VecFree(formatted)
    return result
}

#program_export
Answer :: () -> s32 {
    if !Same("", "") { return 1 }
    if !Same("  \t\r\n", "\n") { return 2 }
    if !Same("a::() {\nb=2\n}\n", "a :: () {\n    b = 2\n}\n") { return 3 }
    if !Same("f::() {\n// } ) = ::\na=1\n}\n",
        "f :: () {\n    // } ) = ::\n    a = 1\n}\n") { return 4 }
    if !Same("f::() {\n/* } ( :: = */\na=1\n}\n",
        "f :: () {\n    /* } ( :: = */\n    a = 1\n}\n") { return 5 }
    if !Same("a=\"} ( :: = \\\"\"\n", "a = \"} ( :: = \\\"\"\n") { return 6 }
    if !Same("if a!=b && a<=b && a>=b && a==b { a+=b; a*=b }\n",
        "if a!=b && a<=b && a>=b && a==b { a+=b; a*=b }\n") { return 7 }
    if !Same("Text::#string END\n  bytes = :: { (   \nEND;\na=1\n",
        "Text :: #string END\n  bytes = :: { (   \nEND;\na = 1\n") { return 8 }
    if !Same("a=\"first\n  second = :: {\"\nb=2\n",
        "a = \"first\n  second = :: {\"\nb = 2\n") { return 9 }
    if !Same("f::(\nx: s32\n) {\nreturn x\n}\n",
        "f :: (\n    x: s32\n    ) {\n    return x\n}\n") { return 10 }
    if !Same("/* nested /* } */ ( */\na=1\n", "/* nested /* } */ ( */\na = 1\n") { return 11 }
    if !Same("a=1", "a = 1\n") { return 12 }
    if !Same("#law N forall x: 0..1,\ny: 0..1 =>\nx == y ||\nx != y;\nAfter :: 42\n",
        "#law N forall x: 0..1,\n    y: 0..1 =>\n    x == y ||\n    x != y;\nAfter :: 42\n") { return 13 }
    if !Same("#law N custom \";#law\" == \";#law\" &&\ntrue;\nAfter :: 42\n",
        "#law N custom \";#law\" == \";#law\" &&\n    true;\nAfter :: 42\n") { return 14 }
    return 42
}

Run :: () { print("%\n", Answer()) }
ZI

"$ziran" ir --root "$work" --module-path "$repo/cmd" \
    -o "$work/ir" "$work/scanner.zi"
for input in "$work/scanner.zi" "$work/ir/scanner.zir"; do
    form=source
    case "$input" in *.zir) form=saved ;; esac
    "$ziran" bundle --root "$work" --module-path "$repo/cmd" \
        --entry scanner:Answer -o "$work/scanner.zx" "$input"
    test "$("$ziran" run "$work/scanner.zx")" = 42
    output="$work/$form"
    "$ziran" build --target=c --exe --entry scanner:Run --root "$work" \
        --module-path "$repo/cmd" -o "$output/c" "$input"
    test "$("$output/c/scanner")" = 42
    "$ziran" build --target=go --exe --entry scanner:Run --pkg main \
        --root "$work" --module-path "$repo/cmd" -o "$output/go" "$input"
    test "$(GO111MODULE=off go run "$output/go/"*.go)" = 42
done

# Rebuild the entire native command from checked IR as a bootstrap gate.
"$ziran" ir --root "$repo/cmd" --module-path "$repo/std" \
    -o "$work/format-ir" "$repo/cmd/format.zi"
"$ziran" build --target=c --entry format:main --root "$repo/cmd" \
    --module-path "$repo/std" -o "$work/format-c" "$work/format-ir/format.zir"
${CC:-cc} -std=c11 -D_GNU_SOURCE -I"$repo/include" -I"$work/format-c" \
    "$work/format-c/"*.c -o "$work/zi-fmt" -lm
"$work/zi-fmt" --check "$work/source.zi"
cp "$work/source.zi" "$work/rebuilt.zi"
"$work/zi-fmt" "$work/rebuilt.zi"
cmp "$work/source.zi" "$work/rebuilt.zi"

# An unknown option must name the offending flag in its diagnostic.
if "$work/zi-fmt" --bogus "$work/source.zi" >"$work/option.out" 2>"$work/option.err"; then
    echo 'unknown formatter option was accepted' >&2
    exit 1
fi
test ! -s "$work/option.out"
grep -Fq -- '--bogus' "$work/option.err"
if "$work/zi-fmt" --diagnostics=json --bogus "$work/source.zi" \
    >"$work/option-json.out" 2>"$work/option-json.err"; then
    echo 'unknown formatter option was accepted in JSON mode' >&2
    exit 1
fi
grep -Fq '"command.arguments"' "$work/option-json.err"
grep -Fq -- '--bogus' "$work/option-json.err"
