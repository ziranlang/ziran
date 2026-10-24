#!/bin/sh
set -eu
. "$(dirname -- "$0")/lib/expect_rejection.sh"

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
source=$repo/tests/spec/language_contract.zi
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

"$ziran" check --root "$repo/tests/spec" "$source"
"$ziran" ir --root "$repo/tests/spec" -o "$work/ir" "$source"

cat > "$work/old_export.zi" <<'EOF'
Answer :: () -> s32 #export { return 42 }
EOF
expect_rejection 'signature #export was accepted' "$work/old_export.err" \
    "$ziran" check --root "$work" "$work/old_export.zi"
grep -Fq 'unknown function modifier: #export' "$work/old_export.err"

cat > "$work/misplaced_export.zi" <<'EOF'
#program_export
Number :: struct { value: s32; }
EOF
expect_rejection 'misplaced #program_export was accepted' "$work/misplaced_export.err" \
    "$ziran" check --root "$work" "$work/misplaced_export.zi"
grep -Fq '#program_export must precede a function declaration' \
    "$work/misplaced_export.err"

cat > "$work/dangling_export.zi" <<'EOF'
#program_export
EOF
expect_rejection 'dangling #program_export was accepted' "$work/dangling_export.err" \
    "$ziran" check --root "$work" "$work/dangling_export.zi"
grep -Fq '#program_export must precede a function declaration' \
    "$work/dangling_export.err"

cat > "$work/old_global.zi" <<'EOF'
count :: s32 #global
EOF
expect_rejection '#global declaration was accepted' "$work/old_global.err" \
    "$ziran" check --root "$work" "$work/old_global.zi"
grep -Fq '#global is not Jai syntax' "$work/old_global.err"

cat > "$work/static_global.zi" <<'EOF'
static count: s32 = 1;
EOF
expect_rejection 'C-style static global was accepted' "$work/static_global.err" \
    "$ziran" check --root "$work" "$work/static_global.zi"
grep -Fq 'invalid file-scope variable name' "$work/static_global.err"

cat > "$work/no_global_semicolon.zi" <<'EOF'
count: s32 = 1
EOF
expect_rejection 'file-scope variable without a semicolon was accepted' "$work/no_global_semicolon.err" \
    "$ziran" check --root "$work" "$work/no_global_semicolon.zi"
grep -Fq "file-scope variable declaration needs ';'" \
    "$work/no_global_semicolon.err"

cat > "$work/old_extern.zi" <<'EOF'
Host :: () -> s32 #extern
EOF
expect_rejection '#extern procedure was accepted' "$work/old_extern.err" \
    "$ziran" check --root "$work" "$work/old_extern.zi"
grep -Fq 'unknown function modifier: #extern' "$work/old_extern.err"

cat > "$work/unknown_foreign_library.zi" <<'EOF'
Host :: () -> s32 #foreign missing;
EOF
expect_rejection 'undeclared #foreign library was accepted' "$work/unknown_foreign_library.err" \
    "$ziran" check --root "$work" "$work/unknown_foreign_library.zi"
grep -Fq '#foreign library is not declared: missing' \
    "$work/unknown_foreign_library.err"

cat > "$work/foreign_semicolon.zi" <<'EOF'
libc :: #system_library "libc";
Abs :: (value: s32) -> s32 #foreign libc
EOF
expect_rejection '#foreign procedure without a semicolon was accepted' "$work/foreign_semicolon.err" \
    "$ziran" check --root "$work" "$work/foreign_semicolon.zi"
grep -Fq "#foreign declaration must end with ';'" \
    "$work/foreign_semicolon.err"

cat > "$work/old_extern_type.zi" <<'EOF'
Record :: struct #extern { value: s32; }
EOF
expect_rejection '#extern type was accepted' "$work/old_extern_type.err" \
    "$ziran" check --root "$work" "$work/old_extern_type.zi"
grep -Fq '#extern is not Jai syntax for a type declaration' \
    "$work/old_extern_type.err"

cat > "$work/old_private.zi" <<'EOF'
Hidden :: () -> s32 #private { return 42 }
EOF
expect_rejection '#private function was accepted' "$work/old_private.err" \
    "$ziran" check --root "$work" "$work/old_private.zi"
grep -Fq '#private is not Jai syntax' "$work/old_private.err"

