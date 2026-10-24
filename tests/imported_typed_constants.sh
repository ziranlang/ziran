#!/bin/sh
set -eu

ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/library.zi" <<'ZI'
BASE :: 1.25;
Ratio :: BASE * 2.0;
Label :: "ready";
#scope_file
Hidden :: "private";
ZI
cat > "$work/app.zi" <<'ZI'
#if Lib.Ratio == 2.5 && Lib.Label == "ready" {
Selected :: 42;
} else {
Selected :: MissingValue;
}
RatioCopy :: #run Lib.Ratio;
LabelCopy :: #run Lib.Label;
Lib :: #import "library";
#assert RatioCopy == 2.5
#assert LabelCopy == "ready"
#program_export
Answer :: () -> s64 {
    if RatioCopy != 2.5 || LabelCopy != "ready" { return 0 }
    return Selected
}
ZI

"$ziran" ir --root "$work" -o "$work/ir" "$work/app.zi"
if grep -aFq 'MissingValue' "$work/ir/app.zir"; then
    echo 'unselected imported branch was saved in IR' >&2
    exit 1
fi
for input in "$work/app.zi" "$work/ir/app.zir"; do
    case "$input" in
        *.zi) root=$work; label=source ;;
        *.zir) root=$work/ir; label=saved ;;
    esac
    "$ziran" bundle --root "$root" --entry app:Answer \
        -o "$work/$label.zx" "$input"
    test "$("$ziran" run "$work/$label.zx")" = 42
    for target in c go; do
        out="$work/$label-$target"
        "$ziran" build "--target=$target" --root "$root" -o "$out" "$input"
        case "$target" in
            c)
                printf '#include "app.h"\nint main(void) { return Answer() == 42 ? 0 : 1; }\n' > "$work/main.c"
                ${CC:-cc} -Iinclude -I"$out" "$out"/*.c \
                    "$work/main.c" -o "$out/app"
                "$out/app"
                ;;
            go)
                cat > "$out/app_test.go" <<'GO'
package ziran
import "testing"
func TestImportedTypedConstants(t *testing.T) {
    if App_Answer() != 42 { t.Fatal("imported constants") }
}
GO
                GO111MODULE=off go test "$out"/*.go
                ;;
        esac
    done
done
cmp "$work/source.zx" "$work/saved.zx"

cat > "$work/private.zi" <<'ZI'
#if Lib.Hidden == "private" {
Answer :: () -> s64 { return 42 }
}
Lib :: #import "library";
ZI
if "$ziran" check --root "$work" "$work/private.zi" \
    2> "$work/private.err"; then
    echo 'private imported string became visible' >&2
    exit 1
fi
grep -Fq '#if condition is not a compile-time constant' "$work/private.err"
