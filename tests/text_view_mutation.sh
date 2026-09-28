#!/bin/sh
set -eu

ziran=${1:?pass the ziran command}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/direct.zi" <<'ZI'
Bad :: () -> s32 {
    bytes: [1]u8
    text := TextView(bytes[:])
    bytes[0] = cast(u8)98
    return cast(s32)text[0]
}
ZI

cat > "$work/field.zi" <<'ZI'
Box :: struct { text: string; }
Bad :: () -> s32 {
    bytes: [1]u8
    value: Box
    value.text = TextView(bytes[:])
    bytes[0] = cast(u8)98
    return cast(s32)value.text[0]
}
ZI

cat > "$work/nested.zi" <<'ZI'
Bad :: () -> s32 {
    bytes: [1]u8
    outer := TextView(bytes[:])
    {
        inner := TextView(bytes[:])
    }
    bytes[0] = cast(u8)98
    return cast(s32)outer[0]
}
ZI

cat > "$work/scope.zi" <<'ZI'
Good :: () -> s32 {
    bytes: [1]u8 = .[97]
    {
        text := TextView(bytes[:])
    }
    bytes[0] = cast(u8)98
    return cast(s32)bytes[0]
}
ZI

cat > "$work/pointer_direct.zi" <<'ZI'
Node :: struct { values: [1]u8; }
Bad :: () -> s32 {
    node: Node
    pointer := *node
    text := TextView(pointer.values[:])
    node.values[0] = cast(u8)98
    return cast(s32)text[0]
}
ZI

cat > "$work/pointer_alias.zi" <<'ZI'
Node :: struct { values: [1]u8; }
Bad :: () -> s32 {
    node: Node
    pointer := *node
    text := TextView(pointer.values[:])
    pointer.values[0] = cast(u8)98
    return cast(s32)text[0]
}
ZI

cat > "$work/pointer_scope.zi" <<'ZI'
Node :: struct { values: [1]u8; }
Good :: () -> s32 {
    node: Node
    pointer := *node
    {
        text := TextView(pointer.values[:])
    }
    node.values[0] = cast(u8)98
    pointer.values[0] += cast(u8)1
    return cast(s32)node.values[0]
}
ZI

cat > "$work/sibling_field.zi" <<'ZI'
Node :: struct { values: [1]u8; result: s32; }
Good :: () -> s32 {
    node: Node
    text := TextView(node.values[:])
    node.result = 7
    return node.result + cast(s32)text[0]
}
ZI

cat > "$work/pointer_sibling_field.zi" <<'ZI'
Node :: struct { values: [1]u8; result: s32; }
Good :: () -> s32 {
    node: Node
    pointer := *node
    text := TextView(pointer.values[:])
    pointer.result = 7
    node.result = 8
    return node.result + cast(s32)text[0]
}
ZI

cat > "$work/view_reassignment.zi" <<'ZI'
Good :: (body: string) -> s64 {
    body = body[1:body.count]
    body = body[1:body.count]
    return body.count
}
ZI

cat > "$work/enclosing_record.zi" <<'ZI'
Node :: struct { values: [1]u8; result: s32; }
Bad :: () -> s32 {
    node: Node
    text := TextView(node.values[:])
    node = Node.{}
    return cast(s32)text[0]
}
ZI

cat > "$work/call_alias.zi" <<'ZI'
Alias :: (bytes: []u8) -> string { return TextView(bytes[:]) }
Bad :: () -> s32 {
    bytes: [1]u8
    text := Alias(bytes[:])
    bytes[0] = cast(u8)98
    return cast(s32)text[0]
}
ZI

cat > "$work/call_second_alias.zi" <<'ZI'
Choose :: (first: []u8, second: []u8, choose_first: bool) -> string {
    if choose_first { return TextView(first[:]) }
    return TextView(second[:])
}
Bad :: () -> s32 {
    first: [1]u8
    second: [1]u8
    text := Choose(first[:], second[:], false)
    second[0] = cast(u8)98
    return cast(s32)text[0]
}
ZI

cat > "$work/conditional_alias.zi" <<'ZI'
Bad :: (use_first: bool) -> s32 {
    first: [1]u8
    second: [1]u8
    text := ifx use_first then TextView(first[:]) else TextView(second[:])
    second[0] = cast(u8)98
    return cast(s32)text[0]
}
ZI

cat > "$work/record_literal_alias.zi" <<'ZI'
Box :: struct { text: string; }
Bad :: () -> s32 {
    bytes: [1]u8
    value := Box.{text = TextView(bytes[:])}
    bytes[0] = cast(u8)98
    return cast(s32)value.text[0]
}
ZI

cat > "$work/record_return.zi" <<'ZI'
Box :: struct { text: string; }
Alias :: (bytes: []u8) -> Box { return Box.{text = TextView(bytes[:])} }
Bad :: () -> s32 {
    bytes: [1]u8
    value := Alias(bytes[:])
    bytes[0] = cast(u8)98
    return cast(s32)value.text[0]
}
ZI

for name in direct field nested pointer_direct pointer_alias call_alias call_second_alias conditional_alias record_literal_alias record_return enclosing_record; do
    if "$ziran" check --diagnostics=json --root "$work" \
        "$work/$name.zi" > "$work/$name.out" 2> "$work/$name.err"; then
        echo "$name accepted mutation of live text backing storage" >&2
        exit 1
    fi
    rg -Fq 'mutating text backing storage while its view is live' \
        "$work/$name.err"
done

for target in c cpp go; do
    if "$ziran" build "--target=$target" --root "$work" \
        -o "$work/direct-$target" "$work/direct.zi" \
        > "$work/direct-$target.out" 2> "$work/direct-$target.err"; then
        echo "$target accepted mutation of live text backing storage" >&2
        exit 1
    fi
    rg -Fq 'mutating text backing storage while its view is live' \
        "$work/direct-$target.err"
done

"$ziran" check --root "$work" "$work/scope.zi"
"$ziran" check --root "$work" "$work/pointer_scope.zi"
"$ziran" check --root "$work" "$work/sibling_field.zi"
"$ziran" check --root "$work" "$work/pointer_sibling_field.zi"
"$ziran" check --root "$work" "$work/view_reassignment.zi"
"$ziran" ir --root "$work" -o "$work/ir" "$work/scope.zi"
"$ziran" check --root "$work/ir" "$work/ir/scope.zir"
