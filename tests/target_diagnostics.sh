#!/bin/sh
set -eu
ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/plain.zi" <<'ZI'
#program_export
Answer :: () -> s32 { return 42 }
ZI
cat > "$work/go_api.zi" <<'ZI'
native :: #system_library "go:strings";
Count :: (value: string, part: string) -> s32 #foreign native "Count";
ZI
cat > "$work/py_api.zi" <<'ZI'
native :: #system_library "py:builtins";
Count :: (value: string) -> s64 #foreign native "len";
ZI
cat > "$work/go_type.zi" <<'ZI'
native :: #system_library "go:time";
Clock :: #type #foreign native "Time";
ZI
cat > "$work/py_type.zi" <<'ZI'
native :: #system_library "py:builtins";
Bytes :: #type #foreign native "bytes";
ZI
cat > "$work/c_api.zi" <<'ZI'
native :: #system_library "libc";
Abs :: (value: s32) -> s32 #foreign native "abs";
ZI
cat > "$work/c_callback.zi" <<'ZI'
Callback :: #type (value: s32) -> s32 #c_call;
ZI
cat > "$work/aggregate_union.zi" <<'ZI'
Item :: struct { value: s64 }
Payload :: union { item: Item; value: s64 }
#program_export
Answer :: () -> s32 { payload: Payload; return cast(s32)payload.item.value }
ZI
"$ziran" ir --root "$work" -o "$work/ir" "$work/plain.zi" \
    "$work/go_api.zi" "$work/py_api.zi" "$work/go_type.zi" "$work/py_type.zi" "$work/c_api.zi" \
    "$work/aggregate_union.zi" "$work/c_callback.zi"

for form in source saved; do
    root=$work
    extension=zi
    if [ "$form" = saved ]; then root=$work/ir; extension=zir; fi
    for target in c cpp go rust py zib plan9-c; do
        "$ziran" check "--target=$target" --root "$root" "$root/plain.$extension"
        for fixture in go_api py_api go_type py_type c_api c_callback; do
            case "$fixture:$target" in
                go_*:go|py_*:py|c_api:c|c_api:cpp|c_api:go|c_api:rust|c_api:py|c_api:plan9-c|c_callback:c|c_callback:cpp|c_callback:rust|c_callback:py|c_callback:plan9-c)
                    "$ziran" check "--target=$target" --root "$root" "$root/$fixture.$extension"
                    continue ;;
            esac
            diagnostics=$work/$form-$target-$fixture.jsonl
            if "$ziran" check "--target=$target" --diagnostics=json --root "$root" \
                "$root/$fixture.$extension" > "$work/stdout" 2> "$diagnostics"; then
                echo "$target accepted unsupported $fixture ($form)" >&2
                exit 1
            fi
            capability=ffi.go
            case "$fixture" in
                py_api) capability=ffi.py ;;
                go_type) capability=types.go ;;
                py_type) capability=types.py ;;
                c_api) capability=ffi.c ;;
                c_callback) capability=procedures.c-call ;;
            esac
            python3 - "$diagnostics" "$target" "$capability" <<'PY'
import json, pathlib, sys
items = [json.loads(line) for line in pathlib.Path(sys.argv[1]).read_text().splitlines()]
assert len(items) == 1, items
item = items[0]
assert item['schema_version'] == 1 and item['severity'] == 'error', item
assert item['target'] == sys.argv[2] and item['capability'] == sys.argv[3], item
assert item['code'] in ('check.record', 'check.foreign') and item['line'] > 0, item
PY
        done
        if [ "$target" = go ]; then
            output=$work/$form-callback-output
            if "$ziran" build --target=go --diagnostics=json --root "$root" \
                -o "$output" "$root/plain.$extension" "$root/c_callback.$extension" \
                > "$work/stdout" 2> "$work/callback.jsonl"; then
                echo 'Go emitted an unsupported C callback type' >&2
                exit 1
            fi
            test ! -d "$output" || test -z "$(find "$output" -type f -print -quit)"
        fi
        # Preflight all inputs before emitting even the first valid module.
        if [ "$target" != zib ]; then
            fixture=go_type
            if [ "$target" = go ]; then fixture=py_type; fi
            output=$work/$form-$target-output
            if "$ziran" build "--target=$target" --diagnostics=json --root "$root" \
                -o "$output" "$root/plain.$extension" "$root/$fixture.$extension" \
                > "$work/stdout" 2> "$work/build.jsonl"; then
                echo "$target emitted an unsupported foreign type" >&2
                exit 1
            fi
            test ! -d "$output" || test -z "$(find "$output" -type f -print -quit)"
            python3 - "$work/build.jsonl" "$target" <<'PY'
import json, pathlib, sys
items = [json.loads(line) for line in pathlib.Path(sys.argv[1]).read_text().splitlines()]
assert len(items) == 1 and items[0]['target'] == sys.argv[2], items
PY
        fi
    done
    for target in go zib; do
        if "$ziran" check "--target=$target" --diagnostics=json --root "$root" \
            "$root/aggregate_union.$extension" > "$work/stdout" 2> "$work/union.jsonl"; then
            echo "$target accepted an aggregate union during checking" >&2
            exit 1
        fi
        python3 - "$work/union.jsonl" "$target" <<'PY'
import json, pathlib, sys
items = [json.loads(line) for line in pathlib.Path(sys.argv[1]).read_text().splitlines()]
assert len(items) == 1 and items[0]['target'] == sys.argv[2], items
assert items[0]['capability'] == 'unions.scalar', items
PY
    done
done
