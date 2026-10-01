#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d "$repo/build/test/wide-comparisons.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY
module=plan9_wide_compare_test
"$ziran" ir --root "$repo/tests/spec" -o "$work/ir" "$repo/tests/spec/$module.zi"
for form in source saved; do
    root=$repo/tests/spec
    input=$root/$module.zi
    if test "$form" = saved; then root=$work/ir; input=$root/$module.zir; fi
    "$ziran" bundle --root "$root" --entry "$module:main" -o "$work/$form.zib" "$input"
    test "$("$ziran" run "$work/$form.zib" | tail -n 1)" = 0
    for target in c cpp go; do
        output=$work/$target-$form
        if test "$target" = go; then
            "$ziran" build --target=go --pkg main --root "$root" -o "$output" "$input"
        else
            "$ziran" build --target="$target" --root "$root" -o "$output" "$input"
        fi
        if test "$target" = c; then
            "${CC:-cc}" -std=c11 -I"$repo/include" -I"$output" "$output"/*.c -o "$output/run"
            "$output/run"
        elif test "$target" = cpp; then
            "${CXX:-c++}" -std=c++17 -I"$repo/include" -I"$output" "$output"/*.cpp -o "$output/run"
            "$output/run"
        else
            cat > "$output/main.go" <<'GO'
package main
func main() { if Plan9WideCompareTest_Main() != 0 { panic("wide comparison result") } }
GO
            GO111MODULE=off go run "$output"/*.go
        fi
    done
done
cmp "$work/source.zib" "$work/saved.zib"
