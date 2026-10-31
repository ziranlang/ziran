#!/bin/sh
set -eu

ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$repo/work"
work=$(mktemp -d "$repo/work/procedure-result.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER

cat > "$work/model.zi" <<'ZI'
Result :: struct { value: s32 }
PointerResult :: struct { value: s64 }
ZI
cat > "$work/provider.zi" <<'ZI'
#import "model"
Model :: #import "model";
LocalResult :: struct { value: bool }
Factory :: #type () -> Result;
PointerFactory :: #type () -> *Model.PointerResult;
LocalFactory :: #type () -> LocalResult;
factory: Factory;
pointer_factory: PointerFactory;
local_factory: LocalFactory;
Ready :: () -> bool {
    return factory != null || pointer_factory != null || local_factory != null
}
ZI
cat > "$work/app.zi" <<'ZI'
#import "provider"
#program_export
Answer :: () -> s32 {
    if Ready() { return 1 }
    return 42
}
ZI

# The result records occur only in retained procedure signatures. No call or
# implementation can incidentally keep them alive for the entry linker.
"$ziran" ir --root "$work" -o "$work/full-ir" "$work/app.zi"
"$ziran" ir --root "$work" --entry app:Answer \
    -o "$work/entry-ir" "$work/app.zi"
for form in source saved entry-saved; do
    case "$form" in
        source) root=$work; input=$work/app.zi ;;
        saved) root=$work/full-ir; input=$root/app.zir ;;
        entry-saved) root=$work/entry-ir; input=$root/app.zir ;;
    esac
    "$ziran" bundle --root "$root" --entry app:Answer \
        -o "$work/$form.zx" "$input"
    test "$("$ziran" run "$work/$form.zx")" = 42
    for target in c go; do
        out=$work/$form-$target
        "$ziran" build --target="$target" --root "$root" --entry app:Answer \
            -o "$out" "$input"
        case "$target" in
            c)
                cat > "$out/main.c" <<'C'
#include "app.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
                "${CC:-cc}" -std=c11 -I"$repo/include" -I"$out" \
                    "$out"/*.c -o "$out/test"
                "$out/test"
                ;;
            go)
                cat > "$out/main_test.go" <<'GO'
package ziran
import "testing"
func TestResultReachability(t *testing.T) {
    if got := App_Answer(); got != 42 { t.Fatal(got) }
}
GO
                GO111MODULE=off go test "$out"/*.go
                ;;
        esac
    done
done
cmp "$work/source.zx" "$work/saved.zx"
cmp "$work/source.zx" "$work/entry-saved.zx"
echo 'procedure result reachability: source/saved IR, C, Go and VM passed'
