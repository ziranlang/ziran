#!/bin/sh
# Enum lowering must retain declarations beyond the old Go limits of
# 32 enums per module and 64 members per enum.
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

python3 - "$work" <<'PY'
from pathlib import Path
import sys
work = Path(sys.argv[1])
source = ['Large :: enum s32 {']
source += [f'    Member{i};' for i in range(96)]
source += ['}']
for i in range(40):
    source += [f'Enum{i} :: enum s32 {{ Zero; Last :: {i}; }}']
source += ['#program_export', 'Answer :: () -> s32 {',
           '    if cast(s32)Large.Member95 != 95 { return 1 }',
           '    if cast(s32)Enum39.Last != 39 { return 2 }',
           '    return 42', '}']
(work / 'app.zi').write_text('\n'.join(source) + '\n')
PY
"$ziran" ir --root "$work" -o "$work/ir" "$work/app.zi"
for form in source saved; do
    module=$work/app.zi
    root=$work
    if test "$form" = saved; then module=$work/ir/app.zir; root=$work/ir; fi
    "$ziran" bundle --root "$root" --entry app:Answer -o "$work/$form.zib" "$module"
    test "$("$ziran" run "$work/$form.zib")" = 42
    for target in c cpp go py rust; do
        output=$work/$target-$form
        if test "$target" = rust; then
            if ! command -v cargo >/dev/null 2>&1; then continue; fi
            "$ziran" build --target=rust --exe --entry app:Answer --root "$root" -o "$output" "$module"
            CARGO_TARGET_DIR="$work/rust-target" cargo build --quiet --manifest-path "$output/Cargo.toml"
            status=0
            "$work/rust-target/debug/ziran_generated" || status=$?
            test "$status" = 42
        elif test "$target" = py; then
            "$ziran" build --target=py --exe --entry app:Answer --root "$root" -o "$output" "$module"
            status=0
            python3 "$output" || status=$?
            test "$status" = 42
        elif test "$target" = go; then
            "$ziran" build --target=go --pkg main --root "$root" -o "$output" "$module"
            printf '%s\n' 'package main' 'func main() { if App_Answer() != 42 { panic("large enums") } }' > "$output/main.go"
            GO111MODULE=off go run "$output"/*.go
        elif test "$target" = cpp; then
            "$ziran" build --target=cpp --root "$root" -o "$output" "$module"
            printf '%s\n' '#include "app.hpp"' 'int main() { return Answer() == 42 ? 0 : 1; }' > "$output/main.cpp"
            "${CXX:-c++}" -std=c++17 -I"$repo/include" -I"$output" "$output"/*.cpp -o "$output/app"
            "$output/app"
        else
            "$ziran" build --target=c --exe --entry app:Answer --root "$root" -o "$output" "$module"
            status=0
            "$output/app" || status=$?
            test "$status" = 42
        fi
    done
done
cmp "$work/source.zib" "$work/saved.zib"
