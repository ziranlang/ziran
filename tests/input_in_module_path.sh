#!/bin/sh
set -eu

# A module passed by file from a module path outside the root is named as
# if imported: its C output is flat and includes its own header by stem,
# not by the absolute location of its source.
ziran=${1:?pass the ziran command}
bin=$(CDPATH= cd -- "$(dirname -- "$ziran")" && pwd)
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

mkdir -p "$work/app" "$work/lib"
cat > "$work/lib/helper.zi" <<'ZI'
Twice :: (x: s32) -> s32 {
    return x * 2
}
ZI
cat > "$work/app/main.zi" <<'ZI'
#import "helper"

#program_export
Answer :: () -> s32 {
    return Twice(21)
}
ZI

"$bin/zi2c" --no-main --root "$work/app" --module-path "$work/lib" \
    -o "$work/out" "$work/app/main.zi" "$work/lib/helper.zi"
test -f "$work/out/helper.c"
test -f "$work/out/helper.h"
if grep -q "#include \"$work" "$work/out/helper.c"; then
    echo "module passed by file includes its header by absolute path" >&2
    exit 1
fi
cat > "$work/out/run.c" <<'C'
#include "main.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
"${CC:-cc}" -std=c99 -I"$repo/include" -I"$work/out" "$work"/out/*.c \
    -o "$work/app/run"
"$work/app/run"
