#!/bin/sh
# A non-void procedure must return on every path. The shared checker
# enforces this for every target, and a `while true` loop that only
# leaves by returning counts as returning.
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

reject() {
    name=$1
    cat > "$work/$name.zi"
    if "$ziran" check --root "$work" "$work/$name.zi" 2> "$work/$name.err"; then
        echo "$name: missing return was accepted" >&2
        exit 1
    fi
    grep -Fq 'missing return: every path must return a value' "$work/$name.err"
}

reject scalar <<'ZI'
Sign :: (a: s32) -> s32 {
    if a > 0 { return 1; }
}
ZI

reject else_if <<'ZI'
Sign :: (a: s32) -> s32 {
    if a > 0 { return 1; }
    else if a < 0 { return -1; }
}
ZI

reject breaking_loop <<'ZI'
Find :: (a: s32) -> s32 {
    while true {
        if a > 3 { break; }
        return a;
    }
}
ZI

reject outer_break <<'ZI'
Find :: (a: s32) -> s32 {
    while outer := true {
        while true {
            if a > 3 { break outer; }
            return a;
        }
    }
}
ZI

reject record <<'ZI'
Point :: struct { x: s32; }
Make :: (a: s32) -> Point {
    if a > 0 { return .{x = a}; }
}
ZI

cat > "$work/flow.zi" <<'ZI'
Count :: (limit: s32) -> s32 {
    i: s32 = 0;
    while true {
        i += 1;
        while true {
            if i > 2 { break; }
            i += 1;
        }
        if i > limit { return i; }
    }
}

Named :: (limit: s32) -> s32 {
    i: s32 = 0;
    while true {
        i += 1;
        if i < limit { continue; }
        return i;
    }
}

Sign :: (a: s32) -> s32 {
    if a > 0 { return 1; }
    else if a < 0 { return -1; }
    else { return 0; }
}

counted: s32;
Tally :: (a: s32) {
    if a > 0 { return; }
    counted += 1;
    return
}

#program_export
Answer :: () -> s32 {
    Tally(1);
    Tally(-1);
    return Count(5) * 100 + Named(4) * 10 + Sign(-3) + counted;
}
ZI

"$ziran" check --root "$work" "$work/flow.zi"
"$ziran" ir --root "$work" -o "$work/ir" "$work/flow.zi"
for input in source saved; do
    if test "$input" = source; then
        module=$work/flow.zi
        root=$work
    else
        module=$work/ir/flow.zir
        root=$work/ir
    fi
    "$ziran" bundle --root "$root" --entry flow:Answer \
        -o "$work/$input.zib" "$module"
    test "$("$ziran" run "$work/$input.zib")" = 640
    output="$work/c-$input"
    "$ziran" build --target=c --root "$root" -o "$output" "$module"
    cat > "$output/main.c" <<'C'
#include "flow.h"
int main(void) { return Answer() == 640 ? 0 : 1; }
C
    "${CC:-cc}" -I"$repo/include" -I"$output" "$output"/*.c -o "$output/app"
    "$output/app"
    output="$work/go-$input"
    "$ziran" build --target=go --pkg main --root "$root" -o "$output" "$module"
    cat > "$output/main.go" <<'GO'
package main
func main() { if Flow_Answer() != 640 { panic("wrong result") } }
GO
    GO111MODULE=off go run "$output"/*.go
done
cmp "$work/source.zib" "$work/saved.zib"
