#!/bin/sh
# Checks the native Linux modules build programs use: process_run_linux,
# file_tree_linux, stdio_linux and text_tools.
set -eu

ziran=$1
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/std_tools_linux.zi" <<'ZI'
#import "file"
#import "file_tree_linux"
#import "process_run_linux"
#import "stdio_linux"
#import "text_tools"

#program_export
main :: () -> s32 {
    // Arguments, a working directory, environment changes, a merged and
    // captured output, and the exit status.
    shell: [3]string = .["sh", "-c", "printf '%s|%s|%s' \"$GIVEN\" \"${HOME-unset}\" \"$(pwd)\"; echo ' err' >&2; exit 3"]
    changes: [2]string = .["GIVEN=a b", "HOME"]
    options: ProcessOptions = .{arguments = shell[:], directory = "tree", environment = changes[:],
        capture_output = true, merge_error = true}
    if !CreateDirectories("tree/a/b") { return 1 }
    run: ProcessRun
    if !RunProcess(options, *run) || run.code != 3 { return 2 }
    text := BuilderFinish(run.output)
    if !TextHasPrefix(text, "a b|unset|/") || !TextHasSuffix(text, "/tree err\n") { return 3 }
    // A missing program never starts.
    missing: [1]string = .["ziran-no-such-program"]
    absent: ProcessOptions = .{arguments = missing[:]}
    failed: ProcessRun
    if RunProcess(absent, *failed) || failed.started || failed.code != 127 { return 4 }
    // Output and input files, appended.
    print_ab: [2]string = .["printf", "ab"]
    to_file: ProcessOptions = .{arguments = print_ab[:], output_path = "tree/out.txt"}
    first: ProcessRun
    if !RunProcess(to_file, *first) || first.code != 0 { return 5 }
    to_file.append_output = true
    second: ProcessRun
    if !RunProcess(to_file, *second) { return 6 }
    cat: [1]string = .["cat"]
    from_file: ProcessOptions = .{arguments = cat[:], input_path = "tree/out.txt", capture_output = true}
    third: ProcessRun
    if !RunProcess(from_file, *third) || BuilderFinish(third.output) != "abab" { return 7 }
    // Output paths stay relative to this process, not the child's directory.
    elsewhere: ProcessOptions = .{arguments = print_ab[:], directory = "tree/a", output_path = "tree/elsewhere.txt"}
    fourth: ProcessRun
    if !RunProcess(elsewhere, *fourth) || fourth.code != 0 { return 32 }
    written, wrote := ReadEntireFile("tree/elsewhere.txt")
    if !wrote || written != "ab" { return 33 }
    // A background child in its own session, stopped by a group signal.
    sleep: [2]string = .["sleep", "30"]
    background: ProcessOptions = .{arguments = sleep[:], new_session = true}
    child: ProcessRun
    if !StartProcess(background, *child) || PollProcess(*child) { return 8 }
    if !SignalProcess(*child, 15, true) || !WaitProcess(*child) || child.code != 143 { return 9 }

    // Folders, copies, permission bits, listings and atomic replacement.
    if !WriteEntireFile("tree/a/b/note.txt", "hello") || !WriteEntireFile("tree/a/z.json", "{}") { return 10 }
    if !SetPathMode("tree/a/z.json", 384) || !CopyTree("tree/a", "tree/copy") { return 11 }
    info := PathStatus("tree/copy/z.json")
    if info.kind != .File || info.mode != 384 || info.size != 2 || info.modified < 1700000000 { return 12 }
    names: Vec(string)
    if !ListDirectory("tree/copy", *names) || names.count != 2 || names[0] != "b" || names[1] != "z.json" { return 13 }
    if !ReplaceFile("tree/copy/z.json", "{\"a\": 1}\n") { return 14 }
    replaced, found := ReadEntireFile("tree/copy/z.json")
    if !found || replaced != "{\"a\": 1}\n" || PathStatus("tree/copy/z.json").mode != 384 { return 15 }
    if !IsDirectory("tree/copy/b") || IsFile("tree/copy/b") || !IsFile("tree/copy/b/note.txt") { return 16 }
    if !TextHasSuffix(ResolvePath("tree/copy/../a"), "/tree/a") || ResolvePath("tree/none") != "" { return 17 }
    lock := LockPath("tree/lock")
    if lock < 0 || LockPath("tree/lock", true, false) != -1 { return 18 }
    UnlockPath(lock)
    again := LockPath("tree/lock", true, false)
    if again < 0 { return 19 }
    UnlockPath(again)
    if !RemoveTree("tree/copy") || PathStatus("tree/copy").kind != .Missing { return 20 }
    if !MatchName("12.png", "*.png") || MatchName("12.txt", "*.png") || !MatchName("island", "isl?nd") ||
        !MatchName("b", "[a-c]") || MatchName("d", "[a-c]") || !MatchName("d", "[!a-c]") { return 21 }
    if PathName("a/b/c.png") != "c.png" || PathParent("a/b/c.png") != "a/b" || PathParent("c") != "." ||
        PathSuffix("a/b.tar.gz") != ".gz" || PathStem("x/12.png") != "12" || JoinPath("a/", "b") != "a/b" ||
        JoinPath("a", "/b") != "/b" { return 22 }

    // Text.
    lines := TextLines("one\ntwo\r\nthree\n")
    if lines.count != 3 || lines[1] != "two" || lines[2] != "three" { return 23 }
    parts := TextSplit("a,,b", ",")
    if parts.count != 3 || parts[1] != "" || TextJoin(parts, "+") != "a++b" { return 24 }
    if TextReplace("aXbXc", "X", "--") != "a--b--c" || TextTrim("  x y \n") != "x y" || TextTrimEnd(" x \n") != " x" { return 25 }
    number, ok := TextInteger("-42")
    if !ok || number != -42 { return 26 }
    bad_value, bad := TextInteger("4x")
    if bad || !TextIsLowerHex("09af") || TextIsLowerHex("09AF") || TextIsDecimal("12a") { return 27 }
    if !TextOnly("0123ft", "0123ft") || TextOnly("0124", "0123ft") || TextFind("abcabc", "c", 3) != 5 ||
        TextFindLast("abcabc", "b") != 4 || TextLower("AbC") != "abc" { return 28 }
    bytes: [3]u8 = .[0, 171, 255]
    if TextHex(bytes[:]) != "00abff" || TextHexDigit(#char "F") != 15 { return 29 }

    // Standard streams.
    if ReadStandardInput() != "piped\n" { return 30 }
    if !WriteOutput("to standard output\n") || !PrintError("to standard error\n") { return 31 }
    return 0
}
ZI

"$ziran" build --target=c --entry std_tools_linux:main \
    --root "$work" --module-path "$root/std" \
    -o "$work/c" "$work/std_tools_linux.zi"
"${CC:-cc}" -std=c99 -I"$root/include" -I"$work/c" \
    "$work/c"/*.c -lm -o "$work/program"
(cd "$work" && printf 'piped\n' | ./program > out.txt 2> error.txt)
test "$(cat "$work/out.txt")" = "to standard output"
test "$(cat "$work/error.txt")" = "to standard error"
