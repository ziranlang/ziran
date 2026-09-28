#!/bin/sh
set -eu

# cast(Enum) of a plain s32 name must stay a cast in every target: C++ and Go
# reject returning an integer where an enum is declared.
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
Zone :: enum {
    ZoneNone :: 0
    ZoneLeft :: 1
    ZoneRight :: 2
}
using Zone;

ZoneValue :: (x: s32) -> s32 {
    if x > 0 { return 2 }
    return 0
}

FromCall :: (x: s32) -> Zone {
    return cast(Zone)ZoneValue(x)
}

FromName :: (x: s32) -> Zone {
    return cast(Zone)x
}

#program_export
Answer :: () -> s32 {
    if cast(s32)FromCall(5) != ZoneRight { return 0 }
    if cast(s32)FromCall(0) != ZoneNone { return 0 }
    if cast(s32)FromName(1) != ZoneLeft { return 0 }
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
func main() { if App_Answer() != 1 { panic("enum cast") } }
GO
    GO111MODULE=off go run "$work"/go/*.go
fi
