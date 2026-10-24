#!/bin/sh
set -eu
ulimit -c 0

ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/defs.zi" <<'ZI'
ZERO :: 0
ImportedAlias :: [ZERO]s32
#scope_file
PrivateAlias :: [0]s32
#scope_module
ZI
cat > "$work/empty.zi" <<'ZI'
#import "defs"
Defs :: #import "defs";
EMPTY_SIZE :: size_of([0]s32)
EMPTY :: s32.[]
Box :: struct {
    empty: [ZERO]s32
    imported: ImportedAlias
    named: Defs.ImportedAlias
    value: s32
}
Only :: struct { empty: [0]s32 }
Nested :: struct {
    empty: Only
    value: s32
}
EmptyAlias :: [ZERO]s32
global_empty: [0]s32;
global_literal: [0]s32 = s32.[];
global_imported: ImportedAlias;
global_named: Defs.ImportedAlias;

#assert EMPTY_SIZE == 0
#assert size_of(Box) == 4
#assert size_of(Only) == 0
#assert size_of(Nested) == 4
#assert size_of([2][0]s32) == 0
#assert size_of(EmptyAlias) == 0
#assert size_of(ImportedAlias) == 0
#assert size_of(Defs.ImportedAlias) == 0
#assert s32.[].count == 0
#assert EMPTY.count == 0

Count :: (values: [0]s32) -> s64 { return values.count }
Identity :: (value: $T) -> T { return value }
ReturnEmpty :: () -> [0]s32 { return s32.[] }
AliasCount :: (values: EmptyAlias) -> s64 { return values.count }
AliasReturn :: () -> EmptyAlias { return s32.[] }
AliasDefault :: (values: EmptyAlias = s32.[]) -> s64 {
    return values.count
}
ImportedCount :: (values: ImportedAlias) -> s64 { return values.count }
NamedCount :: (values: Defs.ImportedAlias) -> s64 {
    return values.count
}

#program_export
Answer :: () -> s32 {
    typed := s32.[]
    contextual: [0]s32 = .[]
    returned: [0]s32 = ReturnEmpty()
    generic: [0]s32 = Identity(typed)
    view: []s32 = typed[:]
    aliased: EmptyAlias = s32.[]
    alias_returned: EmptyAlias = AliasReturn()
    alias_matrix: [2]EmptyAlias
    imported: ImportedAlias = s32.[]
    named: Defs.ImportedAlias = s32.[]
    record_empty := Box.[]
    matrix: [2][0]s32
    box: Box
    box.value = 42
    nested: Nested
    nested.value = box.value
    if typed.count != 0 || contextual.count != 0 ||
       returned.count != 0 || generic.count != 0 || view.count != 0 || global_empty.count != 0 ||
       global_literal.count != 0 || global_imported.count != 0 ||
       global_named.count != 0 ||
       aliased.count != 0 || alias_returned.count != 0 ||
       imported.count != 0 || ImportedCount(imported) != 0 ||
       named.count != 0 || NamedCount(named) != 0 ||
       alias_matrix.count != 2 || alias_matrix[0].count != 0 ||
       record_empty.count != 0 || matrix.count != 2 ||
       matrix[0].count != 0 || box.empty.count != 0 ||
       box.imported.count != 0 || box.named.count != 0 ||
       nested.empty.empty.count != 0 || Count(typed) != 0 ||
       AliasCount(aliased) != 0 || AliasDefault() != 0 ||
       typed.data != null || contextual.data != null ||
       returned.data != null || generic.data != null || global_empty.data != null ||
       global_literal.data != null || global_imported.data != null ||
       global_named.data != null ||
       record_empty.data != null ||
       matrix[0].data != null || box.empty.data != null ||
       box.imported.data != null || box.named.data != null ||
       EMPTY_SIZE != 0 { return 0 }
    return nested.value
}

#program_export
Bad :: () -> s32 {
    values := s32.[]
    index: s32 = 0
    return values[index]
}
ZI

"$ziran" ir --root "$work" -o "$work/ir" "$work/empty.zi"
for input in source saved; do
    if test "$input" = source; then
        root=$work
        file=$work/empty.zi
    else
        root=$work/ir
        file=$work/ir/empty.zir
    fi
    "$ziran" bundle --root "$root" --entry empty:Answer \
        -o "$work/$input.zx" "$file"
    test "$("$ziran" run "$work/$input.zx")" = 42
    "$ziran" bundle --root "$root" --entry empty:Bad \
        -o "$work/$input-bad.zx" "$file"
    if "$ziran" run "$work/$input-bad.zx" >"$work/bad.err" 2>&1; then
        echo "portable zero array index succeeded" >&2
        exit 1
    fi
    for target in c go; do
        out="$work/$target-$input"
        if test "$target" = go; then
            "$ziran" build --target=go --pkg main --root "$root" \
                -o "$out" "$file"
            cat > "$out/main.go" <<'GO'
