#!/bin/sh
set -eu

ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/multiline.zi" <<'ZI'
// #string COMMENT must stay a comment.
/* #string BLOCK_COMMENT must stay a comment. */
/*
#string COMMENT_LINE
*/
Greeting :: #string END
  Jai "quotes" \ slash
END_EXTRA
\n stays two bytes
	é
END;
Empty :: #string EMPTY
EMPTY;

Same :: (left: string, right: string) -> bool {
    return left == right
}

Echo :: (value: string) -> string {
    return value
}

RawReturn :: () -> string {
    return #string STOP
first
second
STOP;
}

#program_export
Answer :: () -> s32 {
    if Large.count != 2301 || Large[2299] != #char "x" ||
       Large[2300] != #char "\n" { return 0 }
    if !Same(Greeting, "  Jai \"quotes\" \\ slash\nEND_EXTRA\n\\n stays two bytes\n\t\u00e9\n") { return 0 }
    if !Same(Empty, "") { return 0 }
    if !Same(RawReturn(), "first\nsecond\n") { return 0 }
    value: string = Echo(#string LAST
call argument
LAST);
    if !Same(value, "call argument\n") { return 0 }
    if !Same(Echo(#string FIRST
same
FIRST), Echo(#string SECOND
same
SECOND)) { return 0 }
    if !Same("#string NOT_A_TOKEN", "#string NOT_A_TOKEN") { return 0 }
    return 42
}
ZI

awk 'BEGIN {
    print "Large :: #string LONG"
    for (i = 0; i < 2300; i++) printf "x"
    print "\nLONG"
}' >> "$work/multiline.zi"

"$ziran" check --root "$work" "$work/multiline.zi"
"$ziran" ir --root "$work" -o "$work/ir" "$work/multiline.zi"
"$ziran" bundle --root "$work" --entry multiline:Answer \
    -o "$work/source.zx" "$work/multiline.zi"
"$ziran" bundle --root "$work" --entry multiline:Answer \
    -o "$work/ir.zx" "$work/ir/multiline.zir"
cmp "$work/source.zx" "$work/ir.zx"
test "$("$ziran" run "$work/source.zx")" = 42

for input in source ir; do
    if test "$input" = source; then
        filename="$work/multiline.zi"
    else
        filename="$work/ir/multiline.zir"
    fi
    for target in c cpp go; do
        output="$work/$target-$input"
        if test "$target" = go; then
            "$ziran" build --target=go --pkg main --root "$work" \
                -o "$output" "$filename"
            cat > "$output/main.go" <<'GO'
package main
func main() { if Multiline_Answer() != 42 { panic("wrong string result") } }
GO
            GO111MODULE=off go run "$output/multiline.go" "$output/main.go"
        elif test "$target" = c; then
            "$ziran" build --target=c --root "$work" \
                -o "$output" "$filename"
            cat > "$output/main.c" <<'C'
#include "multiline.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
            ${CC:-cc} -Iinclude -I"$output" "$output/multiline.c" \
                "$output/main.c" -o "$output/app"
            "$output/app"
        else
            "$ziran" build --target=cpp --root "$work" \
                -o "$output" "$filename"
            cat > "$output/main.cpp" <<'CPP'
#include "multiline.hpp"
int main() { return Answer() == 42 ? 0 : 1; }
CPP
            ${CXX:-c++} -Iinclude -I"$output" "$output/multiline.cpp" \
                "$output/main.cpp" -o "$output/app"
            "$output/app"
        fi
    done
done

cp "$work/multiline.zi" "$work/formatted.zi"
"$ziran" fmt "$work/formatted.zi"
"$ziran" fmt --check "$work/formatted.zi"
"$ziran" bundle --root "$work" --entry formatted:Answer \
    -o "$work/formatted.zx" "$work/formatted.zi"
test "$("$ziran" run "$work/formatted.zx")" = 42

cat > "$work/bad_open.zi" <<'ZI'
Text :: #string END extra
body
END;
ZI
if "$ziran" check --root "$work" "$work/bad_open.zi" 2> "$work/bad_open.err"; then
    echo 'malformed #string opening line was accepted' >&2
    exit 1
fi
grep -Fq '#string delimiter must end its line' "$work/bad_open.err"

cat > "$work/unclosed.zi" <<'ZI'
Text :: #string END
body
ZI
if "$ziran" check --root "$work" "$work/unclosed.zi" 2> "$work/unclosed.err"; then
    echo 'unterminated #string was accepted' >&2
    exit 1
fi
grep -Fq 'unterminated #string delimiter: END' "$work/unclosed.err"

cat > "$work/after_literal.zi" <<'ZI'
Text :: #string END
body
END;
NotADeclaration
ZI
if "$ziran" check --root "$work" "$work/after_literal.zi" \
    2> "$work/after_literal.err"; then
    echo 'invalid declaration after #string was accepted' >&2
    exit 1
fi
if ! grep -Eq 'after_literal\.zi:4:[0-9]+:.*invalid top-level declaration' \
    "$work/after_literal.err"; then
    cat "$work/after_literal.err" >&2
    exit 1
fi
