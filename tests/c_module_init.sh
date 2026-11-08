#!/bin/sh
set -eu

ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work="$repo/build/c-module-init"
mkdir -p "$work"

# mail's old guard collided with mail_state's initializer declaration.
# Constructors need distinct names too.
cat > "$work/mail_state.zi" <<'ZI'
count: s32;
Next :: () -> s32 { count += 1; return count }
value: s32 = Next();
Get :: () -> s32 { return value }
Count :: () -> s32 { return count }
ZI
cat > "$work/mail.zi" <<'ZI'
State :: #import "mail_state";
Constructor :: #import "mail_constructor";
value: s32 = State.Next();
Get :: () -> s32 { return value + Constructor.Get() - 2 }
ZI
cat > "$work/mail_constructor.zi" <<'ZI'
State :: #import "mail_state";
Get :: () -> s32 { return State.Get() + 1 }
ZI
cat > "$work/app.zi" <<'ZI'
Mail :: #import "mail";
State :: #import "mail_state";
Constructor :: #import "mail_constructor";
#program_export
Answer :: () -> s32 {
    if State.Count() != 2 { return -1 }
    return Mail.Get() + State.Get() + Constructor.Get() + 37
}
ZI

"$ziran" ir --root "$work" -o "$work/ir" "$work/app.zi"
for input in source saved; do
    case "$input" in
        source) root=$work; file=$work/app.zi ;;
        saved) root=$work/ir; file=$work/ir/app.zir ;;
    esac
    out="$work/$input-c"
    "$ziran" build --target=c --root "$root" -o "$out" "$file"
    cat > "$out/main.c" <<'C'
#include "app.h"
int main(void) { return Answer() == 42 && Answer() == 42 ? 0 : 1; }
C
    "${CC:-cc}" -std=c11 -I"$repo/include" -I"$out" "$out"/*.c -o "$out/program"
    "$out/program"
done
# Exercise the native engine's explicit startup calls and per-instance guards
# as well as the C constructors tested above.
export ZIRAN_NATIVE_CACHE="$work/cache"
test "$("$ziran" run --root "$work" --entry app:Answer "$work/app.zi")" = 42
test "$("$ziran" run --vm --root "$work" --entry app:Answer "$work/app.zi")" = 42

# An actual startup cycle must still abort rather than recurse indefinitely
# or silently skip an initializer. Run only this test's disposable process.
cat > "$work/mail_state.zi" <<'ZI'
Mail :: #import "mail";
count: s32;
Next :: () -> s32 { count += 1; return count }
value: s32 = Next();
Get :: () -> s32 { return value }
Count :: () -> s32 { return count }
ZI
out="$work/cycle-c"
"$ziran" build --target=c --root "$work" -o "$out" "$work/app.zi"
cp "$work/source-c/main.c" "$out/main.c"
"${CC:-cc}" -std=c11 -I"$repo/include" -I"$out" "$out"/*.c -o "$out/program"
ulimit -c 0
status=0
"$out/program" > "$work/cycle.log" 2>&1 || status=$?
test "$status" -eq 134
echo 'C module initializer names, one-time startup and cycle detection: passed'
