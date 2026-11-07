#!/bin/sh
# A string constant longer than the 4095 characters C99 requires compilers to
# accept in one literal builds as strict C (-std=c99 -Wall -Wextra -pedantic
# -Werror) and prints the same bytes from C and from .zx, which runs through
# TinyCC. Both a constant used from another module and a string global are
# over 64 KiB, with quotes, backslashes, ?? trigraph text, tabs and UTF-8.
set -eu

ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

awk 'function body() {
    for (n = 0; n < 1300; n++) {
        printf "%04d \047single\047 \"double\" back\\slash ??= ??/ tab\t \303\251\342\202\254", n
        print ""
    }
}
BEGIN {
    print "Big :: #string END"; body(); print "END;"
    print "Stored: string = #string END"; body(); print "END;"
}' > "$work/text.zi"
cat > "$work/main.zi" <<'ZI'
#import "text"

#program_export
Main :: () {
    print("%", Big)
    print("%", Stored)
}
ZI
awk 'function body() {
    for (n = 0; n < 1300; n++) {
        printf "%04d \047single\047 \"double\" back\\slash ??= ??/ tab\t \303\251\342\202\254", n
        print ""
    }
}
BEGIN { body(); body() }' > "$work/expected"
# Each of the two constants is over 64 KiB.
test "$(wc -c < "$work/expected")" -gt 131072

"$ziran" bundle --root "$work" --entry main:Main -o "$work/main.zx" "$work/main.zi"
"$ziran" run "$work/main.zx" > "$work/zx.out"
cmp "$work/expected" "$work/zx.out"

"$ziran" build --target=c --root "$work" -o "$work/c" "$work/main.zi"
printf '#include "main.h"\nint main(void) { Main(); return 0; }\n' > "$work/host.c"
for compiler in "${CC:-cc}" clang; do
    command -v "$compiler" > /dev/null 2>&1 || continue
    "$compiler" -std=c99 -Wall -Wextra -pedantic -Werror -Iinclude -I"$work/c" \
        "$work/c/main.c" "$work/c/text.c" "$work/host.c" -o "$work/app"
    "$work/app" > "$work/c.out"
    cmp "$work/expected" "$work/c.out"
done
echo 'string constants over the C99 literal limit build strictly and match .zx: passed'
