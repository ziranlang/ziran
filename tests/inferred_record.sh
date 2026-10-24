#!/bin/sh
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/inferred.zi" <<'ZI'
Inner :: struct {
    value: s32
}
Props :: struct {
    inner: Inner
    scale: s32
}
Make :: () -> Props {
    return .{inner = .{value = 40}, scale = 2}
}
Read :: (props: Props) -> s32 {
    return props.inner.value + props.scale
}
#program_export
Answer :: () -> s32 {
    if Read(Make()) != 42 { return 0 }
    local: Props = .{inner = .{value = 41}, scale = 1}
    if Read(local) != 42 { return 0 }
    local.inner = .{value = 42}
    local.scale = 0
    if Read(local) != 42 { return 0 }
    return Read(.{inner = .{value = 42}, scale = 0})
}
ZI

"$ziran" ir --root "$work" -o "$work/ir" "$work/inferred.zi"
for input in source saved; do
    if test "$input" = source; then
        set -- "$work/inferred.zi"
    else
        set -- "$work/ir/inferred.zir"
    fi
    "$ziran" bundle --root "$work" --entry inferred:Answer \
        -o "$work/$input.zx" "$@"
    test "$("$ziran" run "$work/$input.zx")" = 42
    for target in c go; do
        out="$work/$input-$target"
        if test "$target" = go; then
            "$ziran" build --target=go --pkg main --root "$work" \
                -o "$out" "$@"
        else
            "$ziran" build --target="$target" --root "$work" \
                -o "$out" "$@"
        fi
        if test "$target" = c; then
            cat > "$out/main.c" <<'C'
#include "inferred.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
            "${CC:-cc}" -std=c11 -I"$repo/include" -I"$out" \
                "$out/inferred.c" "$out/main.c" -o "$out/app"
            "$out/app"
        else
            cat > "$out/main.go" <<'GO'
package main
func main() { if Inferred_Answer() != 42 { panic("inferred record") } }
GO
            GO111MODULE=off go run "$out/inferred.go" "$out/main.go"
        fi
    done
done
cmp "$work/source.zx" "$work/saved.zx"

cat > "$work/no_context.zi" <<'ZI'
Answer :: () {
    value := .{value = 42}
}
ZI
if "$ziran" check --root "$work" "$work/no_context.zi" \
    2> "$work/no_context.err"; then
    echo 'record literal without a type context was accepted' >&2
    exit 1
fi
grep -Fq 'inferred record literal needs a record type' \
    "$work/no_context.err"

cat > "$work/unknown_field.zi" <<'ZI'
Props :: struct {
    value: s32
}
Make :: () -> Props {
    return .{missing = 42}
}
ZI
if "$ziran" check --root "$work" "$work/unknown_field.zi" \
    2> "$work/unknown_field.err"; then
    echo 'unknown inferred record field was accepted' >&2
    exit 1
fi
grep -Fq 'unknown initializer field' "$work/unknown_field.err"
