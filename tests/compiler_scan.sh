#!/bin/sh
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
bin=$(CDPATH= cd -- "$(dirname -- "$ziran")" && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/scanner.zi" <<'ZI'
#import "compiler_scan"
#import "std/vec"

One :: (source: string, kind: TokenKind, text: string, begin: s64,
    line: s32, column: s32, after_line: s32, after_column: s32) -> bool {
    token := NextToken(source, 0, 1, 1)
    return token.kind == kind && token.begin == begin &&
    source[token.begin:token.end] == text && token.line == line &&
    token.column == column && token.after_line == after_line &&
    token.after_column == after_column
}

#program_export
Answer :: () -> s32 {
    if !One("", .Eof, "", 0, 1, 1, 1, 1) { return 1 }
    if !One(" \t\r\n\v\f ", .Eof, "", 7, 2, 4, 2, 4) { return 2 }
    if !One("\t\n _name9", .Ident, "_name9", 3, 2, 2, 2, 8) { return 3 }
    if !One("#run", .Directive, "#run", 0, 1, 1, 1, 5) { return 4 }
    if !One("#", .Directive, "#", 0, 1, 1, 1, 2) { return 5 }
    if !One("0xdeadbeef", .Int, "0xdeadbeef", 0, 1, 1, 1, 11) { return 6 }
    if !One("1_234", .Int, "1_234", 0, 1, 1, 1, 6) { return 7 }
    if !One(".25e+2", .Float, ".25e+2", 0, 1, 1, 1, 7) { return 8 }
    if !One("0x1.fp-2", .Float, "0x1.fp-2", 0, 1, 1, 1, 9) { return 9 }
    if !One("1e-2", .Float, "1e-2", 0, 1, 1, 1, 5) { return 10 }
    if !One("0x1e+2", .Int, "0x1e", 0, 1, 1, 1, 5) { return 11 }
    if !One("1bad_name", .Float, "1bad_name", 0, 1, 1, 1, 10) { return 12 }
    if !One("\"a\\\"b\"", .String, "\"a\\\"b\"", 0, 1, 1, 1, 7) { return 13 }
    if !One("'\\''", .Char, "'\\''", 0, 1, 1, 1, 5) { return 14 }
    if !One("\"a\nb\"", .String, "\"a\nb\"", 0, 1, 1, 2, 3) { return 15 }
    if !One("\"unfinished\\", .String, "\"unfinished\\", 0, 1, 1, 1, 13) { return 16 }
    if !One("<<=", .Operator, "<<=", 0, 1, 1, 1, 4) { return 17 }
    if !One(">>=", .Operator, ">>=", 0, 1, 1, 1, 4) { return 18 }
    if !One("::", .Operator, "::", 0, 1, 1, 1, 3) { return 19 }
    if !One(":=", .Operator, ":=", 0, 1, 1, 1, 3) { return 20 }
    if !One(":", .Punct, ":", 0, 1, 1, 1, 2) { return 21 }
    if !One(".", .Operator, ".", 0, 1, 1, 1, 2) { return 22 }
    if !One("@", .Unknown, "@", 0, 1, 1, 1, 2) { return 23 }
    if !One("\x7f", .Unknown, "\x7f", 0, 1, 1, 1, 2) { return 24 }
    if !One("\0ignored", .Eof, "", 0, 1, 1, 1, 1) { return 25 }
    if KindName(.Eof) != "eof" || KindName(.Ident) != "ident" ||
    KindName(.Int) != "int" || KindName(.Float) != "float" ||
    KindName(.String) != "string" || KindName(.Char) != "char" ||
    KindName(.Directive) != "directive" || KindName(.Operator) != "operator" ||
    KindName(.Punct) != "punct" || KindName(.Unknown) != "unknown" ||
    KindName(cast(TokenKind)100) != "unknown" { return 26 }
    source := "#run Sum(0xdeadbeef, .25e+2) <<= 1\n"
    texts: [10]string = .{"#run", "Sum", "(", "0xdeadbeef", ",", ".25e+2", ")", "<<=", "1", ""}
    kinds: [10]TokenKind = .{.Directive, .Ident, .Punct, .Int, .Punct, .Float, .Punct, .Operator, .Int, .Eof}
    at: s64 = 0
    line: s32 = 1
    column: s32 = 1
    index: s32 = 0
    while index < 10 {
        token := NextToken(source, at, line, column)
        if token.kind != kinds[index] || source[token.begin:token.end] != texts[index] {
            return 27
        }
        at = token.end
        line = token.after_line
        column = token.after_column
        index += 1
    }
    if line != 2 || column != 1 { return 28 }
    again := NextToken(source, at, line, column)
    if again.kind != .Eof || again.end != at || again.after_line != line ||
    again.after_column != column { return 29 }
    return 42
}

// A reproducible corpus stresses short-input lookahead, multiline literals,
// malformed numbers, every ASCII operator, and unknown bytes on all targets.
#program_export
Fingerprint :: () -> s64 {
    alphabet := "abcXYZ_0129eEpPxX.\"'\\#{}()[],;:+-*/%=!<>&|^?~@ \t\n\r\v\f\x7f"
    state: u32 = 12345
    result: u32 = 2166136261
    sample: s32 = 0
    while sample < 256 {
        bytes: Vec(u8)
        length: s32 = sample % 65
        index: s32 = 0
        while index < length {
            state = state * cast(u32)1664525 + cast(u32)1013904223
            VecPush(bytes, alphabet[cast(s64)(state >> 16) % cast(s64)alphabet.count])
            index += 1
        }
        source := TextView(VecSlice(bytes, 0, bytes.count))
        at: s64 = 0
        line: s32 = 1
        column: s32 = 1
        while true {
            token := NextToken(source, at, line, column)
            result = result * cast(u32)16777619 ^ cast(u32)token.kind
            result = result * cast(u32)16777619 ^ cast(u32)token.begin
            result = result * cast(u32)16777619 ^ cast(u32)token.end
            result = result * cast(u32)16777619 ^ cast(u32)token.line
            result = result * cast(u32)16777619 ^ cast(u32)token.column
            result = result * cast(u32)16777619 ^ cast(u32)token.after_line
            result = result * cast(u32)16777619 ^ cast(u32)token.after_column
            if token.kind == .Eof { break }
            if token.end <= at { return 0 }
            at = token.end
            line = token.after_line
            column = token.after_column
        }
        VecFree(bytes)
        sample += 1
    }
    return cast(s64)result
}

Run :: () { print("% %\n", Answer(), Fingerprint()) }
ZI

"$ziran" ir --root "$work" --module-path "$repo/cmd" -o "$work/ir" "$work/scanner.zi"
"$ziran" bundle --root "$work" --module-path "$repo/cmd" --entry scanner:Answer \
    -o "$work/answer.zib" "$work/scanner.zi"
test "$("$ziran" run "$work/answer.zib")" = 42
"$ziran" bundle --root "$work" --module-path "$repo/cmd" --entry scanner:Fingerprint \
    -o "$work/fingerprint.zib" "$work/scanner.zi"
expected="42 $("$ziran" run "$work/fingerprint.zib")"

for input in "$work/scanner.zi" "$work/ir/scanner.zir"; do
    form=source
    case "$input" in *.zir) form=saved ;; esac
    output="$work/$form"
    for entry in Answer Fingerprint; do
        "$ziran" bundle --root "$work" --module-path "$repo/cmd" --entry scanner:$entry \
            -o "$work/$entry.zib" "$input"
        case "$entry" in
            Answer) test "$("$ziran" run "$work/$entry.zib")" = 42 ;;
            Fingerprint) test "42 $("$ziran" run "$work/$entry.zib")" = "$expected" ;;
        esac
    done
    "$ziran" build --target=c --exe --entry scanner:Run --root "$work" \
        --module-path "$repo/cmd" -o "$output/c" "$input"
    test "$("$output/c/scanner")" = "$expected"
    "$ziran" build --target=cpp --root "$work" --module-path "$repo/cmd" \
        -o "$output/cpp" "$input"
    printf '#include "scanner.hpp"\nint main() { scanner_Run(); }\n' > "$output/cpp/run.cpp"
    ${CXX:-c++} -std=c++17 -I"$repo/include" -I"$output/cpp" \
        "$output/cpp/"*.cpp -o "$output/cpp/app"
    test "$("$output/cpp/app")" = "$expected"
    "$ziran" build --target=go --exe --entry scanner:Run --pkg main --root "$work" \
        --module-path "$repo/cmd" -o "$output/go" "$input"
    test "$(GO111MODULE=off go run "$output/go/"*.go)" = "$expected"
    "$ziran" build --target=py --exe --entry scanner:Run --root "$work" \
        --module-path "$repo/cmd" -o "$output/py" "$input"
    test "$(python3 "$output/py")" = "$expected"
    if command -v cargo >/dev/null 2>&1; then
        "$ziran" build --target=rust --exe --entry scanner:Run --root "$work" \
            --module-path "$repo/cmd" -o "$output/rust" "$input"
        CARGO_TARGET_DIR="$work/rust-target" cargo build --quiet \
            --manifest-path "$output/rust/Cargo.toml"
        test "$("$work/rust-target/debug/ziran_generated")" = "$expected"
    fi
done

# Checked-in bootstrap output must agree with both the bootstrapped compiler
# and checked IR, so fresh builds cannot silently use a stale lexical policy.
"$ziran" ir --root "$repo/cmd" -o "$work/scan-ir" "$repo/cmd/compiler_scan.zi"
for input in "$repo/cmd/compiler_scan.zi" "$work/scan-ir/compiler_scan.zir"; do
    "$bin/zi2c" --no-main --root "$repo/cmd" -o "$work/scan-c" "$input"
    cmp "$repo/bootstrap/compiler_scan/compiler_scan.c" "$work/scan-c/compiler_scan.c"
    cmp "$repo/bootstrap/compiler_scan/compiler_scan.h" "$work/scan-c/compiler_scan.h"
done

# Exercise the actual C frontend boundary, including truncation, borrowed
# source lifetime, lookahead copies, null input, and interned source spans.
${CC:-cc} ${VM_CFLAGS:-} -std=c11 -D_GNU_SOURCE -I"$repo/include" -I"$repo/cmd/zir" \
    "$repo/tests/compiler_scan_test.c" "${ZIRAN_LIB:-$repo/build/libziran.a}" \
    -o "$work/lexer"
"$work/lexer"
