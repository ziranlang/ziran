#!/bin/sh
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

python3 - "$ziran" "$work" <<'PY'
from pathlib import Path
import os
import stat
import subprocess
import sys

ziran = Path(sys.argv[1]).resolve()
formatter = ziran.with_name("zi-fmt")
work = Path(sys.argv[2])

def run(*arguments, expected=0):
    result = subprocess.run([str(formatter), *map(str, arguments)],
                            capture_output=True, timeout=10)
    assert result.returncode == expected, (arguments, result.returncode, result.stderr)
    return result

# Every string and comment below contains bytes that confused the old
# line-based spacing or brace counter. The program must still return 42.
source = work / "source.zi"
source.write_bytes(b'''// } ) :: = #string COMMENT
Same::(a: string, b: string)->bool { return a == b }
Echo::(value: string)->string { return value }
Answer::()->s32 {
/* } ) = :: \" #string BLOCK
   /* nested { ( => */
   trailing comment bytes = ::\x20\x20\x20
*/
value: string=\"} ( :: = \\\"\" // } ) \" :: =
if !Same(value, \"} ( :: = \\\"\") { return 0 }
if !Same(Echo(#string FIRST
  { } ( ) :: = \" \\\x20\x20\x20
FIRST), Echo(#string SECOND
  { } ( ) :: = \" \\\x20\x20\x20
SECOND)) { return 0 }
// END_EXTRA is content, not a closing delimiter.
raw: string = #string END
END_EXTRA
  body = :: { )\x20\x20\x20
END;
if !Same(raw, \"END_EXTRA\\n  body = :: { )   \\n\") { return 0 }
return 42 // { ( = ::
}
''')
original = source.read_bytes()
assert run("--check", source, expected=1).stderr.startswith(b"zi-fmt: would reformat ")
assert source.read_bytes() == original
source.chmod(0o751)
run(source)
formatted = source.read_bytes()
assert b"    value: string = " in formatted
assert b"    return 42 // { ( = ::\n}" in formatted
assert b"   trailing comment bytes = ::   \n" in formatted
assert b"FIRST), Echo(#string SECOND\n  { } ( ) :: = \" \\   \nSECOND))" in formatted
assert stat.S_IMODE(source.stat().st_mode) == 0o751
run("--check", source)
before = source.stat()
run(source)
assert source.read_bytes() == formatted
assert source.stat().st_ino == before.st_ino
assert source.stat().st_mtime_ns == before.st_mtime_ns

for name, content in [("original", original), ("formatted", formatted)]:
    path = work / (name + ".zi")
    path.write_bytes(content)
    bundle = work / (name + ".zib")
    subprocess.run([str(ziran), "bundle", "--root", str(work), "--entry", name + ":Answer",
                    "-o", str(bundle), str(path)], check=True)
    assert subprocess.check_output([str(ziran), "run", str(bundle)]).strip() == b"42"

# Following a symlink must preserve the link and update the source it names.
target = work / "target.zi"
target.write_bytes(b"Answer::()->s32 {\nreturn 42\n}\n")
link = work / "linked.zi"
link.symlink_to(target.name)
run(link)
assert link.is_symlink()
assert b"    return 42" in target.read_bytes()
run("--check", link)

# The temporary name must fit even when the source basename is already at
# the filesystem's component limit.
long_name = work / ("x" * 252 + ".zi")
long_name.write_bytes(b"Answer::()->s32 { return 42 }\n")
run(long_name)
run("--check", long_name)

empty = work / "empty.zi"
empty.touch()
inode = empty.stat().st_ino
run(empty)
assert empty.read_bytes() == b"" and empty.stat().st_ino == inode

newline = work / "newline.zi"
newline.write_bytes(b"Answer :: () -> s32 { return 42 }")
run(newline)
assert newline.read_bytes().endswith(b"\n")

# A failed path must not prevent later arguments from being formatted.
newline.write_bytes(b"Answer::()->s32 { return 42 }")
run(work / "missing.zi", newline, expected=1)
run("--check", newline)
run(work, expected=1)
fifo = work / "pipe.zi"
os.mkfifo(fifo)
run(fifo, expected=1)
run(expected=2)
assert not list(work.glob("*.zi-fmt.*"))

# A failed replacement leaves the original bytes intact and removes the
# temporary file. This cannot be exercised when running as root.
if os.geteuid() != 0:
    readonly = work / "readonly"
    readonly.mkdir()
    path = readonly / "app.zi"
    path.write_bytes(b"Answer::()->s32 { return 42 }\n")
    before = path.read_bytes()
    readonly.chmod(0o555)
    try:
        run(path, expected=1)
        assert path.read_bytes() == before
        assert not list(readonly.glob("*.zi-fmt.*"))
    finally:
        readonly.chmod(0o755)
PY

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
        --entry scanner:Answer -o "$work/scanner.zib" "$input"
    test "$("$ziran" run "$work/scanner.zib")" = 42
    output="$work/$form"
    "$ziran" build --target=c --exe --entry scanner:Run --root "$work" \
        --module-path "$repo/cmd" -o "$output/c" "$input"
    test "$("$output/c/scanner")" = 42
    "$ziran" build --target=cpp --root "$work" --module-path "$repo/cmd" \
        -o "$output/cpp" "$input"
    printf '#include "scanner.hpp"\nint main() { scanner_Run(); }\n' > "$output/cpp/run.cpp"
    ${CXX:-c++} -std=c++17 -I"$repo/include" -I"$output/cpp" \
        "$output/cpp/"*.cpp -o "$output/cpp/app"
    test "$("$output/cpp/app")" = 42
    "$ziran" build --target=go --exe --entry scanner:Run --pkg main \
        --root "$work" --module-path "$repo/cmd" -o "$output/go" "$input"
    test "$(GO111MODULE=off go run "$output/go/"*.go)" = 42
    "$ziran" build --target=py --exe --entry scanner:Run --root "$work" \
        --module-path "$repo/cmd" -o "$output/py" "$input"
    test "$(python3 "$output/py")" = 42
    if command -v cargo >/dev/null 2>&1; then
        "$ziran" build --target=rust --exe --entry scanner:Run --root "$work" \
            --module-path "$repo/cmd" -o "$output/rust" "$input"
        CARGO_TARGET_DIR="$work/rust-target" cargo build --quiet \
            --manifest-path "$output/rust/Cargo.toml"
        test "$("$work/rust-target/debug/ziran_generated")" = 42
    fi
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
