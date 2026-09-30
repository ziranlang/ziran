#!/bin/sh
# Inputs that once crashed the compiler. Each must fail with a diagnostic
# (or pass), never with a signal. Build with `make sanitize` to also catch
# memory errors that do not crash.
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

for input in "$repo"/tests/fuzz/*.zi; do
    name=$(basename "$input")
    mkdir -p "$work/$name.d"
    cp "$input" "$work/$name.d/$name"
    status=0
    "$ziran" check --root "$work/$name.d" "$work/$name.d/$name" \
        > "$work/$name.out" 2>&1 || status=$?
    if test "$status" -gt 1 || grep -q 'Sanitizer\|runtime error:' "$work/$name.out"; then
        echo "$name: compiler crashed (status $status)" >&2
        cat "$work/$name.out" >&2
        exit 1
    fi
done
