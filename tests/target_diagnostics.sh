#!/bin/sh
set -eu
ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$repo/build/tests"
work=$(mktemp -d "$repo/build/tests/target-diagnostics-XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/plain.zi" <<'ZI'
#program_export
Answer :: () -> s32 { return 42 }
ZI
cat > "$work/go_api.zi" <<'ZI'
native :: #system_library "go:strings";
Count :: (value: string, part: string) -> s32 #foreign native "Count";
ZI
cat > "$work/go_type.zi" <<'ZI'
native :: #system_library "go:time";
Clock :: #type #foreign native "Time";
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
    "$work/go_api.zi" "$work/go_type.zi" "$work/c_api.zi" \
    "$work/aggregate_union.zi" "$work/c_callback.zi"

for form in source saved; do
    root=$work
    extension=zi
    if [ "$form" = saved ]; then root=$work/ir; extension=zir; fi
    for target in c go zx; do
        "$ziran" check "--target=$target" --root "$root" "$root/plain.$extension"
        for fixture in go_api go_type c_api c_callback; do
            case "$fixture:$target" in
                go_*:go|c_api:c|c_api:go|c_callback:c)
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
                go_type) capability=types.go ;;
                c_api) capability=ffi.c ;;
                c_callback) capability=procedures.c-call ;;
            esac
            "$ziran" run --target=c --root "$repo/tests" "$repo/tests/json_assertions.zi" -- target-unsupported "$diagnostics" "$target" "$capability"
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
        if [ "$target" != zx ]; then
            fixture=c_callback
            if [ "$target" = c ]; then fixture=go_type; fi
            output=$work/$form-$target-output
            if "$ziran" build "--target=$target" --diagnostics=json --root "$root" \
                -o "$output" "$root/plain.$extension" "$root/$fixture.$extension" \
                > "$work/stdout" 2> "$work/build.jsonl"; then
                echo "$target emitted an unsupported foreign type" >&2
                exit 1
            fi
            test ! -d "$output" || test -z "$(find "$output" -type f -print -quit)"
            "$ziran" run --target=c --root "$repo/tests" "$repo/tests/json_assertions.zi" -- target-build "$work/build.jsonl" "$target"
        fi
    done
    for target in go zx; do
        if "$ziran" check "--target=$target" --diagnostics=json --root "$root" \
            "$root/aggregate_union.$extension" > "$work/stdout" 2> "$work/union.jsonl"; then
            echo "$target accepted an aggregate union during checking" >&2
            exit 1
        fi
        "$ziran" run --target=c --root "$repo/tests" "$repo/tests/json_assertions.zi" -- target-union "$work/union.jsonl" "$target"
    done
done
