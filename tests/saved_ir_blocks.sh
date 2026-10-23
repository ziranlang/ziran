#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
# Saved IR is read and rewritten through block buffers. A program larger than
# several blocks must survive the round trip, and the reader must still refuse
# a file with bytes after its program or one that ends early.
{
    i=0
    while [ "$i" -lt 600 ]; do
        printf 'Step%d :: (value: s64) -> s64 {\n' "$i"
        printf '    doubled := value * 2 + %d\n' "$i"
        printf '    if doubled > 1000000 { return doubled - 1000000 }\n'
        printf '    return doubled - value\n}\n'
        i=$((i + 1))
    done
    printf '#program_export\nAnswer :: () -> s64 {\n    total: s64 = 0\n'
    i=0
    while [ "$i" -lt 600 ]; do
        printf '    total = Step%d(total)\n' "$i"
        i=$((i + 1))
    done
    printf '    return total\n}\n'
} > "$work/large.zi"
"$ziran" ir --root "$work" --module-path "$repo/std" -o "$work/ir" "$work/large.zi"
size=$(wc -c < "$work/ir/large.zir")
test "$size" -gt 262144
"$ziran" bundle --root "$work" --module-path "$repo/std" --entry large:Answer -o "$work/source.zx" "$work/large.zi"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" --entry large:Answer -o "$work/saved.zx" "$work/ir/large.zir"
cmp "$work/source.zx" "$work/saved.zx"
test "$("$ziran" run "$work/saved.zx")" = 179700

mkdir "$work/trailing" "$work/short"
cp "$work/ir/large.zir" "$work/trailing/large.zir"
printf 'x' >> "$work/trailing/large.zir"
if "$ziran" bundle --root "$work/trailing" --module-path "$work/trailing" --entry large:Answer \
    -o "$work/trailing.zx" "$work/trailing/large.zir" 2> "$work/trailing.err"; then
    echo 'saved IR with trailing bytes was accepted' >&2
    exit 1
fi
grep -q 'trailing or unreadable ZIR data' "$work/trailing.err"
head -c $((size - 3)) "$work/ir/large.zir" > "$work/short/large.zir"
if "$ziran" bundle --root "$work/short" --module-path "$work/short" --entry large:Answer \
    -o "$work/short.zx" "$work/short/large.zir" 2> "$work/short.err"; then
    echo 'truncated saved IR was accepted' >&2
    exit 1
fi
grep -q 'truncated' "$work/short.err"
echo 'Saved IR larger than its read blocks round-trips; trailing and truncated IR are refused'
