#!/bin/sh
set -eu

ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/deep.zi" <<'ZI'
#if SECRET == 1 {
LoadedChoice :: 1;
} else {
LoadedChoice :: MissingValue;
}
LATER :: 1;
Later :: struct { value: s64; }
#scope_file
SECRET :: 1;
ZI
cat > "$work/middle.zi" <<'ZI'
#load "deep.zi";
ZI
cat > "$work/forward_load.zi" <<'ZI'
#if LATER == 1 && size_of(Later) == 8 {
Selected :: 42;
} else {
Selected :: MissingValue;
}
#load "middle.zi";
#program_export
Answer :: () -> s64 { return Selected + LoadedChoice - 1 }
ZI

"$ziran" ir --root "$work" -o "$work/ir" "$work/forward_load.zi"
if grep -aFq 'MissingValue' "$work/ir/forward_load.zir"; then
    echo 'unselected loaded branch was saved in IR' >&2
    exit 1
fi
for input in "$work/forward_load.zi" "$work/ir/forward_load.zir"; do
    case "$input" in
        *.zi) root=$work; label=source ;;
        *.zir) root=$work/ir; label=saved ;;
    esac
    "$ziran" bundle --root "$root" --entry forward_load:Answer \
        -o "$work/$label.zx" "$input"
    test "$("$ziran" run "$work/$label.zx")" = 42
    for target in c cpp go; do
        out="$work/$label-$target"
        "$ziran" build "--target=$target" --root "$root" -o "$out" "$input"
        case "$target" in
            c)
                printf '#include "forward_load.h"\nint main(void) { return Answer() == 42 ? 0 : 1; }\n' > "$work/main.c"
                ${CC:-cc} -Iinclude -I"$out" "$out"/*.c \
                    "$work/main.c" -o "$out/app"
                "$out/app"
                ;;
            cpp)
                printf '#include "forward_load.hpp"\nint main() { return Answer() == 42 ? 0 : 1; }\n' > "$work/main.cpp"
                ${CXX:-c++} -Iinclude -I"$out" "$out"/*.cpp \
                    "$work/main.cpp" -o "$out/app"
                "$out/app"
                ;;
            go)
                cat > "$out/forward_load_test.go" <<'GO'
package ziran
import "testing"
func TestForwardLoad(t *testing.T) {
    if ForwardLoad_Answer() != 42 { t.Fatal("forward #load") }
}
GO
                GO111MODULE=off go test "$out"/*.go
                ;;
        esac
    done
done
cmp "$work/source.zx" "$work/saved.zx"

cat > "$work/private_load.zi" <<'ZI'
#if SECRET == 1 {
Answer :: () -> s64 { return 42 }
}
#load "middle.zi";
ZI
if "$ziran" check --root "$work" "$work/private_load.zi" \
    2> "$work/private_load.err"; then
    echo 'file-private loaded constant became visible' >&2
    exit 1
fi
grep -Fq '#if condition is not a compile-time constant' \
    "$work/private_load.err"

cat > "$work/inactive_load.zi" <<'ZI'
#if false {
#load "missing.zi";
}
#program_export
Answer :: () -> s64 { return 42 }
ZI
"$ziran" check --root "$work" "$work/inactive_load.zi"
