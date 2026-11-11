#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${1:-$repo/build/bin/ziran}
work="$repo/build/math-sqrt"
mkdir -p "$work"
unset DISPLAY WAYLAND_DISPLAY
cd "$repo"
"$ziran" check --root tests/spec --module-path std tests/spec/std_math_sqrt.zi
"$ziran" ir --root tests/spec --module-path std -o "$work/ir" tests/spec/std_math_sqrt.zi
"$ziran" bundle --root tests/spec --module-path std --entry std_math_sqrt:SelfTest \
    -o "$work/source.zx" tests/spec/std_math_sqrt.zi
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" --entry std_math_sqrt:SelfTest \
    -o "$work/saved.zx" "$work/ir/std_math_sqrt.zir"
cmp "$work/source.zx" "$work/saved.zx"
test "$("$ziran" run "$work/source.zx")" = 42
test "$("$ziran" run "$work/saved.zx")" = 42
"$ziran" build --target=c --root tests/spec --module-path std -o "$work/source-c" tests/spec/std_math_sqrt.zi
"${CC:-cc}" -std=c11 -O2 -iquote "$work/source-c" "$work/source-c"/*.c -lm -o "$work/source"
"$work/source"
"$ziran" build --target=c --root "$work/ir" --module-path "$work/ir" -o "$work/saved-c" "$work/ir/std_math_sqrt.zir"
"${CC:-cc}" -std=c11 -O2 -iquote "$work/saved-c" "$work/saved-c"/*.c -lm -o "$work/saved"
"$work/saved"
echo 'math-sqrt-parity-ok: source/saved IR and native/portable'
