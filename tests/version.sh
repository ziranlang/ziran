#!/bin/sh
set -eu

ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
want=$(cat "$repo/VERSION")

case "$want" in
    [0-9]*.[0-9]*.[0-9]*) ;;
    *) echo "VERSION must be MAJOR.MINOR.PATCH, got '$want'" >&2; exit 1 ;;
esac

test "$("$ziran" version)" = "ziran $want"
test "$("$ziran" --version)" = "ziran $want"
grep -q "^## \[$want\]" "$repo/CHANGELOG.md" || {
    echo "CHANGELOG.md has no entry for $want" >&2
    exit 1
}
"$ziran" --help 2>&1 | grep -Fq 'ziran version'
echo "version: $want matches VERSION, CHANGELOG.md, and the built compiler"
