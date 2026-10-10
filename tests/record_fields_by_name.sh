#!/bin/sh
set -eu

# A host may return records built against another layout of the same type
# when its instance matches fields by name.
ziran=$1
host_test=$2
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/fields_host.zi" <<'ZI'
host_api :: #system_library "host_api";
Size :: struct {
    width: s32
    height: s32
}
Frame :: struct {
    width: s32
    label: string
    scale: s32
    size: Size
}
ReadFrameHost :: () -> Frame #foreign host_api;
#program_export
Answer :: () -> s32 {
    frame := ReadFrameHost()
    if frame.width != 7 || frame.label != "ok" || frame.scale != 0 {
        return 0
    }
    if frame.size.width != 0 || frame.size.height != 5 {
        return 0
    }
    return 42
}
ZI

"$ziran" bundle --root "$work" --entry fields_host:Answer \
    -o "$work/fields.zib" "$work/fields_host.zi"
"$host_test" "$work/fields.zib"
