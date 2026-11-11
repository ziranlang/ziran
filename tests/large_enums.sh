#!/bin/sh
# Enum lowering must retain declarations beyond the old Go limits of
# 32 enums per module and 64 members per enum.
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$repo/build/tests"
work=$(mktemp -d "$repo/build/tests/large-enums-XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM

"$ziran" run --target=c --root "$repo/tests" "$repo/tests/fixture_generation.zi" -- "$ziran" "$work" enums
"$ziran" ir --root "$work" -o "$work/ir" "$work/app.zi"
for form in source saved; do
    module=$work/app.zi
    root=$work
    if test "$form" = saved; then module=$work/ir/app.zir; root=$work/ir; fi
    "$ziran" bundle --root "$root" --entry app:Answer -o "$work/$form.zx" "$module"
    test "$("$ziran" run "$work/$form.zx")" = 42
    for target in c go; do
        output=$work/$target-$form
        if test "$target" = go; then
            "$ziran" build --target=go --pkg main --root "$root" -o "$output" "$module"
            printf '%s\n' 'package main' 'func main() { if App_Answer() != 42 { panic("large enums") } }' > "$output/main.go"
            GO111MODULE=off go run "$output"/*.go
        else
            "$ziran" build --target=c --exe --entry app:Answer --root "$root" -o "$output" "$module"
            status=0
            "$output/app" || status=$?
            test "$status" = 42
        fi
    done
done
cmp "$work/source.zx" "$work/saved.zx"
