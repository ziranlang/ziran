#!/bin/sh
set -eu

ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/lib.zi" <<'ZI'
Point :: struct { x: s64; y: s64; }
shared: Point;
Start :: 2;
count: s64 = Start;
ZI
cat > "$work/app.zi" <<'ZI'
#import "lib"
Lib :: #import "lib"
using shared;
#program_export
Answer :: () -> s64 {
    x = 38
    Lib.count = Lib.count + 1
    count = count + 1
    return x + count
}
ZI

"$ziran" check --root "$work" "$work/app.zi"
"$ziran" ir --root "$work" -o "$work/ir" "$work/app.zi"
"$ziran" bundle --root "$work" --entry app:Answer \
    -o "$work/source.zib" "$work/app.zi"
"$ziran" bundle --root "$work/ir" --entry app:Answer \
    -o "$work/saved.zib" "$work/ir/app.zir"
cmp "$work/source.zib" "$work/saved.zib"
test "$("$ziran" run "$work/source.zib")" = 42
test "$("$ziran" run "$work/saved.zib")" = 42

for input in "$work/app.zi" "$work/ir/app.zir"; do
    case "$input" in
        *.zi) suffix=source; root=$work ;;
        *) suffix=saved; root=$work/ir ;;
    esac
    for target in c cpp go; do
        out="$work/$target-$suffix"
        "$ziran" build "--target=$target" --root "$root" -o "$out" "$input"
        if test "$target" = go; then
            cat > "$out/app_test.go" <<'GO'
