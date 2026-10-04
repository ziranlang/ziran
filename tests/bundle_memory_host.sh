#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
library=${ZIRAN_LIB:-"$(dirname "$ziran")/../libziran.a"}
"${CC:-cc}" -D_GNU_SOURCE -std=c11 -I"$repo/include" \
    "$repo/tests/host_capability_no_temp_test.c" "$library" \
    -Wl,--wrap=tmpfile -lm -o "$work/host"
sh "$repo/tests/host_capability.sh" "$ziran" "$work/host"
