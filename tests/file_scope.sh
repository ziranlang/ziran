#!/bin/sh
set -eu

ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/part.zi" <<'ZI'
#scope_file
Hidden :: () -> s32 { return 40 }
HiddenValue :: 1;
hidden_global: s32 = 1;
HiddenType :: struct {
    number: s32
}
#scope_module
Shared :: () -> s32 { return Hidden() + HiddenValue + 1 }
#scope_export
Visible :: () -> s32 { return 1 }
ZI
cat > "$work/main.zi" <<'ZI'
#load "part.zi";
#program_export
Answer :: () -> s32 { return Shared() + Visible() }
ZI

"$ziran" check --root "$work" "$work/main.zi"
"$ziran" ir --root "$work" -o "$work/ir" "$work/main.zi"
"$ziran" bundle --root "$work" --entry main:Answer \
    -o "$work/source.zx" "$work/main.zi"
"$ziran" bundle --root "$work/ir" --entry main:Answer \
    -o "$work/saved.zx" "$work/ir/main.zir"
cmp "$work/source.zx" "$work/saved.zx"
test "$("$ziran" run "$work/saved.zx")" = 43

"$ziran" build --target=c --root "$work" -o "$work/c" "$work/main.zi"
cat > "$work/c/entry.c" <<'C'
#include "main.h"
int main(void) { return Answer() == 43 ? 0 : 1; }
C
"${CC:-cc}" -Iinclude -I"$work/c" "$work/c"/*.c -o "$work/c/app"
"$work/c/app"
"$ziran" build --target=go --pkg main --root "$work" \
    -o "$work/go" "$work/main.zi"
cat > "$work/go/entry.go" <<'GO'
package main
func main() { if Main_Answer() != 43 { panic("file scope changed") } }
GO
GO111MODULE=off go run "$work/go"/*.go

for kind in procedure constant global type; do
    case "$kind" in
        procedure) body='return Hidden()' ;;
        constant) body='return HiddenValue' ;;
        global) body='return hidden_global' ;;
        type) body='value: HiddenType; return value.number' ;;
    esac
    cat > "$work/main.zi" <<ZI
#load "part.zi";
Answer :: () -> s32 { $body }
ZI
    if "$ziran" check --root "$work" "$work/main.zi" \
        2> "$work/$kind.err"; then
        echo "#scope_file $kind leaked into another loaded file" >&2
        exit 1
    fi
done

cat > "$work/main.zi" <<'ZI'
#load "part.zi";
#if HiddenValue {
Answer :: () -> s32 { return 42 }
}
ZI
if "$ziran" check --root "$work" "$work/main.zi" \
    2> "$work/condition.err"; then
    echo '#scope_file constant leaked into a compile-time condition' >&2
    exit 1
fi
grep -Fq '#if condition is not a compile-time constant' \
    "$work/condition.err"

cat > "$work/main.zi" <<'ZI'
#load "part.zi";
Answer :: () -> s32 { return size_of(HiddenType) }
ZI
if "$ziran" check --root "$work" "$work/main.zi" \
    2> "$work/size.err"; then
    echo '#scope_file type leaked into size_of in another loaded file' >&2
    exit 1
fi
grep -Fq 'size_of requires a known sized type' "$work/size.err"

cat > "$work/main.zi" <<'ZI'
#load "part.zi";
leak: s32 = hidden_global;
Answer :: () -> s32 { return leak }
ZI
if "$ziran" check --root "$work" "$work/main.zi" \
    2> "$work/global_init.err"; then
    echo '#scope_file global leaked into another file initializer' >&2
    exit 1
fi
grep -Fq 'file-private declaration is not visible' \
    "$work/global_init.err"

cat > "$work/main.zi" <<'ZI'
#scope_file
RootHidden :: () -> s32 { return 1 }
#scope_export
#load "user.zi";
Answer :: () -> s32 { return 42 }
ZI
cat > "$work/user.zi" <<'ZI'
UseRoot :: () -> s32 { return RootHidden() }
ZI
if "$ziran" check --root "$work" "$work/main.zi" \
    2> "$work/root.err"; then
    echo '#scope_file root declaration leaked into a loaded file' >&2
    exit 1
fi

cat > "$work/foreign.zi" <<'ZI'
#program_export
Value :: () -> s32 { return 1 }
ZI
cat > "$work/part.zi" <<'ZI'
#scope_file
Foreign :: #import "foreign";
#scope_module
UseForeign :: () -> s32 { return Foreign.Value() }
ZI
cat > "$work/main.zi" <<'ZI'
#load "part.zi";
Answer :: () -> s32 { return UseForeign() }
ZI
"$ziran" check --root "$work" "$work/main.zi"
cat > "$work/main.zi" <<'ZI'
#load "part.zi";
Answer :: () -> s32 { return Foreign.Value() }
ZI
if "$ziran" check --root "$work" "$work/main.zi" \
    2> "$work/import.err"; then
    echo '#scope_file import alias leaked into another loaded file' >&2
    exit 1
fi

cat > "$work/part.zi" <<'ZI'
#scope_file
libc :: #system_library "libc";
Read :: (value: s32) -> s32 #foreign libc "sample_read";
ZI
cat > "$work/main.zi" <<'ZI'
#load "part.zi";
Answer :: () -> s32 { return 42 }
ZI
"$ziran" check --root "$work" "$work/main.zi"
cat > "$work/main.zi" <<'ZI'
#load "part.zi";
Another :: (value: s32) -> s32 #foreign libc "sample_read";
ZI
if "$ziran" check --root "$work" "$work/main.zi" \
    2> "$work/library.err"; then
    echo '#scope_file system library leaked into another loaded file' >&2
    exit 1
fi
grep -Fq '#foreign library is not declared' "$work/library.err"

cat > "$work/part.zi" <<'ZI'
Callback :: #type () -> s32;
#scope_file
Same :: () -> s32 { return 1 }
SameValue :: 1;
Other :: SameValue + 1;
same_global: s32;
SameType :: struct {
    number: s32
}
WrapperOne :: struct {
    item: SameType
}
#scope_module
One :: () -> s32 {
    same_global = 1
    callback: Callback = Same
    record: SameType = .{number = 1}
    return callback() + Other + same_global + record.number
}
Shadow :: () -> s32 {
    same_global: s32 = 3
    return same_global
}
ZI
cat > "$work/part2.zi" <<'ZI'
#scope_file
Same :: () -> s32 { return 2 }
SameValue :: 2;
same_global: s32;
SameType :: struct {
    number: s64
}
WrapperTwo :: struct {
    item: SameType
}
#scope_module
Two :: () -> s32 {
    same_global = 2
    return Same() + SameValue + same_global + cast(s32) size_of(SameType)
}
ZI
cat > "$work/main.zi" <<'ZI'
#load "part.zi";
#load "part2.zi";
#program_export
Answer :: () -> s32 { return One() + Two() + Shadow() }
ZI
"$ziran" check --root "$work" "$work/main.zi"
"$ziran" ir --root "$work" -o "$work/duplicate-ir" "$work/main.zi"
"$ziran" bundle --root "$work" --entry main:Answer \
    -o "$work/duplicate-source.zx" "$work/main.zi"
"$ziran" bundle --root "$work/duplicate-ir" --entry main:Answer \
    -o "$work/duplicate-saved.zx" "$work/duplicate-ir/main.zir"
cmp "$work/duplicate-source.zx" "$work/duplicate-saved.zx"
test "$("$ziran" run "$work/duplicate-saved.zx")" = 22

"$ziran" build --target=c --root "$work" -o "$work/duplicate-c" \
    "$work/main.zi"
cat > "$work/duplicate-c/entry.c" <<'C'
#include "main.h"
int main(void) { return Answer() == 22 ? 0 : 1; }
C
if rg -q '^#define SameValue[[:space:]]' "$work/duplicate-c/main.c"; then
    echo 'file-private constants share a C macro name' >&2
    exit 1
fi
"${CC:-cc}" -Iinclude -I"$work/duplicate-c" \
    "$work/duplicate-c"/*.c -o "$work/duplicate-c/app"
"$work/duplicate-c/app"
"$ziran" build --target=go --pkg main --root "$work" \
    -o "$work/duplicate-go" "$work/main.zi"
cat > "$work/duplicate-go/entry.go" <<'GO'
package main
func main() { if Main_Answer() != 22 { panic("file-private declarations collided") } }
GO
GO111MODULE=off go run "$work/duplicate-go"/*.go

cat > "$work/part.zi" <<'ZI'
#scope_file
SameValue :: 1;
first: s32 = SameValue;
#scope_module
One :: () -> s32 { return first }
ZI
cat > "$work/part2.zi" <<'ZI'
#scope_file
SameValue :: 2;
second: s32 = SameValue;
#scope_module
Two :: () -> s32 { return second }
ZI
cat > "$work/main.zi" <<'ZI'
#load "part.zi";
#load "part2.zi";
#program_export
Answer :: () -> s32 { return One() + Two() }
ZI
"$ziran" ir --root "$work" -o "$work/private-global-ir" "$work/main.zi"
for input in source saved; do
    if test "$input" = source; then
        file=$work/main.zi
        root=$work
    else
        file=$work/private-global-ir/main.zir
        root=$work/private-global-ir
    fi
    for target in c go; do
        output=$work/private-global-$input-$target
        if test "$target" = go; then
            "$ziran" build --target=go --pkg main --root "$root" \
                -o "$output" "$file"
            cat > "$output/entry.go" <<'GO'
package main
func main() { if Main_Answer() != 3 { panic("private constant global initialization") } }
GO
            GO111MODULE=off go run "$output"/*.go
        elif test "$target" = c; then
            "$ziran" build --target=c --root "$root" -o "$output" "$file"
            cat > "$output/entry.c" <<'C'
#include "main.h"
int main(void) { return Answer() == 3 ? 0 : 1; }
C
            "${CC:-cc}" -Iinclude -I"$output" \
                "$output"/*.c -o "$output/app"
            "$output/app"
        fi
    done
done

cat > "$work/part.zi" <<'ZI'
#scope_file
Choice :: enum u8 { Low :: 1; High :: 2; }
#scope_module
ReadOne :: () -> s32 { return cast(s32) size_of(Choice) }
ZI
cat > "$work/part2.zi" <<'ZI'
#scope_file
Choice :: enum u64 { Low :: 4; High :: 8; }
#scope_module
ReadTwo :: () -> s32 { return cast(s32) size_of(Choice) }
ZI
cat > "$work/main.zi" <<'ZI'
#load "part.zi";
#load "part2.zi";
#program_export
Answer :: () -> s32 { return ReadOne() + ReadTwo() }
ZI
"$ziran" ir --root "$work" -o "$work/private-enum-ir" "$work/main.zi"
"$ziran" bundle --root "$work" --entry main:Answer \
    -o "$work/private-enum-source.zx" "$work/main.zi"
"$ziran" bundle --root "$work/private-enum-ir" --entry main:Answer \
    -o "$work/private-enum-saved.zx" "$work/private-enum-ir/main.zir"
cmp "$work/private-enum-source.zx" "$work/private-enum-saved.zx"
test "$("$ziran" run "$work/private-enum-saved.zx")" = 9
"$ziran" build --target=c --root "$work" -o "$work/private-enum-c" \
    "$work/main.zi"
cat > "$work/private-enum-c/entry.c" <<'C'
#include "main.h"
int main(void) { return Answer() == 9 ? 0 : 1; }
C
"${CC:-cc}" -Iinclude -I"$work/private-enum-c" \
    "$work/private-enum-c"/*.c -o "$work/private-enum-c/app"
"$work/private-enum-c/app"
"$ziran" build --target=go --pkg main --root "$work" \
    -o "$work/private-enum-go" "$work/main.zi"
cat > "$work/private-enum-go/entry.go" <<'GO'
package main
func main() { if Main_Answer() != 9 { panic("private enums collided") } }
GO
GO111MODULE=off go run "$work/private-enum-go"/*.go