package main
func main() { if Empty_Answer() != 42 { panic("empty arrays") } }
GO
            GO111MODULE=off go run "$out"/*.go
            cat > "$out/bad.go" <<'GO'
package main
func main() { Empty_Bad() }
GO
            if GO111MODULE=off go run "$out/empty.go" "$out/bad.go" \
                >"$work/bad.err" 2>&1; then
                echo "Go zero array index succeeded" >&2
                exit 1
            fi
        elif test "$target" = c; then
            "$ziran" build --target=c --root "$root" -o "$out" "$file"
            cat > "$out/main.c" <<'C'
#include "empty.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
            "${CC:-cc}" -std=c99 -Wall -Werror -pedantic-errors \
                -I"$repo/include" -I"$out" "$out"/*.c -o "$out/program"
            "$out/program"
            cat > "$out/bad.c" <<'C'
#include "empty.h"
int main(void) { return Bad(); }
C
            "${CC:-cc}" -std=c99 -Wall -Werror -pedantic-errors \
                -I"$repo/include" -I"$out" \
                "$out/empty.c" "$out/bad.c" -o "$out/bad"
            if "$out/bad" >"$work/bad.err" 2>&1; then
                echo "C zero array index succeeded" >&2
                exit 1
            fi
        fi
    done
done
cmp "$work/source.zx" "$work/saved.zx"

cat > "$work/bridge.zi" <<'ZI'
using Defs :: #import "defs";
ZI
cat > "$work/bridge2.zi" <<'ZI'
using Bridge :: #import "bridge";
ZI
cat > "$work/relay.zi" <<'ZI'
#import "bridge2"
Bridge2 :: #import "bridge2";
#assert size_of(ImportedAlias) == 0
#assert size_of(Bridge2.ImportedAlias) == 0
#program_export
Answer :: () -> s32 {
    value: ImportedAlias = s32.[]
    named: Bridge2.ImportedAlias = s32.[]
    if value.count != 0 || value.data != null ||
       named.count != 0 || named.data != null { return 0 }
    return 42
}
ZI
"$ziran" ir --root "$work" -o "$work/relay-ir" "$work/relay.zi"
"$ziran" bundle --root "$work" --entry relay:Answer \
    -o "$work/relay-source.zx" "$work/relay.zi"
"$ziran" bundle --root "$work/relay-ir" --entry relay:Answer \
    -o "$work/relay-saved.zx" "$work/relay-ir/relay.zir"
cmp "$work/relay-source.zx" "$work/relay-saved.zx"
test "$("$ziran" run "$work/relay-source.zx")" = 42
test "$("$ziran" run "$work/relay-saved.zx")" = 42
for input in source saved; do
    if test "$input" = source; then
        root=$work
        file=$work/relay.zi
    else
        root=$work/relay-ir
        file=$work/relay-ir/relay.zir
    fi
    for target in c go; do
        out="$work/relay-$target-$input"
        if test "$target" = go; then
            "$ziran" build --target=go --pkg main --root "$root" \
                -o "$out" "$file"
            cat > "$out/main.go" <<'GO'
package main
func main() { if Relay_Answer() != 42 { panic("re-exported array") } }
GO
            GO111MODULE=off go run "$out"/*.go
        elif test "$target" = c; then
            "$ziran" build --target=c --root "$root" -o "$out" "$file"
            cat > "$out/main.c" <<'C'
#include "relay.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
            "${CC:-cc}" -std=c99 -Wall -Werror -pedantic-errors \
                -I"$repo/include" -I"$out" "$out"/*.c -o "$out/program"
            "$out/program"
        fi
    done
done

cat > "$work/private_bridge.zi" <<'ZI'
#scope_module
using Defs :: #import "defs";
ZI
cat > "$work/private_relay.zi" <<'ZI'
#import "private_bridge"
Bad :: (values: ImportedAlias) -> s64 { return values.count }
ZI
if "$ziran" check --root "$work" "$work/private_relay.zi" \
    >"$work/private_relay.out" 2>&1; then
    echo "private re-exported array alias was accepted" >&2
    exit 1
fi

cat > "$work/private_alias.zi" <<'ZI'
Defs :: #import "defs";
Bad :: (values: Defs.PrivateAlias) -> s64 { return values.count }
ZI
if "$ziran" check --root "$work" "$work/private_alias.zi" \
    >"$work/private.out" 2>&1; then
    echo "private imported array alias was accepted" >&2
    exit 1
fi

cat > "$work/other.zi" <<'ZI'
ImportedAlias :: [1]s32
ZI
cat > "$work/ambiguous_alias.zi" <<'ZI'
#import "defs"
#import "other"
Bad :: (values: ImportedAlias) -> s64 { return values.count }
ZI
if "$ziran" check --root "$work" "$work/ambiguous_alias.zi" \
    >"$work/ambiguous.out" 2>&1; then
    echo "ambiguous imported array alias was accepted" >&2
    exit 1
fi

cat > "$work/other_bridge.zi" <<'ZI'
using Other :: #import "other";
ZI
cat > "$work/ambiguous_relay.zi" <<'ZI'
#import "bridge"
#import "other_bridge"
Bad :: (values: ImportedAlias) -> s64 { return values.count }
ZI
if "$ziran" check --root "$work" "$work/ambiguous_relay.zi" \
    >"$work/ambiguous_relay.out" 2>&1; then
    echo "ambiguous re-exported array alias was accepted" >&2
    exit 1
fi

cat > "$work/shadow_alias.zi" <<'ZI'
#import "defs"
ImportedAlias :: struct { value: s32 }
#program_export
Answer :: () -> s32 {
    item: ImportedAlias
    item.value = 42
    return item.value
}
ZI
"$ziran" bundle --root "$work" --entry shadow_alias:Answer \
    -o "$work/shadow_alias.zx" "$work/shadow_alias.zi"
test "$("$ziran" run "$work/shadow_alias.zx")" = 42
