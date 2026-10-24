#!/bin/sh
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/lib/native_answer.sh"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/lib.zi" <<'ZI'
Sum :: (first: s32, second: s32) -> s32 { return first + second }
ZI
cat > "$work/app.zi" <<'ZI'
#import "lib"
FROM_RUN :: #run Sum(second = 2, first = 40);
#assert FROM_RUN == 42
calls: s32;
Step :: (value: s32) -> s32 {
    calls += 1
    return calls * 10 + value
}
Pack :: (first: s32, second: s32) -> s32 {
    return first * 100 + second
}
Pair :: struct { value: s32; }
Read :: (record: Pair, extra: s32) -> s32 {
    return record.value + extra
}
Pick :: (first: $T, second: T) -> T { return first }
Operation :: #type (first: s32, second: s32) -> s32;
#program_export
Answer :: () -> s32 {
    ordered: s32 = Pack(second = Step(2), first = Step(1))
    if ordered != 2112 || calls != 2 { return 0 }
    one: s32 = 1
    two: s32 = 2
    if Pick(second = two, first = one) != 1 { return 0 }
    operation: Operation = Pack
    if operation(second = 2, first = 4) != 402 { return 0 }
    if Read(extra = 2, record = Pair.{value = 40}) != 42 { return 0 }
    if Sum(second = 2, first = 40) != FROM_RUN { return 0 }
    return 42
}
ZI

"$ziran" check --root "$work" "$work/app.zi"
"$ziran" ir --root "$work" -o "$work/ir" "$work/app.zi"
"$ziran" bundle --root "$work" --entry app:Answer \
    -o "$work/source.zx" "$work/app.zi"
"$ziran" bundle --root "$work/ir" --entry app:Answer \
    -o "$work/saved.zx" "$work/ir/app.zir"
cmp "$work/source.zx" "$work/saved.zx"
test "$("$ziran" run "$work/source.zx")" = 42
test "$("$ziran" run "$work/saved.zx")" = 42

for input in "$work/app.zi" "$work/ir/app.zir"; do
    case "$input" in
        *.zi) suffix=source; root=$work ;;
        *) suffix=saved; root=$work/ir ;;
    esac
    for target in c go; do
        out="$work/$target-$suffix"
        native_answer_42 "$target" "$root" "$out" "$input" \
            app App_Answer 'named arguments'
    done
done

cat > "$work/unknown.zi" <<'ZI'
Pack :: (first: s32, second: s32) -> s32 { return first + second }
Answer :: () -> s32 { return Pack(unknown = 1, second = 2) }
ZI
if "$ziran" check --root "$work" "$work/unknown.zi" 2> "$work/unknown.err"; then
    echo 'unknown named argument was accepted' >&2
    exit 1
fi
grep -Fq 'unknown named argument: unknown' "$work/unknown.err"

cat > "$work/duplicate.zi" <<'ZI'
Pack :: (first: s32, second: s32) -> s32 { return first + second }
Answer :: () -> s32 { return Pack(first = 1, first = 2) }
ZI
if "$ziran" check --root "$work" "$work/duplicate.zi" 2> "$work/duplicate.err"; then
    echo 'duplicate named argument was accepted' >&2
    exit 1
fi
grep -Fq 'duplicate call argument: first' "$work/duplicate.err"