cat > "$work/old_ternary.zi" <<'EOF'
Answer :: () -> s32 {
    return true ? 1 : 2
}
EOF
expect_rejection 'C-style conditional expression was accepted' "$work/old_ternary.err" \
    "$ziran" check --root "$work" "$work/old_ternary.zi"
grep -Fq 'C-style conditional is not valid Jai syntax' \
    "$work/old_ternary.err"

cat > "$work/bad_pointer_field.zi" <<'EOF'
Record :: struct {
    value: s32*
}
EOF
cat > "$work/bad_pointer_global.zi" <<'EOF'
value: s32*;
EOF
cat > "$work/bad_pointer_param.zi" <<'EOF'
Answer :: (value: s32*) -> s32 { return 0 }
EOF
cat > "$work/bad_pointer_return.zi" <<'EOF'
Answer :: () -> s32* { return null }
EOF
cat > "$work/bad_pointer_local.zi" <<'EOF'
Answer :: () -> s32 {
    value: s32* = null
    return 0
}
EOF
cat > "$work/bad_pointer_foreign.zi" <<'EOF'
libc :: #system_library "libc";
Foreign :: (value: s32*) -> s32 #foreign libc;
EOF
cat > "$work/bad_pointer_alias.zi" <<'EOF'
Node :: struct {
    value: s32
}
Alias :: Node*;
EOF
for bad in field global param return local foreign alias; do
    if "$ziran" check --root "$work" "$work/bad_pointer_$bad.zi" \
        2> "$work/bad_pointer_$bad.err"; then
        echo "C-style pointer type was accepted in $bad" >&2
        exit 1
    fi
    grep -Fq 'C-style pointer type is not Jai syntax' \
        "$work/bad_pointer_$bad.err"
done

cat > "$work/jai_pointer_type.zi" <<'EOF'
Record :: struct {
    values: [2*3]s32
    pointer: *s32
}
Answer :: () -> s32 {
    record: Record
    return record.values[5]
}
EOF
"$ziran" check --root "$work" "$work/jai_pointer_type.zi"

cat > "$work/pointer_global.zi" <<'EOF'
pointer: *s32;
EOF
"$ziran" ir --root "$work" -o "$work/pointer-ir" \
    "$work/pointer_global.zi"
python3 - "$work/pointer-ir/pointer_global.zir" \
    "$work/pointer-ir/forged.zir" <<'PY'
from pathlib import Path
import sys
saved = Path(sys.argv[1]).read_bytes()
assert saved.count(b"*s32") == 1
Path(sys.argv[2]).write_bytes(saved.replace(b"*s32", b"s32*"))
PY
expect_rejection 'C-style pointer type was accepted from saved IR' "$work/forged_pointer.err" \
    "$ziran" check --root "$work" "$work/pointer-ir/forged.zir"
grep -Fq 'C-style pointer type is not Jai syntax' \
    "$work/forged_pointer.err"

cat > "$work/directive_text.zi" <<'EOF'
Message :: "#private and #global are ordinary text";
#program_export
Answer :: () -> s32 {
    return 42
}
EOF
"$ziran" check --root "$work" "$work/directive_text.zi"

cat > "$work/scoped.zi" <<'EOF'
#scope_file
HIDDEN :: 39;
HiddenRecord :: struct {
    values: [HIDDEN]s32
}
Hidden :: () -> s32 {
    item: HiddenRecord
    item.values[0] = HIDDEN
    return item.values[0]
}
#scope_module
increment: s32 = 2;
#scope_export
PublicRecord :: struct {
    value: s32
}
PUBLIC :: 1;
Public :: () -> s32 {
    item: PublicRecord
    item.value = Hidden() + increment + PUBLIC
    return item.value
}
EOF
cat > "$work/scoped_client.zi" <<'EOF'
#import "scoped"
#program_export
Answer :: () -> s32 {
    item: PublicRecord
    item.value = Public()
    return item.value + PUBLIC - 1
}
EOF
cat > "$work/scoped_bad_client.zi" <<'EOF'
#import "scoped"
Answer :: () -> s32 {
    return Hidden()
}
EOF
cat > "$work/scoped_bad_type.zi" <<'EOF'
#import "scoped"
Answer :: () -> s32 {
    item: HiddenRecord
    return 0
}
EOF
cat > "$work/scoped_bad_constant.zi" <<'EOF'
#import "scoped"
Answer :: () -> s32 {
    return HIDDEN
}
EOF
"$ziran" check --root "$work" "$work/scoped_client.zi"
expect_rejection '#scope_file procedure leaked across modules' "$work/scoped_bad_client.err" \
    "$ziran" check --root "$work" "$work/scoped_bad_client.zi"
