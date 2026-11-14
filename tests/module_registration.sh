#!/bin/sh
set -eu

# A module registers itself from a global initializer. Importing the module is
# what includes it: its initializer runs before the entry, and a program that
# does not import it carries neither the module nor what only it reaches.
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$repo/build/tests"
work=$(mktemp -d "$repo/build/tests/module-registration-XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/host.zi" <<'ZI'
Draw :: #type (value: s32) -> s32;
draw: Draw;

Register :: (procedure: Draw) -> bool {
    draw = procedure
    return true
}

// -1 when no module registered a procedure.
Call :: (value: s32) -> s32 {
    if draw == null { return -1 }
    return draw(value)
}
ZI
cat > "$work/heavy.zi" <<'ZI'
Scale :: () -> s32 { return 2 }
ZI
cat > "$work/feature.zi" <<'ZI'
Host :: #import "host";
Heavy :: #import "heavy";

Double :: (value: s32) -> s32 { return value * Heavy.Scale() }

registered: bool = Host.Register(Double);
ZI
# Nothing in feature is named: the import alone registers it.
cat > "$work/with_feature.zi" <<'ZI'
Host :: #import "host";
#import "feature"

#program_export
Answer :: () -> s32 { return Host.Call(21) }

main :: () -> s32 {
    print("%\n", Answer())
    return 0
}
ZI
cat > "$work/without_feature.zi" <<'ZI'
Host :: #import "host";

#program_export
Answer :: () -> s32 { return Host.Call(21) }

main :: () -> s32 {
    print("%\n", Answer())
    return 0
}
ZI

for program in with_feature without_feature; do
    case "$program" in
        with_feature) expected=42 ;;
        without_feature) expected=-1 ;;
    esac
    "$ziran" ir --root "$work" -o "$work/$program-ir" "$work/$program.zi"
    for input in source saved; do
        case "$input" in
            source) root=$work; file=$work/$program.zi ;;
            saved) root=$work/$program-ir; file=$work/$program-ir/$program.zir ;;
        esac
        out="$work/$program-$input-c"
        "$ziran" build --target=c --root "$root" --entry "$program:Answer" -o "$out" "$file"
        cat > "$out/main.c" <<C
#include "$program.h"
int main(void) { return Answer() == $expected ? 0 : 1; }
C
        "${CC:-cc}" -std=c11 -I"$repo/include" -I"$out" "$out"/*.c -o "$out/program"
        "$out/program"
        # The feature and its dependency exist exactly when the import does.
        case "$program" in
            with_feature) test -f "$out/feature.c"; test -f "$out/heavy.c" ;;
            without_feature) test ! -e "$out/feature.c"; test ! -e "$out/heavy.c" ;;
        esac
        test "$("$ziran" run --root "$root" --entry "$program:Answer" "$file")" = "$expected"
        test "$("$ziran" run --vm --root "$root" --entry "$program:Answer" "$file")" = "$expected"
    done
    # Go runs the same initializer from its package initialization.
    go_out="$work/$program-go"
    "$ziran" build --target=go --pkg main --exe --root "$work" --entry "$program:main" \
        -o "$go_out" "$work/$program.zi"
    (cd "$go_out" && GO111MODULE=off go build -o program .)
    test "$("$go_out/program")" = "$expected"
    case "$program" in
        with_feature) test -f "$go_out/feature.go" ;;
        without_feature) test ! -e "$go_out/feature.go" ;;
    esac
done
echo 'An imported module registers itself from its initializer; unimported modules are absent'
