#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${1:-"$repo/build/bin/ziran"}
case "$ziran" in /*) ;; *) ziran="$repo/$ziran";; esac
work="$repo/build/tests/safe-inference"
mkdir -p "$work/tmp" "$work/source"
export TMPDIR="$work/tmp" ZIRAN_DESKTOP_RECOVERY=0 HARMONY_DESKTOP_RECOVERY=0 YUE_DESKTOP_RECOVERY=0
export GOCACHE="$work/go-cache" GOPATH="$work/go" GOTMPDIR="$work/tmp" GOMAXPROCS=2 GOFLAGS=-p=2 GOTOOLCHAIN=local
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS GOWORK
cp "$repo/tests/safe_inference.zi" "$work/source/safe_inference.zi"
"$ziran" ir --root "$work/source" -o "$work/ir" "$work/source/safe_inference.zi"
for state in source saved; do
    input="$work/source/safe_inference.zi"
    root="$work/source"
    if test "$state" = saved; then input="$work/ir/safe_inference.zir"; root="$work/ir"; fi
    "$ziran" bundle --root "$root" --entry safe_inference:main -o "$work/$state.zx" "$input"
    test "$("$(dirname "$ziran")/zi2zx" run --vm "$work/$state.zx")" = 42
    test "$("$ziran" run "$work/$state.zx")" = 42
    for target in c go; do
        out="$work/$state-$target"
        if test "$target" = go; then
            "$ziran" build --target=go --pkg main --exe --entry safe_inference:main --root "$root" -o "$out" "$input"
            GO111MODULE=off go build -o "$out/app" "$out"/*.go
        else
            "$ziran" build --target=c --exe --entry safe_inference:main --root "$root" -o "$out" "$input"
        fi
        executable="$out/app"
        if test "$target" = c; then executable="$out/safe_inference"; fi
        test "$("$executable")" = 42
    done
done
python3 - "$ziran" "$work" <<'PY'
import json,pathlib,subprocess,sys
ziran,where=sys.argv[1:]; root=pathlib.Path(where)/'negative'; root.mkdir(exist_ok=True)
cases={
 'no_context':'Answer :: () { result := .{value=42}; }',
 'ambiguous':'A :: struct { value: s32; }\nB :: struct { value: s32; }\nAnswer :: () { result := ifx true then .{value=1} else .{value=2}; }',
 'distinct':'A :: struct { value: s32; }\nB :: struct { value: s32; }\nAnswer :: () { result := ifx true then A.{value=1} else B.{value=2}; }',
 'unknown_field':'A :: struct { value: s32; }\nAnswer :: () { result := ifx true then A.{value=1} else .{missing=2}; }',
 'field_type':'A :: struct { value: s32; }\nAnswer :: () { result := ifx true then A.{value=1} else .{value="wrong"}; }',
 'owned':'#import "std/vec"\nA :: struct { values: Vec(u8); }\nAnswer :: () { result := ifx true then A.{} else .{}; }',
 'enum':'A :: enum { One; Two; }\nAnswer :: () { result := ifx true then A.{} else .{}; }',
 'foreign':'c :: #system_library "libc";\nA :: #type #foreign c;\nAnswer :: () { result := ifx true then A.{} else .{}; }',
 'recursive':'A :: struct { value: s32; }\nAnswer :: () { result := ifx true then result else A.{value=2}; }',
 'global':'A :: struct { value: s32; }\nresult := ifx true then A.{value=1} else .{value=2};',
 'missing_import':'Answer :: () { result := Missing.Record.{value=42}; }',
}
for name,text in cases.items():
 path=root/(name+'.zi'); path.write_text(text+'\n')
 result=subprocess.run([ziran,'check','--root',str(root),'--diagnostics=json',str(path)],capture_output=True,timeout=30)
 assert result.returncode and not result.stdout,(name,result.stdout,result.stderr)
 rows=[json.loads(line) for line in result.stderr.splitlines()]; assert rows and all(row['schema_version']==2 for row in rows),name
 assert all(len(line)<=32768 for line in result.stderr.splitlines(keepends=True)),name
 if name=='missing_import': assert all(not row['alternatives'] for row in rows)
print('safe inference: C/Go/native bundle/explicit VM source and saved IR; ambiguity, fields, owned/foreign/enum, recursion, globals/imports rejected PASS')
PY
