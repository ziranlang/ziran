#!/bin/sh
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/lib/native_answer.sh"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/phase.zi" <<'ZI'
Flag :: () -> bool {
    return #compile_time
}
AtCompileTime :: () -> s64 {
    if #compile_time { return 42 }
    return 0
}
VALUE :: #run AtCompileTime();
DIRECT :: #run #compile_time;
#assert VALUE == 42 && DIRECT && Flag()
#program_export
Answer :: () -> s32 {
    if Flag() { return 0 }
    if DIRECT == 0 { return 0 }
    return VALUE
}
ZI

"$ziran" check --root "$work" "$work/phase.zi"
"$ziran" ir --root "$work" -o "$work/ir" "$work/phase.zi"
"$ziran" check --root "$work/ir" "$work/ir/phase.zir"
"$ziran" bundle --root "$work" --entry phase:Answer \
    -o "$work/source.zx" "$work/phase.zi"
"$ziran" bundle --root "$work/ir" --entry phase:Answer \
    -o "$work/saved.zx" "$work/ir/phase.zir"
cmp "$work/source.zx" "$work/saved.zx"
test "$("$ziran" run "$work/source.zx")" = 42
test "$("$ziran" run "$work/saved.zx")" = 42

for input in source saved; do
    if test "$input" = source; then
        root=$work
        file=$work/phase.zi
    else
        root=$work/ir
        file=$work/ir/phase.zir
    fi
    for target in c go; do
        out="$work/$target-$input"
        native_answer_42 "$target" "$root" "$out" "$file" \
            phase Phase_Answer 'compile-time phase'
    done
done

cat > "$work/not_constant.zi" <<'ZI'
VALUE :: #compile_time;
ZI
if "$ziran" check --root "$work" "$work/not_constant.zi" \
    2> "$work/not_constant.err"; then
    echo '#compile_time was accepted as a constant' >&2
    exit 1
fi
grep -Fq '#compile_time cannot be used as a constant' \
    "$work/not_constant.err"

cat > "$work/not_condition.zi" <<'ZI'
#if #compile_time || true {
VALUE :: 42;
}
ZI
if "$ziran" check --root "$work" "$work/not_condition.zi" \
    2> "$work/not_condition.err"; then
    echo '#compile_time was accepted as a #if constant' >&2
    exit 1
fi
grep -Fq '#if condition is not a compile-time constant' \
    "$work/not_condition.err"

cat > "$work/quoted.zi" <<'ZI'
TEXT :: "#compile_time is literal text";
ZI
"$ziran" check --root "$work" "$work/quoted.zi"
