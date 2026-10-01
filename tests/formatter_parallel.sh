#!/bin/sh
set -eu

formatter=$(dirname "$1")/zi-fmt
scratch=$(mktemp -d)
trap 'rm -rf "$scratch"' 0

pids=
for index in 1 2 3 4 5 6 7 8; do
    source="$scratch/$index.zi"
    awk -v marker="$index" 'BEGIN {
        print "// unique marker " marker
        for (line = 0; line < 1000; line++)
            print "fn Example() -> i32 {\nreturn 1\n}"
    }' > "$source"
    TMPDIR="$scratch" "$formatter" "$source" &
    pids="$pids $!"
done

for pid in $pids; do
    wait "$pid"
done

for index in 1 2 3 4 5 6 7 8; do
    source="$scratch/$index.zi"
    [ "$(head -n 1 "$source")" = "// unique marker $index" ]
    [ "$(wc -c < "$source")" -gt 10000 ]
    TMPDIR="$scratch" "$formatter" --check "$source"
done
