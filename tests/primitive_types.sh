#!/bin/sh
set -eu

ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/types.zi" <<'ZI'
Probe :: (number: s32, small: float32, large: float64,
          integer_alias: int, float_alias: float) -> s64 {
    return integer_alias
}
#program_export
Answer :: () -> s64 { return 42 }
ZI

"$ziran" ir --root "$work" -o "$work/ir" "$work/types.zi"
python3 - "$work/ir/types.zir" <<'PY'
from pathlib import Path
import sys

saved = Path(sys.argv[1]).read_bytes()
assert saved[:8] == b'ZIR\0\x35\0\0\0'
for spelling in (b's32', b's64', b'float32', b'float64'):
    assert spelling in saved, spelling
for spelling in (b'i32', b'i64', b'f32', b'f64', b'double'):
    assert spelling not in saved, spelling
PY

"$ziran" bundle --root "$work/ir" --entry types:Answer \
    -o "$work/types.zib" "$work/ir/types.zir"
test "$("$ziran" run "$work/types.zib")" = 42
