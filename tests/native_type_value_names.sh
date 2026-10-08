#!/bin/sh
# Types retain module ownership when other modules export same-named values.
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS LD_PRELOAD
if test -n "${ZIRAN_TYPE_NAMES_OUTPUT:-}"; then
    work=$ZIRAN_TYPE_NAMES_OUTPUT
    mkdir -p "$work"
    test -z "$(ls -A "$work")"
else
    work=$(mktemp -d)
    trap 'rm -rf "$work"' EXIT HUP INT TERM
fi
cat > "$work/shapes.zi" <<'ZI'
Counter :: struct { value: s64; }
Value :: struct { value: s64; }
Mode :: enum { Off; On; }
Amount :: struct { value: s64; }
procedures_Plain :: struct { value: s64; }
ZI
cat > "$work/values.zi" <<'ZI'
Value: s64 = 7;
Mode :: 11;
ZI
cat > "$work/procedures.zi" <<'ZI'
#program_export
Counter :: (value: s64) -> s64 { return value + 1 }
#program_export
Amount :: (value: s64) -> s64 { return value }
Plain :: (value: s64) -> s64 { return value }
ZI
cat > "$work/app.zi" <<'ZI'
Shapes :: #import "shapes";
Values :: #import "values";
Procedures :: #import "procedures";
counter: Shapes.Counter = Shapes.Counter.{.value=20};
stored: Shapes.Value = Shapes.Value.{.value=7};
amount: Shapes.Amount = Shapes.Amount.{.value=11};
plain: Shapes.procedures_Plain = Shapes.procedures_Plain.{.value=3};
#program_export
Answer :: () -> s64 {
    mode: Shapes.Mode = Shapes.Mode.On
    if mode != Shapes.Mode.On || counter.value != 20 || stored.value != Values.Value { return 0 }
    choice: Shapes.Amount = Shapes.Amount.{.value=Values.Mode}
    if choice.value != amount.value { return 0 }
    return Procedures.Counter(counter.value) + stored.value + Procedures.Amount(amount.value) + Procedures.Plain(plain.value)
}
ZI
"$ziran" ir --root "$work" -o "$work/ir" "$work/app.zi"
for input in source saved; do
    if test "$input" = source; then root=$work; file=$work/app.zi
    else root=$work/ir; file=$work/ir/app.zir; fi
    "$ziran" bundle --root "$root" --entry app:Answer -o "$work/$input.zib" "$file"
    test "$("$ziran" run "$work/$input.zib")" = 42
    for target in c cpp go; do
        out=$work/$input-$target
        "$ziran" build --target="$target" --root "$root" -o "$out" "$file"
        case "$target" in
            c)
                cat > "$out/driver.c" <<'C'
#include "app.h"
#include "procedures.h"
int main(void) { return Answer() == 42 && Counter(41) == 42 && Amount(11) == 11 ? 0 : 1; }
C
                "${CC:-cc}" -std=c11 -I"$repo/include" -I"$out" "$out"/*.c -o "$out/program"
                "$out/program"
                ;;
            cpp)
                cat > "$out/driver.cpp" <<'CPP'
#include "app.hpp"
#include "procedures.hpp"
int main() { return Answer() == 42 && Counter(41) == 42 && Amount(11) == 11 ? 0 : 1; }
CPP
                "${CXX:-c++}" -std=c++17 -I"$repo/include" -I"$out" "$out"/*.cpp -o "$out/program"
                "$out/program"
                ;;
            go)
                cat > "$out/driver_test.go" <<'GO'
package ziran
import "testing"
func TestTypeValueNames(t *testing.T) {
    if App_Answer() != 42 { t.Fatal("type/value ownership") }
}
GO
                GO111MODULE=off go test "$out"/*.go
                ;;
        esac
    done
done
cmp "$work/source.zib" "$work/saved.zib"
echo 'Native type/value names passed unpruned C/C++/Go source and saved IR; exported C ABI and portable bundle agree'
