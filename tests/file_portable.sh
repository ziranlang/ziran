#!/bin/sh
# std/file is one import for files on every native C target: file_linux on
# Linux, Android, the web, and Windows, and file_plan9 when
# --target=plan9-c defines PLAN9. ReadEntireFile and WriteEntireFile read
# and replace whole files, across 4 KB chunks. Source and saved IR agree.
set -eu

ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
#import "std/file"

#program_export
Answer :: () -> s32 {
    if !CreateDirectory("files") && !DirectoryExists("files") { return 1 }
    if !WriteEntireFile("files/notes.txt", "hello\nworld\n") { return 2 }
    text, ok := ReadEntireFile("files/notes.txt")
    if !ok || text != "hello\nworld\n" { return 3 }
    missing, found := ReadEntireFile("files/missing.txt")
    if found || missing.count != 0 { return 4 }
    letters: Vec(u8)
    i: s64 = 0
    while i < 10000 { VecPush(letters, cast(u8)(65 + i % 26)); i += 1; }
    long := BuilderFinish(letters)
    if !WriteEntireFile("files/long.txt", long) { return 5 }
    back, read := ReadEntireFile("files/long.txt")
    if !read || back != long { return 6 }
    if !RemoveFile("files/notes.txt") || !RemoveFile("files/long.txt") ||
       PathExists("files/notes.txt") { return 7 }
    return 42
}
ZI

"$ziran" check --root "$work" "$work/app.zi"
"$ziran" ir --root "$work" -o "$work/ir" "$work/app.zi"
for input in source saved; do
    if test "$input" = source; then
        module=$work/app.zi
        root=$work
    else
        module=$work/ir/app.zir
        root=$work/ir
    fi
    output="$work/c-$input"
    "$ziran" build --target=c --exe --entry app:Answer --root "$root" -o "$output" "$module"
    mkdir -p "$work/run-$input"
    status=0
    (cd "$work/run-$input" && "$output/app") || status=$?
    test "$status" = 42
done

# Plan 9 builds the same source over file_plan9.
"$ziran" build --target=plan9-c --root "$work" -o "$work/plan9" "$work/app.zi"
test -f "$work/plan9/file_plan9.c"
test ! -e "$work/plan9/file_linux.c"
