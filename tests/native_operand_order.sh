#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
module=native_operand_order_test
source=$repo/tests/spec/$module.zi
"$ziran" ir --root "$repo/tests/spec" -o "$work/ir" "$source"
for form in source saved; do
    input=$source; root=$repo/tests/spec
    if [ "$form" = saved ]; then input=$work/ir/$module.zir; root=$work/ir; fi
    "$ziran" bundle --root "$root" --entry "$module:Answer" -o "$work/$form.zx" "$input"
    test "$("$ziran" run "$work/$form.zx")" = 42
    for target in c go; do
        output=$work/$form-$target
        if [ "$target" = go ]; then
            "$ziran" build --target=go --pkg main --exe --entry "$module:main" --root "$root" -o "$output" "$input"
            GO111MODULE=off go run "$output/"*.go
        else
            "$ziran" build "--target=$target" --exe --entry "$module:main" --root "$root" -o "$output" "$input"
            case "$target" in
                c) "$output/$module" ;;
            esac
        fi
    done
    python3 - "$work/$form-c/$module.c" <<'PY'
from pathlib import Path
import re, sys
path = Path(sys.argv[1])
text = path.read_text()
for name in ('PlainWide', 'PlainView', 'PlainShort'):
    match = re.search(r'\b' + name + r'\([^;{}]*\)\s*\{(.*?)\n\}', text, re.S)
    assert match, (path, name)
    assert not re.search(r'\bvalue_\d+\b', match[1]), (path, name, match[1])
PY
done
cmp "$work/source.zx" "$work/saved.zx"
