#!/bin/sh
set -eu

ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/double.zi" <<'ZI'
#import "vec"
TakeTwo :: (a: Vec(s32), b: Vec(s32)) -> s64 { return a.count + b.count }
Check :: () -> s64 {
    values: Vec(s32)
    VecPush(values, 7)
    return TakeTwo(values, values)
}
ZI

cat > "$work/read_after.zi" <<'ZI'
#import "vec"
Take :: (value: Vec(s32)) -> s64 { return value.count }
Check :: () -> s64 {
    values: Vec(s32)
    VecPush(values, 7)
    return Take(values) + values.count
}
ZI

cat > "$work/push_after.zi" <<'ZI'
#import "vec"
Take :: (value: Vec(s32)) -> s32 { return value[0] }
Check :: () -> s32 {
    values: Vec(s32)
    VecPush(values, 7)
    VecPush(values, Take(values))
    return 0
}
ZI

cat > "$work/temporary_member.zi" <<'ZI'
#import "vec"
Make :: () -> Vec(s32) {
    values: Vec(s32)
    VecPush(values, 7)
    return values
}
Check :: () -> s64 { return Make().count }
ZI

for name in double read_after push_after temporary_member; do
    if "$ziran" check --diagnostics=json --root "$work" \
        --module-path "$repo/std" "$work/$name.zi" \
        > "$work/$name.out" 2> "$work/$name.err"; then
        echo "$name reused an owned vector after moving it" >&2
        exit 1
    fi
    "$ziran" run --target=c --root "$repo/tests" "$repo/tests/json_assertions.zi" -- move "$work/$name.err"
done
