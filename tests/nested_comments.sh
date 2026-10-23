#!/bin/sh
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/part.zi" <<'ZI'
/* The outer comment continues after the inner close.
   /* nested comment */
   invalid source here
*/
Number :: 40 // a line comment after a declaration
ZI
cat > "$work/nested_comments.zi" <<'ZI'
#load "part.zi"; // the line comment ends here
// A line comment containing /* cannot start a block comment.
/* outer /* inner */ still outer */
#program_export
Answer :: () -> s32 {
    marker: string = "/* string // contents */"
    value := Number /* first /* second */ first again */ + 2 // and here
    if marker.count == 24 { return cast(s32)value }
    return 0
}
ZI

"$ziran" ir --root "$work" -o "$work/ir" "$work/nested_comments.zi"
for input in source saved; do
    if test "$input" = source; then
        module=$work/nested_comments.zi
        root=$work
    else
        module=$work/ir/nested_comments.zir
        root=$work/ir
    fi
    "$ziran" bundle --root "$root" --entry nested_comments:Answer \
        -o "$work/$input.zx" "$module"
    test "$("$ziran" run "$work/$input.zx")" = 42
    for target in c cpp go; do
        output=$work/$target-$input
        "$ziran" build "--target=$target" --root "$root" \
            -o "$output" "$module"
        case "$target" in
            c)
                printf '#include "nested_comments.h"\nint main(void) { return Answer() == 42 ? 0 : 1; }\n' > "$work/main.c"
                "${CC:-cc}" -std=c11 -I"$repo/include" -I"$output" \
                    "$output"/*.c "$work/main.c" -o "$output/app"
                "$output/app"
                ;;
            cpp)
                printf '#include "nested_comments.hpp"\nint main() { return Answer() == 42 ? 0 : 1; }\n' > "$work/main.cpp"
                "${CXX:-c++}" -std=c++17 -I"$repo/include" -I"$output" \
                    "$output"/*.cpp "$work/main.cpp" -o "$output/app"
                "$output/app"
                ;;
            go)
                cat > "$output/nested_comments_test.go" <<'GO'
package ziran
import "testing"
func TestNestedComments(t *testing.T) {
    if NestedComments_Answer() != 42 { t.Fatal("nested comments") }
}
GO
                GO111MODULE=off go test "$output"/*.go
                ;;
        esac
    done
done
cmp "$work/source.zx" "$work/saved.zx"

cat > "$work/unclosed.zi" <<'ZI'
/* outer /* inner */ still outer
ZI
if "$ziran" check --root "$work" "$work/unclosed.zi" \
    2> "$work/unclosed.err"; then
    echo 'accepted an unterminated nested comment' >&2
    exit 1
fi
rg -q 'unterminated block comment' "$work/unclosed.err"

cat > "$work/unclosed_part.zi" <<'ZI'
/* comment opened in a loaded file
ZI
cat > "$work/unclosed_load.zi" <<'ZI'
#load "unclosed_part.zi";
Answer :: () -> s32 { return 42 }
ZI
if "$ziran" check --root "$work" "$work/unclosed_load.zi" \
    2> "$work/unclosed_load.err"; then
    echo 'accepted an unterminated comment in a loaded file' >&2
    exit 1
fi
rg -q 'unterminated block comment' "$work/unclosed_load.err"
