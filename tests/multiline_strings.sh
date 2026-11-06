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
    for target in c go; do
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

# The line limit counts physical lines: a body of three 2000-character lines
# is far past one line's limit once lowered, yet every line is short.
awk 'BEGIN {
    print "Text :: #string END"
    for (n = 0; n < 3; n++) { for (i = 0; i < 2000; i++) printf "x"; print "" }
    print "END;"
    print "#program_export"
    print "Answer :: () -> s32 {"
    print "    if Text.count != 6003 { return 0 }"
    print "    return 42"
    print "}"
    print "Broken :: 1 +"
}' > "$work/long_body.zi"
if "$ziran" check --root "$work" "$work/long_body.zi" 2> "$work/long_body.err"; then
    echo 'invalid declaration after a long #string was accepted' >&2
    exit 1
fi
if grep -Fq 'exceeds' "$work/long_body.err"; then
    cat "$work/long_body.err" >&2
    exit 1
fi
# The three body lines and END keep their places: the broken line is line 11.
grep -Eq 'long_body\.zi:11:' "$work/long_body.err" || {
    cat "$work/long_body.err" >&2
    exit 1
}
awk 'BEGIN {
    print "Text :: #string END"
    for (n = 0; n < 3; n++) { for (i = 0; i < 2000; i++) printf "x"; print "" }
    print "END;"
    print "#program_export"
    print "Answer :: () -> s32 {"
    print "    if Text.count != 6003 { return 0 }"
    print "    return 42"
    print "}"
}' > "$work/long_ok.zi"
"$ziran" check --root "$work" "$work/long_ok.zi"

# A physical line over the limit is refused at its own line, inside a body too.
awk 'BEGIN {
    print "Text :: #string END"
    print "short"
    for (i = 0; i < 4100; i++) printf "x"
    print ""
    print "END;"
}' > "$work/long_line.zi"
if "$ziran" check --root "$work" "$work/long_line.zi" 2> "$work/long_line.err"; then
    echo 'over-long physical line inside #string was accepted' >&2
    exit 1
fi
grep -Eq 'long_line\.zi:3:[0-9]+: source line exceeds 4095 characters' \
    "$work/long_line.err" || {
    cat "$work/long_line.err" >&2
    exit 1
}

# A raw table longer than a statement buffer works as an ordinary constant in
# statements on every target, and keeps all of its bytes.
awk 'BEGIN {
    print "Table :: #string END"
    for (n = 0; n < 3; n++) { for (i = 0; i < 2000; i++) printf "%c", 97 + (i % 26); print "" }
    print "END;"
    print "Take :: (value: string) -> s32 { return cast(s32)value.count }"
    print "#program_export"
    print "Answer :: () -> s32 {"
    print "    if Table.count != 6003 || Take(Table) != 6003 { return 0 }"
    print "    if Table[0] != #char \"a\" || Table[1999] != #char \"x\" { return 0 }"
    print "    if Table[2000] != #char \"\\n\" || Table[6002] != #char \"\\n\" { return 0 }"
    print "    return 42"
    print "}"
}' > "$work/long_table.zi"
"$ziran" ir --root "$work" -o "$work/long_table_ir" "$work/long_table.zi"
"$ziran" bundle --root "$work" --entry long_table:Answer \
    -o "$work/long_table.zx" "$work/long_table.zi"
"$ziran" bundle --root "$work/long_table_ir" --entry long_table:Answer \
    -o "$work/long_table_ir.zx" "$work/long_table_ir/long_table.zir"
cmp "$work/long_table.zx" "$work/long_table_ir.zx"
test "$("$ziran" run "$work/long_table.zx")" = 42
for input in source ir; do
    if test "$input" = source; then
        filename="$work/long_table.zi"
    else
        filename="$work/long_table_ir/long_table.zir"
    fi
    "$ziran" build --target=c --root "$work" -o "$work/long-c-$input" "$filename"
    cat > "$work/long-c-$input/main.c" <<'C'
