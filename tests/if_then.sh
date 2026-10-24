#!/bin/sh
set -eu

ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/then.zi" <<'ZI'
Pick :: () -> s32 {
    if false then return 0
    else if true then return 40
    else return 0
}
Semicolon :: () -> s32 {
    if false then return 0; else return 2;
}
Block :: () -> s32 {
    if false then { return 0 }
    if true then { return Pick() + Semicolon() }
    return 0
}
CloseElse :: () -> s32 {
    if false {
        return 0
    } else return 42
}
InlineBranches :: (take_first: bool) -> s32 {
    if take_first then { if false { return 0 } else { return 42 } } else if false then { return 0 } else { return 42 }
}
EmptyInline :: () -> s32 {
    if false {} else { return 42 }
    return 0
}
NextLineElse :: () -> s32 {
    if false then { return 0 }
    else { return 42 }
}
#program_export
Answer :: () -> s32 {
    word: string = "then"
    if word != "then" then return 0
    if Block() != 42 then return 0
    if CloseElse() != 42 then return 0
    if InlineBranches(true) != 42 || InlineBranches(false) != 42 then return 0
    if EmptyInline() != 42 then return 0
    if NextLineElse() != 42 then return 0
    return Pick() + Semicolon()
}
ZI

"$ziran" ir --root "$work" -o "$work/ir" "$work/then.zi"
for input in source saved; do
    if test "$input" = source; then
        module=$work/then.zi
        root=$work
    else
        module=$work/ir/then.zir
        root=$work/ir
    fi
    "$ziran" bundle --root "$root" --entry then:Answer \
        -o "$work/$input.zx" "$module"
    test "$("$ziran" run "$work/$input.zx")" = 42
    for target in c go; do
        output=$work/$target-$input
        "$ziran" build "--target=$target" --root "$root" \
            -o "$output" "$module"
        case "$target" in
            c)
                printf '#include "then.h"\nint main(void) { return Answer() == 42 ? 0 : 1; }\n' > "$work/main.c"
                "${CC:-cc}" -std=c11 -I"$repo/include" -I"$output" \
                    "$output/then.c" "$work/main.c" -o "$output/app"
                "$output/app"
                ;;
            go)
                cat > "$output/then_test.go" <<'GO'
package ziran
import "testing"
func TestThen(t *testing.T) {
    if Then_Answer() != 42 { t.Fatal("if then") }
}
GO
                GO111MODULE=off go test "$output"/*.go
                ;;
        esac
    done
done
cmp "$work/source.zx" "$work/saved.zx"

cat > "$work/missing_condition.zi" <<'ZI'
Answer :: () -> s32 {
    if then return 42
    return 0
}
ZI
if "$ziran" check --root "$work" "$work/missing_condition.zi" \
    2> "$work/missing_condition.err"; then
    echo 'accepted if then without a condition' >&2
    exit 1
fi
rg -q 'if then requires a condition' "$work/missing_condition.err"

cat > "$work/missing_if.zi" <<'ZI'
Answer :: () -> s32 {
    if
        return 42
    return 0
}
ZI
if "$ziran" check --root "$work" "$work/missing_if.zi" \
    2> "$work/missing_if.err"; then
    echo 'accepted if without a condition' >&2
    exit 1
fi
rg -q 'condition requires an expression' "$work/missing_if.err"
