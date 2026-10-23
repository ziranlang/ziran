#!/bin/sh
set -eu

ziran=${1:?pass the ziran command}
vm_test=${2:?pass the VM scope test executable}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/scope.zi" <<'ZI'
#import "vec"
host_api :: #system_library "host_api";
ProbeHost :: () -> s64 #foreign host_api;

Inner :: () -> s32 {
    values: Vec(s32)
    VecPush(values, 7)
    return values[0]
}

Make :: () -> Vec(s32) {
    values: Vec(s32)
    VecPush(values, 7)
    return values
}

Take :: (values: Vec(s32)) -> s32 { return values[0] }

#program_export
Check :: () -> s32 {
    if Inner() != 7 { return 1 }
    if ProbeHost() != 0 { return 2 }
    {
        values: Vec(s32)
        VecPush(values, 1)
    }
    if ProbeHost() != 0 { return 3 }
    index: s32 = 0
    while index < 3 {
        values: Vec(s32)
        VecPush(values, index)
        index += 1
        if index < 3 { continue }
        break
    }
    if ProbeHost() != 0 { return 4 }
    transferred: Vec(s32) = Make()
    if Take(transferred) != 7 { return 5 }
    if ProbeHost() != 0 { return 6 }
    if Take(Make()) != 7 { return 7 }
    if ProbeHost() != 0 { return 8 }
    Make()
    if ProbeHost() != 0 { return 9 }
    return 0
}
ZI

"$ziran" ir --root "$work" --module-path "$repo/std" \
    -o "$work/ir" "$work/scope.zi"
for input in source saved; do
    if test "$input" = source; then
        module=$work/scope.zi
        root=$work
    else
        module=$work/ir/scope.zir
        root=$work/ir
    fi
    "$ziran" bundle --root "$root" --module-path "$repo/std" \
        --entry scope:Check -o "$work/$input.zx" "$module"
    "$vm_test" "$work/$input.zx"
done