#include "long_table.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
    ${CC:-cc} -Iinclude -I"$work/long-c-$input" "$work/long-c-$input/long_table.c" \
        "$work/long-c-$input/main.c" -o "$work/long-c-$input/app"
    "$work/long-c-$input/app"
    "$ziran" build --target=go --pkg main --root "$work" \
        -o "$work/long-go-$input" "$filename"
    cat > "$work/long-go-$input/main.go" <<'GO'
package main
func main() { if LongTable_Answer() != 42 { panic("long table") } }
GO
    GO111MODULE=off go run "$work/long-go-$input/long_table.go" "$work/long-go-$input/main.go"
done

# A table far past 64 KB, as a constant and as a global variable, works on the
# portable, C and Go targets from source and from saved IR.
for kind in const global; do
    name="huge_$kind"
    camel="Huge$(printf %s "$kind" | awk '{print toupper(substr($0,1,1)) substr($0,2)}')"
    awk -v kind="$kind" 'BEGIN {
        if (kind == "const") print "Table :: #string END"
        else print "Table: string = #string END"
        for (n = 0; n < 50; n++) { for (i = 0; i < 2000; i++) printf "%c", 97 + (i % 26); print "" }
        print "END;"
        print "#program_export"
        print "Answer :: () -> s32 {"
        print "    if Table.count != 100050 { return 0 }"
        print "    if Table[0] != #char \"a\" || Table[1999] != #char \"x\" { return 0 }"
        print "    if Table[2000] != #char \"\\n\" || Table[100049] != #char \"\\n\" { return 0 }"
        print "    return 42"
        print "}"
    }' > "$work/$name.zi"
    "$ziran" ir --root "$work" -o "$work/${name}_ir" "$work/$name.zi"
    "$ziran" bundle --root "$work" --entry "$name:Answer" \
        -o "$work/$name.zx" "$work/$name.zi"
    "$ziran" bundle --root "$work/${name}_ir" --entry "$name:Answer" \
        -o "$work/${name}_ir.zx" "$work/${name}_ir/$name.zir"
    cmp "$work/$name.zx" "$work/${name}_ir.zx"
    test "$("$ziran" run "$work/$name.zx")" = 42
    for input in source ir; do
        if test "$input" = source; then
            filename="$work/$name.zi"
        else
            filename="$work/${name}_ir/$name.zir"
        fi
        out="$work/$name-c-$input"
        "$ziran" build --target=c --root "$work" -o "$out" "$filename"
        printf '#include "%s.h"\nint main(void) { return Answer() == 42 ? 0 : 1; }\n' \
            "$name" > "$out/main.c"
        ${CC:-cc} -Iinclude -I"$out" "$out/$name.c" "$out/main.c" -o "$out/app"
        "$out/app"
        out="$work/$name-go-$input"
        "$ziran" build --target=go --pkg main --root "$work" -o "$out" "$filename"
        printf 'package main\nfunc main() { if %s_Answer() != 42 { panic("huge table") } }\n' \
            "$camel" > "$out/main.go"
        GO111MODULE=off go run "$out/$name.go" "$out/main.go"
    done
done

# A template string over 2 KB that spans physical lines keeps the line numbers
# of every declaration after it.
awk 'BEGIN {
    print "// Four."
    printf "Four :: () -> s64 #asm(\"x86_64\", \""
    for (n = 0; n < 52; n++) { if (n) print ""; printf "        movq %%r10, %%r11 ; pad pad pad pad pad" }
    print "\", \"=r:out\", \"\", \"\", \"\") { return 0 }"
    print ""
    print "// Five."
    print "Five :: () -> s64 { return 5 }"
}' > "$work/api_lines.zi"
five=$(grep -n '^Five ::' "$work/api_lines.zi" | cut -d: -f1)
"$ziran" api --json --root "$work" "$work/api_lines.zi" | grep -q "\"line\":$five[,}]"
