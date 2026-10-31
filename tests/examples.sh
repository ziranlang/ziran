#!/bin/sh
set -eu

# Every site example with a main prints the same text on the portable VM
# and as C and Go.
ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
examples=$repo/site/examples
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

checked=0
for source in "$examples"/*.zi; do
    name=$(basename "$source" .zi)
    grep -q '^main :: ()' "$source" || continue
    out=$work/$name
    mkdir -p "$out"

    "$ziran" bundle --root "$examples" --entry "$name:main" -o "$out/$name.zx" "$source"
    "$ziran" run "$out/$name.zx" > "$out/expected"
    test -s "$out/expected" || { echo "$name printed nothing" >&2; exit 1; }

    "$ziran" build --target=c --root "$examples" -o "$out/c" "$source"
    printf '#include "%s.h"\nint main(void) { %s_main(); return 0; }\n' "$name" "$name" \
        > "$out/c/run.c"
    "${CC:-cc}" -std=c99 -I"$out/c" "$out/c/$name.c" "$out/c/run.c" -o "$out/c/program"
    "$out/c/program" > "$out/c.out"

    "$ziran" build --target=go --pkg main --exe --entry "$name:main" \
        --root "$examples" -o "$out/go" "$source"
    (cd "$out/go" && GO111MODULE=off go run .) > "$out/go.out"


    for target in c go; do
        if ! cmp -s "$out/expected" "$out/$target.out"; then
            echo "$name: $target output differs from the portable VM" >&2
            diff "$out/expected" "$out/$target.out" >&2 || true
            exit 1
        fi
    done

    checked=$((checked + 1))
done
test "$checked" -ge 10
