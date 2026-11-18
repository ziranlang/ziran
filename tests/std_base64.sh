#!/bin/sh
# std/base64: RFC 4648 encoding, decoding, whitespace and malformed-input
# rejection in the portable engine (from source and saved IR) and natively.
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY
cd "$repo"

"$ziran" check --root tests/spec --module-path std tests/spec/std_base64.zi
"$ziran" ir --root tests/spec --module-path std -o "$work/ir" tests/spec/std_base64.zi
"$ziran" bundle --root tests/spec --module-path std \
    --entry std_base64:SelfTest -o "$work/source.zx" tests/spec/std_base64.zi
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    --entry std_base64:SelfTest -o "$work/saved.zx" "$work/ir/std_base64.zir"
cmp "$work/source.zx" "$work/saved.zx"
test "$("$ziran" run "$work/source.zx")" = 42

"$ziran" build --target=c --no-main --root tests/spec --module-path std -o "$work/c" tests/spec/std_base64.zi
cat > "$work/main.c" <<'C'
#include <stdint.h>
int32_t SelfTest(void);
int main(void) { return SelfTest() == 42 ? 0 : 1; }
C
cc -std=c11 -O2 -iquote "$work/c" "$work/c"/*.c "$work/main.c" -o "$work/run"
"$work/run"
echo 'std-base64-test-ok: RFC 4648 vectors and malformed inputs match across portable bundle and native C'
