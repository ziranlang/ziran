#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$repo/build/test"
work=$(mktemp -d "$repo/build/test/callback-type-order.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY
cat > "$work/callbacks.zi" <<'ZI'
Mode :: enum { Increment :: 0; }
Node :: struct {
    #abi_incomplete
    value: s32
    visit: Visit
}
Visit :: #type (node: *Node, mode: Mode) -> s32 #c_call;
Add :: (node: *Node, mode: Mode) -> s32 {
    if mode == Mode.Increment { node.value += 1 }
    return node.value
}
Read :: () -> s32 {
    node: Node
    node.value = 41; node.visit = Add
    return node.visit(*node, Mode.Increment)
}
#scope_file
PrivateNode :: struct { value: s32; }
PrivateVisit :: #type (node: *PrivateNode) -> s32 #c_call;
PrivateAdd :: (node: *PrivateNode) -> s32 { return node.value }
#scope_export
ReadPrivate :: () -> s32 {
    node: PrivateNode
    node.value = 42
    visit: PrivateVisit = PrivateAdd
    return visit(*node)
}
ZI
cat > "$work/app.zi" <<'ZI'
Callbacks :: #import "callbacks";
#program_export
main :: () -> s32 {
    return ifx Callbacks.Read() == 42 && Callbacks.ReadPrivate() == 42 then 0 else 1
}
ZI
"$ziran" ir --root "$work" -o "$work/ir" "$work/app.zi"
for form in source saved; do
    root=$work
    input=$work/app.zi
    if test "$form" = saved; then root=$work/ir; input=$root/app.zir; fi
    for target in c cpp; do
        out=$work/$form-$target
        "$ziran" build --target="$target" --root "$root" -o "$out" "$input"
        if test "$target" = c; then compiler=${CC:-cc}; standard=c11; extension=c
        else compiler=${CXX:-c++}; standard=c++17; extension=cpp; fi
        "$compiler" -std="$standard" -I"$out" "$out"/*."$extension" -o "$out/run"
        timeout --kill-after=2s 10s "$out/run"
    done
done
echo 'Native callbacks over own public/private records and enums passed C/C++ source and saved IR'
