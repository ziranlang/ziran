#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
bin=$(dirname "${1:-$repo/build/bin/ziran}")
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/first.zi" <<'ZI'
Session :: struct { value: s32 }
ZI
cat > "$work/second.zi" <<'ZI'
Session :: struct { other: s32 }
ZI
cat > "$work/callback.zi" <<'ZI'
#import, file "first.zi";
Slot :: #type (session: Session) -> Session;
Pass :: (session: Session) -> Session { return session }
#program_export
SlotAnswer :: () -> s32 {
    callback: Slot = Pass
    input: Session = Session.{.value = 40}
    output: Session = callback(input)
    return output.value
}
ZI
cat > "$work/main.zi" <<'ZI'
#import, file "callback.zi";
Other :: #import, file "second.zi";
#program_export
Answer :: () -> s32 {
    other: Other.Session = Other.Session.{.other = 2}
    return SlotAnswer() + other.other
}
ZI

"$bin/zi2c" --no-main --root "$work" -o "$work/c" "$work/main.zi"
cat > "$work/c/entry.c" <<'C'
#include "main.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
"${CC:-cc}" -std=c11 -I"$repo/include" -I"$work/c" \
    "$work/c"/*.c -o "$work/c/app"
"$work/c/app"

echo "Native callback types survive duplicate imported record names"
