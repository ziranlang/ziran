#!/bin/sh
set -eu

ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
mkdir -p "$work/src" "$work/ir"

cat > "$work/src/base.zi" <<'ZI'
Base :: () -> s32 { return 39 }
ZI
cat > "$work/src/main.zi" <<'ZI'
#import, string #string SOURCE
#import "base"
#if Base() == 39 {
Inline :: () -> s32 { return Base() + 1 }
} else {
Inline :: () -> s32 { return Missing() }
}
SOURCE;
Generated :: #import, string "One :: () -> s32 { return 2 }";
#program_export
Result :: () -> s32 { return Inline() + Generated.One() }
ZI

"$ziran" check --root "$work/src" "$work/src/main.zi"
"$ziran" ir --root "$work/src" -o "$work/ir" "$work/src/main.zi"
"$ziran" bundle --root "$work/src" --entry main:Result \
    -o "$work/source.zib" "$work/src/main.zi"
"$ziran" bundle --root "$work/ir" --entry main:Result \
    -o "$work/saved.zib" "$work/ir/main.zir"
cmp "$work/source.zib" "$work/saved.zib"
test "$("$ziran" run "$work/saved.zib")" = 42

for input in source saved; do
    if test "$input" = source; then
        root=$work/src
        file=$work/src/main.zi
    else
        root=$work/ir
        file=$work/ir/main.zir
    fi
    "$ziran" build --target=c --root "$root" \
        -o "$work/c-$input" "$file"
    cat > "$work/c-$input/test.c" <<'C'
#include "main.h"
int main(void) { return Result() == 42 ? 0 : 1; }
C
    ${CC:-cc} -Iinclude -I"$work/c-$input" \
        "$work/c-$input"/*.c -o "$work/c-app-$input"
    "$work/c-app-$input"

    "$ziran" build --target=cpp --root "$root" \
        -o "$work/cpp-$input" "$file"
    cat > "$work/cpp-$input/test.cpp" <<'CPP'
#include "main.hpp"
int main() { return Result() == 42 ? 0 : 1; }
CPP
    ${CXX:-c++} -Iinclude -I"$work/cpp-$input" \
        "$work/cpp-$input"/*.cpp -o "$work/cpp-app-$input"
    "$work/cpp-app-$input"

    "$ziran" build --target=go --pkg main --root "$root" \
        -o "$work/go-$input" "$file"
    cat > "$work/go-$input/test.go" <<'GO'
package main
func main() { if Main_Result() != 42 { panic("string import changed") } }
GO
    GO111MODULE=off go run "$work/go-$input"/*.go
done

cat > "$work/src/named_leak.zi" <<'ZI'
Generated :: #import, string "One :: () -> s32 { return 2 }";
Result :: () -> s32 { return One() }
ZI
if "$ziran" check --root "$work/src" "$work/src/named_leak.zi" \
    2> "$work/named_leak.err"; then
    echo 'named string import leaked an unqualified symbol' >&2
    exit 1
fi

cat > "$work/src/bad_escape.zi" <<'ZI'
#import, string "Bad :: \q";
ZI
if "$ziran" check --root "$work/src" "$work/src/bad_escape.zi" \
    2> "$work/bad_escape.err"; then
    echo 'invalid string import escape was accepted' >&2
    exit 1
fi
grep -Fq 'unsupported #import, string escape' "$work/bad_escape.err"

# An import target longer than the name limit is rejected, not silently
# truncated into a colliding alias.
long_target=$(printf 'a%.0s' $(seq 1 130))
cat > "$work/src/long_import.zi" <<ZI
#import "$long_target"
ZI
if "$ziran" check --root "$work/src" "$work/src/long_import.zi" \
    2> "$work/long_import.err"; then
    echo 'over-long import target was accepted' >&2
    exit 1
fi
grep -Fq 'exceeds the 127-byte limit' "$work/long_import.err"
