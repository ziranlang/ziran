#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${1:-$repo/build/bin/ziran}
work="$repo/build/math-sqrt-bench"
mkdir -p "$work"
unset DISPLAY WAYLAND_DISPLAY
cd "$repo"
printf 'source_revision '; git rev-parse HEAD
sha256sum std/math.zi tests/spec/math_sqrt_reference.zi bench/math_sqrt.zi
"$ziran" build --target=c --root bench --module-path std --module-path tests/spec \
    -o "$work/c" bench/math_sqrt.zi
"${CC:-cc}" -std=c11 -O2 -iquote "$work/c" "$work/c"/*.c -lm -o "$work/math_sqrt"
"$work/math_sqrt"
