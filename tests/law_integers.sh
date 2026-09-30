#!/bin/sh
set -eu
unset DISPLAY WAYLAND_DISPLAY
ziran=${1:?pass ziran}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

python3 - "$work" <<'PY'
from pathlib import Path
import sys
root = Path(sys.argv[1]); source = []; checks = []
for prefix in ('s', 'u'):
    for width in (8, 16, 32, 64):
        t = f'{prefix}{width}'; maximum = 2**(width-(prefix=='s'))-1
        wrapped = -2**(width-1) if prefix=='s' else 0
        source += [f'Add{t} :: (a: {t}, b: {t}) -> {t} {{ return a + b }}',
                   f'#law Wrap{t} custom Add{t}(cast({t}){maximum}, cast({t})1) == cast({t})({wrapped});',
                   f'#law Commute{t} theorem a: {t}, b: {t} => Add{t}(a,b) == Add{t}(b,a);',
                   f'#proof Commute{t} {{ unfold Add{t}; ring; }}']
        checks.append(f'    if Add{t}(cast({t}){maximum}, cast({t})1) != cast({t})({wrapped}) {{ return 1 }}')
source += ['NoGreater :: (values: [2]s64) -> bool { return values[0] <= values[1] }',
           '#law ExactLarge custom !NoGreater(s64.[9007199254740993, 9007199254740992]);',
           '#law UnsignedOrder custom cast(u64)18446744073709551615 > cast(u64)9223372036854775808;',
           '#law UnsignedDivide custom cast(u64)18446744073709551615 / cast(u64)3 == cast(u64)6148914691236517205;',
           '#law UnsignedSignedCast custom cast(s64)(cast(u64)18446744073709551615) == cast(s64)(-1);',
           '#law NarrowSignedCast custom cast(u8)(cast(s8)(-1)) == cast(u8)255;',
           '#law InvertByte custom ~cast(u8)0 == cast(u8)255;',
           '#law ShiftByte custom (cast(s8)(-128) >> cast(s8)1) == cast(s8)(-64);',
           '#law MinDivide custom cast(s64)(-9223372036854775808) / cast(s64)(-1) == cast(s64)(-9223372036854775808);',
           'Code :: enum u8 { Ok, Failed }',
           '#law EnumCast custom cast(u8)(cast(Code)255) == cast(u8)255;',
           '#program_export', 'Answer :: () -> s32 {']
checks += ['    if NoGreater(s64.[9007199254740993, 9007199254740992]) { return 2 }',
           '    if cast(u64)18446744073709551615 <= cast(u64)9223372036854775808 { return 3 }']
source += checks + ['    return 42', '}']
(root / 'numeric.zi').write_text('\n'.join(source)+'\n')
(root / 'false.zi').write_text('NoGreater :: (values: [2]s64) -> bool { return values[0] <= values[1] }\n#law WideFalse custom NoGreater(s64.[9007199254740993,9007199254740992]);\n#program_export\nAnswer :: () -> s32 { return 0 }\n')
PY
"$ziran" check --root "$work" "$work/numeric.zi" > "$work/laws.json"
if "$ziran" check --root "$work" "$work/false.zi" > "$work/false.json" 2> "$work/false.err"; then
    echo 'the integer precision regression was accepted' >&2; exit 1
fi
rg -q '"status":"disproved"' "$work/false.json"
"$ziran" ir --root "$work" -o "$work/ir" "$work/numeric.zi"
"$ziran" check --root "$work/ir" "$work/ir/numeric.zir" > "$work/saved.json"
cmp "$work/laws.json" "$work/saved.json"
"$ziran" bundle --root "$work" --entry numeric:Answer -o "$work/numeric.zib" "$work/numeric.zi"
test "$("$ziran" run "$work/numeric.zib")" = 42
for target in c cpp go; do
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
    else
        "$ziran" build --target=cpp --no-main --root "$work" -o "$output" "$work/numeric.zi"
        cat > "$output/main.cpp" <<'CPP'
#include "numeric.hpp"
int main() { return Answer() == 42 ? 0 : 1; }
CPP
        ${CXX:-c++} -I"$repo/include" -I"$output" "$output/numeric.cpp" "$output/main.cpp" -lm -o "$output/app"
    fi
    "$output/app"
done
"$ziran" build --target=rust --exe --entry numeric:Answer --root "$work" -o "$work/rust" "$work/numeric.zi"
CARGO_TARGET_DIR=$work/rust-target cargo build --quiet --manifest-path "$work/rust/Cargo.toml"
status=0; "$work/rust-target/debug/ziran_generated" || status=$?
test "$status" = 42
"$ziran" build --target=py --exe --entry numeric:Answer --root "$work" -o "$work/app.py" "$work/numeric.zi"
status=0; python3 "$work/app.py" || status=$?
test "$status" = 42
