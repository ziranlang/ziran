#!/bin/sh
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/lib/native_answer.sh"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/source_line.zi" <<'ZI'
DECL_LINE :: #line;
TEXT :: "#line";
#if #line == 3 {
SELECTED :: 40;
} else {
SELECTED :: Missing();
}
Defaulted :: (value: s64 = #line) -> s64 { return value }
#program_export
Answer :: () -> s64 {
    body_line: s64 = #line
    if body_line != 11 || TEXT != "#line" { return 0 }
    if Defaulted() != 8 || DECL_LINE != 1 { return 0 }
    return SELECTED + 2
}
ZI

"$ziran" check --root "$work" "$work/source_line.zi"
"$ziran" ir --root "$work" -o "$work/ir" "$work/source_line.zi"
"$ziran" bundle --root "$work" --entry source_line:Answer \
    -o "$work/source.zib" "$work/source_line.zi"
"$ziran" bundle --root "$work/ir" --entry source_line:Answer \
    -o "$work/saved.zib" "$work/ir/source_line.zir"
cmp "$work/source.zib" "$work/saved.zib"
test "$("$ziran" run "$work/source.zib")" = 42

for input in source saved; do
    if test "$input" = source; then
        root=$work
        file=$work/source_line.zi
    else
        root=$work/ir
        file=$work/ir/source_line.zir
    fi
    for target in c cpp go; do
        out=$work/$target-$input
        if test "$target" = go; then
            "$ziran" build --target=go --pkg main --root "$root" \
                -o "$out" "$file"
        else
            "$ziran" build "--target=$target" --root "$root" \
                -o "$out" "$file"
        fi
        case "$target" in
            c)
                cat > "$out/main.c" <<'C'
#include "source_line.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
                "${CC:-cc}" -std=c11 -I"$repo/include" -I"$out" \
                    "$out"/*.c -o "$out/app"
                "$out/app" ;;
            cpp)
                cat > "$out/main.cpp" <<'CPP'
#include "source_line.hpp"
int main() { return Answer() == 42 ? 0 : 1; }
CPP
                "${CXX:-c++}" -std=c++17 -I"$repo/include" -I"$out" \
                    "$out"/*.cpp -o "$out/app"
                "$out/app" ;;
            go)
                cat > "$out/main.go" <<'GO'
package main
func main() { if SourceLine_Answer() != 42 { panic("#line") } }
GO
                GO111MODULE=off go run "$out"/*.go ;;
        esac
    done
done

cat > "$work/loaded.zi" <<'ZI'
LOADED_LINE :: #line;
ZI
cat > "$work/from_load.zi" <<'ZI'
#load "loaded.zi";
MAIN_LINE :: #line;
#program_export
Answer :: () -> s64 { return LOADED_LINE * 40 + MAIN_LINE }
ZI
"$ziran" bundle --root "$work" --entry from_load:Answer \
    -o "$work/from_load.zib" "$work/from_load.zi"
test "$("$ziran" run "$work/from_load.zib")" = 42

cat > "$work/raw_line.zi" <<'ZI'
TEXT :: #string END
#line
END
#program_export
Answer :: () -> s64 {
    if TEXT != "#line\n" { return 0 }
    return #line
}
ZI
"$ziran" bundle --root "$work" --entry raw_line:Answer \
    -o "$work/raw_line.zib" "$work/raw_line.zi"
test "$("$ziran" run "$work/raw_line.zib")" = 7

cat > "$work/file_meta.zi" <<'ZI'
LOADED_FILE :: #file;
LOADED_DIR :: #filepath;
ZI
cat > "$work/file_path.zi" <<ZI
#load "file_meta.zi";
FILE :: #file;
DIR :: #filepath;
TEXT :: "#file #filepath";
#program_export
Answer :: () -> s64 {
    if FILE != "$work/file_path.zi" || DIR != "$work" { return 0 }
    if LOADED_FILE != "$work/file_meta.zi" || LOADED_DIR != "$work" { return 0 }
    if TEXT != "#file #filepath" { return 0 }
    return 42
}
ZI
"$ziran" ir --root "$work" -o "$work/file_ir" "$work/file_path.zi"
"$ziran" bundle --root "$work" --entry file_path:Answer \
    -o "$work/file_source.zib" "$work/file_path.zi"
"$ziran" bundle --root "$work/file_ir" --entry file_path:Answer \
    -o "$work/file_saved.zib" "$work/file_ir/file_path.zir"
cmp "$work/file_source.zib" "$work/file_saved.zib"
test "$("$ziran" run "$work/file_source.zib")" = 42
for input in source saved; do
    if test "$input" = source; then
        root=$work
        file=$work/file_path.zi
    else
        root=$work/file_ir
        file=$work/file_ir/file_path.zir
    fi
    for target in c cpp go; do
        out=$work/file-$target-$input
        native_answer_42 "$target" "$root" "$out" "$file" \
            file_path FilePath_Answer '#file'
    done
done
