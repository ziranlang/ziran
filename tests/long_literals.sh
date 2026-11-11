#!/bin/sh
# A statement may be longer than 4 KB, such as a table of records written as
# one array literal: it keeps its whole text through parsing, checking,
# and saved IR. Source and saved IR agree on every target.
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$repo/build/tests"
work=$(mktemp -d "$repo/build/tests/long-literals-XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM

"$ziran" run --target=c --root "$repo/tests" "$repo/tests/fixture_generation.zi" -- "$ziran" "$work" long-literals
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
    "$ziran" bundle --root "$root" --entry app:Answer -o "$work/$input.zx" "$module"
    test "$("$ziran" run "$work/$input.zx")" = 42
    output="$work/c-$input"
    "$ziran" build --target=c --exe --entry app:Answer --root "$root" -o "$output" "$module"
    status=0
    "$output/app" || status=$?
    test "$status" = 42
    output="$work/go-$input"
    "$ziran" build --target=go --pkg main --root "$root" -o "$output" "$module"
    cat > "$output/main.go" <<'GO'
package main
func main() { if App_Answer() != 42 { panic("wrong result") } }
GO
    GO111MODULE=off go run "$output"/*.go
done
cmp "$work/source.zx" "$work/saved.zx"