package ziran
import "testing"
func TestImportedGlobals(t *testing.T) {
    if App_Answer() != 42 { t.Fatal("imported globals") }
}
GO
            GO111MODULE=off go test "$out"/*.go
        elif test "$target" = c; then
            cat > "$out/main.c" <<'C'
#include "app.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
            "${CC:-cc}" -std=c11 -Wall -Werror -I"$repo/include" \
                -I"$out" "$out"/*.c -o "$out/app"
            "$out/app"
        else
            cat > "$out/main.cpp" <<'CPP'
#include "app.hpp"
int main() { return Answer() == 42 ? 0 : 1; }
CPP
            "${CXX:-c++}" -std=c++17 -Wall -Werror -I"$repo/include" \
                -I"$out" "$out"/*.cpp -o "$out/app"
            "$out/app"
        fi
    done
done

cat > "$work/named_using.zi" <<'ZI'
Lib :: #import "lib"
using Lib.shared;
#program_export
Answer :: () -> s64 { y = 42; return y }
ZI
"$ziran" check --root "$work" "$work/named_using.zi"
"$ziran" ir --root "$work" -o "$work/named-ir" "$work/named_using.zi"
for input in "$work/named_using.zi" "$work/named-ir/named_using.zir"; do
    case "$input" in
        *.zi) root=$work; suffix=source ;;
        *) root=$work/named-ir; suffix=saved ;;
    esac
    "$ziran" bundle --root "$root" --entry named_using:Answer \
        -o "$work/named.zib" "$input"
    test "$("$ziran" run "$work/named.zib")" = 42
    for target in c cpp go; do
        out="$work/named-$target-$suffix"
        "$ziran" build "--target=$target" --root "$root" -o "$out" "$input"
        if test "$target" = go; then
            cat > "$out/named_using_test.go" <<'GO'
package ziran
import "testing"
func TestNamedUsing(t *testing.T) {
    if NamedUsing_Answer() != 42 { t.Fatal("named global using") }
}
GO
            GO111MODULE=off go test "$out"/*.go
        elif test "$target" = c; then
            cat > "$out/main.c" <<'C'
#include "named_using.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
            "${CC:-cc}" -std=c11 -Wall -Werror -I"$repo/include" \
                -I"$out" "$out"/*.c -o "$out/app"
            "$out/app"
        else
            cat > "$out/main.cpp" <<'CPP'
#include "named_using.hpp"
int main() { return Answer() == 42 ? 0 : 1; }
CPP
            "${CXX:-c++}" -std=c++17 -Wall -Werror -I"$repo/include" \
                -I"$out" "$out"/*.cpp -o "$out/app"
            "$out/app"
        fi
    done
done

cat > "$work/other.zi" <<'ZI'
count: s64;
ZI
cat > "$work/ambiguous.zi" <<'ZI'
#import "lib"
#import "other"
Bad :: () -> s64 { return count }
ZI
if "$ziran" check --root "$work" "$work/ambiguous.zi" \
    2> "$work/ambiguous.err"; then
    echo 'ambiguous imported global was accepted' >&2
    exit 1
fi
grep -Fq 'ambiguous global name: count' "$work/ambiguous.err"

cat > "$work/ambiguous_using.zi" <<'ZI'
#import "lib"
#import "other"
using count;
ZI
if "$ziran" check --root "$work" "$work/ambiguous_using.zi" \
    2> "$work/ambiguous_using.err"; then
    echo 'ambiguous imported using root was accepted' >&2
    exit 1
fi
grep -Fq 'ambiguous global name: count' "$work/ambiguous_using.err"

cat > "$work/type_shadow.zi" <<'ZI'
#import "lib"
__zi_open_0 :: #import "lib"
Point :: struct { z: s64; }
#program_export
Answer :: () -> s64 {
    using shared;
    x = 42
    local: Point
    local.z = 1
    return x + shared.x + local.z - 43
}
ZI
"$ziran" ir --root "$work" -o "$work/open-shadow-ir" \
    "$work/type_shadow.zi"
for input in source saved; do
    if test "$input" = source; then
        root=$work
        file=$work/type_shadow.zi
    else
        root=$work/open-shadow-ir
        file=$work/open-shadow-ir/type_shadow.zir
    fi
    "$ziran" bundle --root "$root" --entry type_shadow:Answer \
        -o "$work/open-shadow-$input.zib" "$file"
    test "$("$ziran" run "$work/open-shadow-$input.zib")" = 42
    for target in c cpp go; do
        out="$work/open-shadow-$target-$input"
        "$ziran" build "--target=$target" --root "$root" -o "$out" "$file"
        if test "$target" = go; then
            cat > "$out/main_test.go" <<'GO'
package ziran
import "testing"
func TestOpenTypeShadow(t *testing.T) {
    if TypeShadow_Answer() != 42 { t.Fatal("open imported type owner") }
}
GO
            GO111MODULE=off go test "$out"/*.go
        elif test "$target" = c; then
            cat > "$out/main.c" <<'C'
#include "type_shadow.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
            "${CC:-cc}" -std=c11 -Wall -Werror -I"$repo/include" \
                -I"$out" "$out"/*.c -o "$out/app"
            "$out/app"
        else
            cat > "$out/main.cpp" <<'CPP'
#include "type_shadow.hpp"
int main() { return Answer() == 42 ? 0 : 1; }
CPP
            "${CXX:-c++}" -std=c++17 -Wall -Werror -I"$repo/include" \
                -I"$out" "$out"/*.cpp -o "$out/app"
            "$out/app"
        fi
    done
done
cmp "$work/open-shadow-source.zib" "$work/open-shadow-saved.zib"

cat > "$work/using_import_type_shadow.zi" <<'ZI'
using Lib :: #import "lib"
Point :: struct { z: s64; }
using shared;
#program_export
Answer :: () -> s64 {
    x = 42
    local: Point
    local.z = 1
    return x + shared.x + local.z - 43
}
ZI
"$ziran" ir --root "$work" -o "$work/using-import-shadow-ir" \
    "$work/using_import_type_shadow.zi"
for input in source saved; do
    if test "$input" = source; then
        root=$work
        file=$work/using_import_type_shadow.zi
    else
        root=$work/using-import-shadow-ir
        file=$work/using-import-shadow-ir/using_import_type_shadow.zir
    fi
    "$ziran" bundle --root "$root" --entry using_import_type_shadow:Answer \
        -o "$work/using-import-shadow-$input.zib" "$file"
    test "$("$ziran" run "$work/using-import-shadow-$input.zib")" = 42
done
cmp "$work/using-import-shadow-source.zib" \
    "$work/using-import-shadow-saved.zib"

cat > "$work/named_type_shadow.zi" <<'ZI'
Lib :: #import "lib"
Point :: struct { z: s64; }
using Lib.shared;
#program_export
Answer :: () -> s64 {
    x = 42
    direct: s64 = Lib.shared.x
    local: Point
    local.z = 1
    return x + direct + local.z - 43
}
ZI
"$ziran" ir --root "$work" -o "$work/shadow-ir" \
    "$work/named_type_shadow.zi"
for input in source saved; do
    if test "$input" = source; then
        root=$work
        file=$work/named_type_shadow.zi
    else
        root=$work/shadow-ir
        file=$work/shadow-ir/named_type_shadow.zir
    fi
    "$ziran" bundle --root "$root" --entry named_type_shadow:Answer \
        -o "$work/shadow-$input.zib" "$file"
    test "$("$ziran" run "$work/shadow-$input.zib")" = 42
    for target in c cpp go; do
        out="$work/shadow-$target-$input"
        "$ziran" build "--target=$target" --root "$root" -o "$out" "$file"
        if test "$target" = go; then
            cat > "$out/main_test.go" <<'GO'
package ziran
import "testing"
func TestNamedTypeShadow(t *testing.T) {
    if NamedTypeShadow_Answer() != 42 { t.Fatal("imported type owner") }
}
GO
            GO111MODULE=off go test "$out"/*.go
        elif test "$target" = c; then
            cat > "$out/main.c" <<'C'
#include "named_type_shadow.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
            "${CC:-cc}" -std=c11 -Wall -Werror -I"$repo/include" \
                -I"$out" "$out"/*.c -o "$out/app"
            "$out/app"
        else
            cat > "$out/main.cpp" <<'CPP'
#include "named_type_shadow.hpp"
int main() { return Answer() == 42 ? 0 : 1; }
CPP
            "${CXX:-c++}" -std=c++17 -Wall -Werror -I"$repo/include" \
                -I"$out" "$out"/*.cpp -o "$out/app"
            "$out/app"
        fi
    done
done
cmp "$work/shadow-source.zib" "$work/shadow-saved.zib"

cat > "$work/private.zi" <<'ZI'
Lib :: #import "private_lib"
Bad :: () -> s64 { return Lib.hidden }
ZI
cat > "$work/private_lib.zi" <<'ZI'
#scope_file
hidden: s64;
#scope_module
ZI
if "$ziran" check --root "$work" "$work/private.zi" \
    2> "$work/private.err"; then
    echo 'file-private imported global was accepted' >&2
    exit 1
fi
grep -Fq 'unresolved name: Lib.hidden' "$work/private.err"

# An imported global array keeps its element type's module, so its elements
# can be read, written and passed by pointer through the module alias.
cat > "$work/store.zi" <<'ZI'
Item :: struct { value: s32; }
items: [4]Item;
Read :: (item: *Item) -> s32 { return item.value }
ZI
cat > "$work/items.zi" <<'ZI'
Store :: #import "store";
#program_export
Answer :: () -> s32 {
    Store.items[1].value = 5
    item := *Store.items[1]
    item.value += 1
    return Store.Read(item) + Store.items[1].value
}
ZI
"$ziran" bundle --root "$work" --entry items:Answer -o "$work/items.zib" "$work/items.zi"
test "$("$ziran" run "$work/items.zib")" = 12
"$ziran" build --target=c --entry items:Answer --root "$work" -o "$work/items-c" "$work/items.zi"
cat > "$work/items-c/main.c" <<'C'
#include "items.h"
int main(void) { return Answer() == 12 ? 0 : 1; }
C
"${CC:-cc}" -std=c99 -pedantic-errors -I"$repo/include" -I"$work/items-c" \
    "$work/items-c"/*.c -o "$work/items-c/app"
"$work/items-c/app"
