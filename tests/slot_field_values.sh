#!/bin/sh
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/slots.zi" <<'EOF'
Callback :: #type (value: s32) -> s32;
Holder :: struct {
    apply: Callback
}

double_it :: (value: s32) -> s32 {
    return value * 2
}

call_holder :: (holder: *Holder, value: s32) -> s32 {
    if holder.apply != null {
        return holder.apply(value)
    }
    return -1
}

#program_export
Answer :: () -> s32 {
    holder: Holder
    if call_holder(*holder, 21) != -1 { return 0 }
    holder.apply = double_it
    same: Callback = holder.apply
    if holder.apply != same { return 0 }
    empty: Holder
    holder.apply = empty.apply
    if call_holder(*holder, 21) != -1 { return 0 }
    holder.apply = same
    if holder.apply == null {
        return 0
    }
    return call_holder(*holder, 21)
}
EOF

"$ziran" check --root "$work" "$work/slots.zi"
"$ziran" ir --root "$work" -o "$work/ir" "$work/slots.zi"
"$ziran" bundle --root "$work" --entry slots:Answer \
    -o "$work/source.zib" "$work/slots.zi"
"$ziran" bundle --root "$work/ir" --entry slots:Answer \
    -o "$work/saved.zib" "$work/ir/slots.zir"
cmp "$work/source.zib" "$work/saved.zib"
test "$("$ziran" run "$work/source.zib")" = 42
test "$("$ziran" run "$work/saved.zib")" = 42
if grep -aFq 'unresolved' "$work/ir/slots.zir"; then
    echo 'slot field call was not resolved' >&2
    exit 1
fi
"$ziran" build --target=c --root "$work" -o "$work/c" "$work/slots.zi"
cat > "$work/c/main.c" <<'EOF'
#include "slots.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
EOF
${CC:-cc} -I"$repo/include" -I"$work/c" "$work/c/slots.c" "$work/c/main.c" -o "$work/c/app"
"$work/c/app"
