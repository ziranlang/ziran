#!/bin/sh
set -eu

# A parameter or local passed straight to a call must stay the local even
# when an imported module declares a function with the same name.
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/lib.zi" <<'ZI'
manual :: (x: *s32) {
    x.* = 1
}

config :: () -> s32 {
    return 99
}
ZI
cat > "$work/app.zi" <<'ZI'
#import, file "lib.zi";

Sum :: (left: s32, right: s32) -> s32 {
    return left + right
}

Pick :: (manual: s32) -> s32 {
    config: s32 = manual + 1
    return Sum(manual, config)
}

#program_export
Answer :: () -> s32 {
    if Pick(20) != 41 { return 0 }
    return 1
}
ZI

"$ziran" bundle --root "$work" --entry app:Answer -o "$work/app.zib" \
    "$work/app.zi"
test "$("$ziran" run "$work/app.zib")" = 1

"$ziran" build --target=c --entry app:Answer --root "$work" \
    -o "$work/c" "$work/app.zi"
cat > "$work/c/main.c" <<'C'
#include "app.h"
int main(void) { return Answer() == 1 ? 0 : 1; }
C
"${CC:-cc}" -std=c99 -pedantic-errors -I"$repo/include" -I"$work/c" \
    "$work"/c/*.c -o "$work/c/app"
"$work/c/app"

"$ziran" build --target=cpp --entry app:Answer --root "$work" \
    -o "$work/cpp" "$work/app.zi"
cat > "$work/cpp/main.cpp" <<'CPP'
#include "app.hpp"
int main() { return Answer() == 1 ? 0 : 1; }
CPP
"${CXX:-c++}" -std=c++17 -I"$repo/include" -I"$work/cpp" \
    "$work"/cpp/*.cpp -o "$work/cpp/app"
"$work/cpp/app"

if command -v go >/dev/null 2>&1; then
    "$ziran" build --target=go --pkg main --entry app:Answer --root "$work" \
        -o "$work/go" "$work/app.zi"
    cat > "$work/go/main.go" <<'GO'
package main
func main() { if App_Answer() != 1 { panic("local shadows import") } }
GO
    GO111MODULE=off go run "$work"/go/*.go
fi
