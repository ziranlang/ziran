#!/bin/sh
set -eu

ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/exported.zi" <<'ZI'
#program_export "native_answer" Answer :: () -> s32 {
    return Helper()
}
#program_export
Helper :: () -> s32 { return 42 }
#program_export "standalone_answer"
Standalone :: () -> s32 { return Answer() }
ZI
"$ziran" check --root "$work" "$work/exported.zi"
"$ziran" ir --root "$work" -o "$work/ir" "$work/exported.zi"
"$ziran" check --root "$work/ir" "$work/ir/exported.zir"
"$ziran" bundle --root "$work" --entry exported:Answer \
    -o "$work/source.zx" "$work/exported.zi"
"$ziran" bundle --root "$work/ir" --entry exported:Answer \
    -o "$work/saved.zx" "$work/ir/exported.zir"
cmp "$work/source.zx" "$work/saved.zx"
test "$("$ziran" run "$work/saved.zx")" = 42

cat > "$work/main.c" <<'C'
#include "exported.h"
int main(void) {
    return native_answer() == 42 && standalone_answer() == 42 &&
           Helper() == 42 ? 0 : 1;
}
C
for input in source saved; do
    if test "$input" = source; then
        root=$work
        file=$work/exported.zi
    else
        root=$work/ir
        file=$work/ir/exported.zir
    fi
    out="$work/c-$input"
    "$ziran" build --target=c --root "$root" -o "$out" "$file"
    ${CC:-cc} -Iinclude -I"$out" "$out/exported.c" \
        "$work/main.c" -o "$out/main"
    "$out/main"
done

cat > "$work/plain.zi" <<'ZI'
#program_export Plain :: () -> s32 { return 42 }
ZI
"$ziran" ir --root "$work" -o "$work/plain-ir" "$work/plain.zi"
for input in "$work/plain.zi" "$work/plain-ir/plain.zir"; do
    "$ziran" build --target=go --root "$work" -o "$work/plain-go" "$input"
done

for input in "$work/exported.zi" "$work/ir/exported.zir"; do
    "$ziran" build --target=go --pkg main --no-main --root "$work" \
        -o "$work/alias-go" "$input"
    cat > "$work/alias-go/main.go" <<'GO'
package main
func main() {
    if native_answer() != 42 || standalone_answer() != 42 || Exported_Helper() != 42 {
        panic("native Go export name")
    }
}
GO
    GO111MODULE=off go run "$work/alias-go"/*.go
done

cat > "$work/invalid.zi" <<'ZI'
#program_export "bad-name" Broken :: () -> s32 { return 42 }
ZI
if "$ziran" check --root "$work" "$work/invalid.zi" \
    2> "$work/invalid.err"; then
    echo 'invalid linker symbol was accepted' >&2
    exit 1
fi
grep -Fq '#program_export symbol must be an identifier' "$work/invalid.err"

cat > "$work/keywords.zi" <<'ZI'
#program_export
switch :: () -> s64 { return 39 }
#program_export
ziran_keyword_switch_0 :: () -> s64 { return 1 }
#program_export "class"
Legacy :: () -> s64 { return 2 }
#program_export
Answer :: () -> s64 {
    return switch() + ziran_keyword_switch_0() + Legacy()
}
ZI
"$ziran" ir --root "$work" -o "$work/keywords-ir" "$work/keywords.zi"
cat > "$work/keyword_main.c" <<'C'
#include "keywords.h"
int main(void) {
    return Answer() == 42 && ziran_keyword_switch_1() == 39 &&
           ziran_keyword_switch_0() == 1 && ziran_keyword_class_0() == 2
               ? 0 : 1;
}
C
for input in source saved; do
    if test "$input" = source; then
        root=$work
        file=$work/keywords.zi
    else
        root=$work/keywords-ir
        file=$work/keywords-ir/keywords.zir
    fi
    "$ziran" bundle --root "$root" --entry keywords:Answer \
        -o "$work/keywords-$input.zx" "$file"
    test "$("$ziran" run "$work/keywords-$input.zx")" = 42
    out="$work/keywords-c-$input"
    "$ziran" build --target=c --root "$root" -o "$out" "$file"
    "${CC:-cc}" -std=c99 -pedantic-errors -Iinclude -I"$out" \
        "$out/keywords.c" "$work/keyword_main.c" -o "$out/main"
    "$out/main"
    nm "$out/main" | grep -Eq ' T switch$'
    nm "$out/main" | grep -Eq ' T class$'
done
cmp "$work/keywords-source.zx" "$work/keywords-saved.zx"
