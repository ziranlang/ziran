#!/bin/sh
set -eu
unset DISPLAY WAYLAND_DISPLAY
ziran=${1:?pass ziran}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$repo/build/tests"
work=$(mktemp -d "$repo/build/tests/law-integers-XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM

"$ziran" run --target=c --root "$repo/tests" "$repo/tests/fixture_generation.zi" -- "$ziran" "$work" law-integers
"$ziran" check --root "$work" "$work/numeric.zi" > "$work/laws.json"
if "$ziran" check --root "$work" "$work/false.zi" > "$work/false.json" 2> "$work/false.err"; then
    echo 'the integer precision regression was accepted' >&2; exit 1
fi
rg -q '"status":"disproved"' "$work/false.json"
"$ziran" ir --root "$work" -o "$work/ir" "$work/numeric.zi"
"$ziran" check --root "$work/ir" "$work/ir/numeric.zir" > "$work/saved.json"
cmp "$work/laws.json" "$work/saved.json"
"$ziran" bundle --root "$work" --entry numeric:Answer -o "$work/numeric.zx" "$work/numeric.zi"
test "$("$ziran" run "$work/numeric.zx")" = 42
for target in c go; do
    output=$work/$target
    if test "$target" = go; then
        "$ziran" build --target=go --pkg main --no-main --root "$work" -o "$output" "$work/numeric.zi"
        cat > "$output/main.go" <<'GO'
package main
func main() { if Numeric_Answer() != 42 { panic("law integer mismatch") } }
GO
        GO111MODULE=off go build -o "$output/app" "$output/numeric.go" "$output/main.go"
    elif test "$target" = c; then
        "$ziran" build --target=c --no-main --root "$work" -o "$output" "$work/numeric.zi"
        cat > "$output/main.c" <<'C'
#include "numeric.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
        ${CC:-cc} -I"$repo/include" -I"$output" "$output/numeric.c" "$output/main.c" -lm -o "$output/app"
    fi
    "$output/app"
done
