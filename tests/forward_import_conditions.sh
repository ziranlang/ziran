#!/bin/sh
set -eu

ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/library.zi" <<'ZI'
BASE :: 39;
Value :: BASE + 1;
#scope_file
Hidden :: 40;
#scope_export
Pair :: struct { value: s64; }
Read :: () -> s64 { return 40 }
ZI
cat > "$work/forward_import.zi" <<'ZI'
#if Lib.Value == 40 && Lib.Read() == 40 && size_of(Lib.Pair) == 8 {
Selected :: 2;
} else {
Selected :: MissingValue;
}
RUN :: #run Lib.Value;
Lib :: #import "library";
#program_export
Answer :: () -> s64 { return Selected + RUN }
ZI

"$ziran" ir --root "$work" -o "$work/ir" "$work/forward_import.zi"
if grep -aFq 'MissingValue' "$work/ir/forward_import.zir"; then
    echo 'unselected imported branch was saved in IR' >&2
    exit 1
fi
for input in "$work/forward_import.zi" "$work/ir/forward_import.zir"; do
    case "$input" in
        *.zi) root=$work; label=source ;;
        *.zir) root=$work/ir; label=saved ;;
    esac
    "$ziran" bundle --root "$root" --entry forward_import:Answer \
        -o "$work/$label.zx" "$input"
    test "$("$ziran" run "$work/$label.zx")" = 42
    for target in c cpp go; do
        out="$work/$label-$target"
        "$ziran" build "--target=$target" --root "$root" -o "$out" "$input"
        case "$target" in
            c)
                printf '#include "forward_import.h"\nint main(void) { return Answer() == 42 ? 0 : 1; }\n' > "$work/main.c"
                ${CC:-cc} -Iinclude -I"$out" "$out"/*.c \
                    "$work/main.c" -o "$out/app"
                "$out/app"
                ;;
            cpp)
                printf '#include "forward_import.hpp"\nint main() { return Answer() == 42 ? 0 : 1; }\n' > "$work/main.cpp"
                ${CXX:-c++} -Iinclude -I"$out" "$out"/*.cpp \
                    "$work/main.cpp" -o "$out/app"
                "$out/app"
                ;;
            go)
                cat > "$out/forward_import_test.go" <<'GO'
package ziran
import "testing"
func TestForwardImport(t *testing.T) {
    if ForwardImport_Answer() != 42 { t.Fatal("forward import") }
}
GO
                GO111MODULE=off go test "$out"/*.go
                ;;
        esac
    done
done
cmp "$work/source.zx" "$work/saved.zx"

cat > "$work/open_import.zi" <<'ZI'
#if Read() == 40 {
Selected :: 42;
} else {
Selected :: MissingValue;
}
#import "library";
#program_export
Answer :: () -> s64 { return Selected }
ZI
"$ziran" check --root "$work" "$work/open_import.zi"

cat > "$work/inactive_import.zi" <<'ZI'
#if false {
#import "library";
}
#if Read() == 40 {
Answer :: () -> s64 { return 42 }
}
ZI
if "$ziran" check --root "$work" "$work/inactive_import.zi" \
    2> "$work/inactive_import.err"; then
    echo 'import in inactive branch became visible' >&2
    exit 1
fi
grep -Fq '#if condition is not a compile-time constant' \
    "$work/inactive_import.err"

cat > "$work/private_import.zi" <<'ZI'
#if Lib.Hidden == 40 {
Answer :: () -> s64 { return 42 }
}
Lib :: #import "library";
ZI
if "$ziran" check --root "$work" "$work/private_import.zi" \
    2> "$work/private_import.err"; then
    echo 'file-private imported constant became visible' >&2
    exit 1
fi
grep -Fq '#if condition is not a compile-time constant' \
    "$work/private_import.err"
