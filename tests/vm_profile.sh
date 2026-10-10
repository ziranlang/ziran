#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS ZIRAN_VM_PROFILE
cat > "$work/app.zi" <<'ZI'
Cell :: struct { value: s32; label: string }
counter: s32;
Leaf :: (value: s32, secret: string) -> Cell { return .{value,secret} }
Child :: (value: s32, secret: string) -> s32 {
    result := Leaf(value,secret)
    return result.value + counter
}
#program_export
main :: () -> s32 {
    total: s32
    for i: 0..999 {
        counter = cast(s32)i
        total += Child(cast(s32)i,"arguments-must-never-enter-a-profile")
    }
    return ifx total == 999000 then 0 else 1
}
ZI
"$ziran" ir --root "$work" -o "$work/ir" "$work/app.zi"
for form in source saved; do
    source="$work/app.zi"; root="$work"
    if test "$form" = saved; then source="$work/ir/app.zir"; root="$work/ir"; fi
    "$ziran" bundle --root "$root" --entry app:main -o "$work/$form.zib" "$source"
    test "$("$ziran" run "$work/$form.zib")" = 0
    test ! -e "$work/$form.jsonl"
    for repeat in 1 2; do
        test "$(ZIRAN_VM_PROFILE="$work/$form.jsonl" "$ziran" run "$work/$form.zib")" = 0
    done
    test "$(ZIRAN_VM_PROFILE="$work/missing/profile.jsonl" "$ziran" run "$work/$form.zib" 2> "$work/$form-unavailable.log")" = 0
    python3 - "$work/$form.jsonl" <<'PY'
import json,sys
from pathlib import Path
text=Path(sys.argv[1]).read_text()
assert 'arguments-must-never-enter-a-profile' not in text
reports=[json.loads(line) for line in text.splitlines()]
assert len(reports)==2
for report in reports:
    assert report['complete'] and not report['failed']
    assert report['clock']=='process_cpu' and report['live_value_bytes']>=0
    caches=report['metadata_caches']
    assert caches['call']['hits']>0 and caches['call']['misses']>0
    assert caches['global']['hits']>0 and caches['global']['misses']>0
    assert all(0<=item['collisions']<=item['misses'] for item in caches.values())
    rows={row['function']:row for row in report['functions'] if row['module']=='app'}
    assert rows['main']['calls']==1
    assert rows['Child']['calls']==1000 and rows['Leaf']['calls']==1000
    assert rows['Leaf']['self_allocations']>0
    assert rows['main']['inclusive_cpu_ms']>=rows['main']['self_cpu_ms']>=0
    assert all(row['inclusive_cpu_ms']>=row['self_cpu_ms']>=0 for row in rows.values())
PY
done
printf '%s\n' 'VM function profiles preserve results, nesting, allocation counts and argument privacy'