for bad in scoped_bad_type scoped_bad_constant; do
    if "$ziran" check --root "$work" "$work/$bad.zi" \
        2> "$work/$bad.err"; then
        echo "private declaration leaked across modules: $bad" >&2
        exit 1
    fi
done
"$ziran" ir --root "$work" -o "$work/scope-ir" \
    "$work/scoped.zi" "$work/scoped_client.zi"
for bad in scoped_bad_type scoped_bad_constant; do
    if "$ziran" check --root "$work" "$work/scope-ir/scoped.zir" \
        "$work/$bad.zi" 2> "$work/$bad-saved.err"; then
        echo "private saved-IR declaration leaked: $bad" >&2
        exit 1
    fi
done
for extension in zi zir; do
    if test "$extension" = zi; then
        inputs="$work/scoped.zi $work/scoped_client.zi"
    else
        inputs="$work/scope-ir/scoped.zir $work/scope-ir/scoped_client.zir"
    fi
    out=$work/scope-c-$extension
    # The fixture paths contain no spaces; split this two-file list deliberately.
    "$ziran" build --target=c --root "$work" -o "$out" $inputs
    if grep -Fq 'Hidden(' "$out/scoped.h" ||
       grep -Fq 'increment' "$out/scoped.h"; then
        echo 'private scope leaked into generated header' >&2
        exit 1
    fi
    ${CC:-cc} -E -x c -I"$repo/include" -I"$out" \
        "$out/scoped.h" > "$out/public.i"
    ${CC:-cc} -dM -E -x c -I"$repo/include" -I"$out" \
        "$out/scoped.h" > "$out/public_macros.i"
    if grep -Fq 'HiddenRecord' "$out/public.i" ||
       grep -Fq '#define HIDDEN ' "$out/public_macros.i"; then
        echo 'private declaration leaked into generated header' >&2
        exit 1
    fi
    cat > "$out/main.c" <<'EOF'
#include "scoped_client.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
EOF
    ${CC:-cc} -std=c11 -I"$repo/include" -I"$out" \
        "$out/scoped.c" "$out/scoped_client.c" "$out/main.c" -o "$out/app"
    "$out/app"
done

cat > "$work/foreign_c.zi" <<'EOF'
libc :: #system_library "libc";
Abs :: (value: s32) -> s32 #foreign libc "abs";
#program_export
Answer :: () -> s32 {
    return Abs(-42)
}
EOF
"$ziran" ir --root "$work" -o "$work/foreign-ir" "$work/foreign_c.zi"
for input in "$work/foreign_c.zi" "$work/foreign-ir/foreign_c.zir"; do
    kind=$(basename "$input")
    c_out=$work/foreign-c-$kind
    "$ziran" build --target=c --root "$work" -o "$c_out" "$input"
    cat > "$c_out/main.c" <<'EOF'
#include "foreign_c.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
EOF
    ${CC:-cc} -std=c11 -I"$repo/include" -I"$c_out" \
        "$c_out/foreign_c.c" "$c_out/main.c" -o "$c_out/app"
    "$c_out/app"
done

for input in "$source" "$work/ir/language_contract.zir"; do
    kind=$(basename "$input")
    c_out=$work/c-$kind
    go_out=$work/go-$kind
    "$ziran" build --target=c --root "$repo/tests/spec" \
        -o "$c_out" "$input"
    "$ziran" build --target=go --pkg main \
        --root "$repo/tests/spec" -o "$go_out" "$input"

    cat > "$c_out/main.c" <<'EOF'
#include "language_contract.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
EOF
    ${CC:-cc} -std=c11 -I"$repo/include" -I"$c_out" \
        "$c_out/language_contract.c" "$c_out/main.c" -o "$c_out/app"
    "$c_out/app"

    cat > "$go_out/main.go" <<'EOF'
package main
func main() { if LanguageContract_Answer() != 42 { panic("language contract") } }
EOF
    GO111MODULE=off go run "$go_out/language_contract.go" "$go_out/main.go"
done
