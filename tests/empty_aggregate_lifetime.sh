#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/clear.zi" <<'ZI'
Callback :: #type (context: *s32, value: string) -> bool;
Inner :: struct { text: string; }
Record :: struct {
    text: string
    values: []u8
    inner: [2]Inner
    callback: Callback
    context: *s32
    serial: s32
}
stable: [3]u8 = .[97, 98, 99];
Clear :: (output: *Record) {
    output.* = Record.{}
}
Forward :: (output: *Record) -> *Record { return output }
SetStatic :: (output: *Record) {
    Forward(output).* = Record.{text = "stable"}
}
ClearIndexed :: (output: *Record) {
    output[0] = Record.{}
}
Read :: (context: *s32, value: string) -> bool {
    unused context
    return value.count > 0
}
#program_export
main :: () -> s32 {
    value: Record
    value.text = TextView(stable[:])
    value.values = stable[:]
    value.inner[0].text = "nested"
    handler: Callback = Read
    value.callback = handler
    value.serial = 42
    Clear(*value)
    if value.text.count != 0 || value.values.count != 0 ||
        value.inner[0].text.count != 0 ||
        value.context != null || value.serial != 0 {
        return 1
    }
    SetStatic(*value)
    if value.text != "stable" { return 2 }
    ClearIndexed(*value)
    if value.text.count != 0 { return 3 }
    return 0
}
ZI

"$ziran" check --root "$work" "$work/clear.zi"
"$ziran" ir --root "$work" -o "$work/ir" "$work/clear.zi"
for input in source saved; do
    source="$work/clear.zi"
    source_root=$work
    if test "$input" = saved; then
        source="$work/ir/clear.zir"
        source_root="$work/ir"
    fi
    for target in c go; do
        output="$work/$input-$target"
        set --
        if test "$target" = go; then
            set -- --pkg main --exe
        fi
        "$ziran" build --target="$target" --entry clear:main \
            "$@" --root "$source_root" -o "$output" "$source"
        case "$target" in
        c)
            "${CC:-cc}" -std=c11 -I"$repo/include" -I"$output" \
                "$output"/*.c -o "$work/test"
            "$work/test"
            ;;
        go)
            GO111MODULE=off go run "$output"/*.go
            ;;
        esac
    done
done

cat > "$work/escape.zi" <<'ZI'
Record :: struct { text: string; }
Bad :: (output: *Record) {
    bytes: [2]u8 = .[97, 98]
    output.* = Record.{text = TextView(bytes[:])}
}
ZI
if "$ziran" check --root "$work" "$work/escape.zi" \
    > "$work/escape.log" 2>&1; then
    echo 'aggregate assignment let local backing storage escape' >&2
    exit 1
fi
rg -q 'view assignment may escape its backing storage' "$work/escape.log"
