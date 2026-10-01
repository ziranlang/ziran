#!/bin/sh
# A statement may be longer than 4 KB, such as a table of records written as
# one array literal: it keeps its whole text through parsing, checking,
# and saved IR. Source and saved IR agree on every target.
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

python3 - "$work/app.zi" <<'PY'
import sys
count = 80
lines = ["Entry :: struct { code: s32; name: string; note: string; }", "",
         "Ordered :: struct { first: s32; second: s32; third: s32; }",
         "sequence: s32;",
         "Next :: () -> s32 { sequence += 1; return sequence }", "",
         "#program_export", "Answer :: () -> s32 {",
         f"    table: [{count}]Entry = .["]
for i in range(count):
    lines.append(f'        .{{code = {i}, name = "entry{i}", note = "a long explanatory note for entry {i}, '
                 'padding the literal well past one statement buffer"},')
lines += ["    ]",
          "    bytes := u8.[\n" + ",\n".join("        " + ", ".join(str(i % 256) for i in range(start, start + 64)) for start in range(0, 2048, 64)) + "\n    ]",
          "    for index: 0..2047 { if bytes[index] != cast(u8)(index % 256) { return 2 } }",
          "    sequence = 0",
          "    ordered := s32.[sequence, Next(), sequence, Next(), sequence]",
          "    if ordered[0] != 0 || ordered[1] != 1 || ordered[2] != 1 || ordered[3] != 2 || ordered[4] != 2 { return 3 }",
          "    sequence = 0",
          "    record := Ordered.{first = sequence, second = Next(), third = sequence}",
          "    if record.first != 0 || record.second != 1 || record.third != 1 { return 4 }",
          "    total: s32 = 0",
          "    for table { total += it.code }",
          f"    if total != {count * (count - 1) // 2} || table[{count - 1}].name != \"entry{count - 1}\" {{ return 1 }}",
          "    return 42", "}"]
open(sys.argv[1], "w").write("\n".join(lines) + "\n")
PY
test "$(wc -c < "$work/app.zi")" -gt 9000

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
    "$ziran" bundle --root "$root" --entry app:Answer -o "$work/$input.zib" "$module"
    test "$("$ziran" run "$work/$input.zib")" = 42
    output="$work/c-$input"
    "$ziran" build --target=c --exe --entry app:Answer --root "$root" -o "$output" "$module"
    status=0
    "$output/app" || status=$?
    test "$status" = 42
    output="$work/cpp-$input"
    "$ziran" build --target=cpp --root "$root" -o "$output" "$module"
    cat > "$output/main.cpp" <<'CPP'
#include "app.hpp"
int main() { return Answer() == 42 ? 0 : 1; }
CPP
    "${CXX:-c++}" -I"$repo/include" -I"$output" "$output"/*.cpp -o "$output/app"
    "$output/app"
    output="$work/go-$input"
    "$ziran" build --target=go --pkg main --root "$root" -o "$output" "$module"
    cat > "$output/main.go" <<'GO'
package main
func main() { if App_Answer() != 42 { panic("wrong result") } }
GO
    GO111MODULE=off go run "$output"/*.go
done
cmp "$work/source.zib" "$work/saved.zib"
