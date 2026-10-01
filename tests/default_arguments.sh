#!/bin/sh
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/dependency.zi" <<'ZI'
#scope_export
Other :: () -> s32 { return 43 }
ZI
cat > "$work/lib.zi" <<'ZI'
Dependency :: #import "dependency";
#scope_file
Base :: () -> s32 { return 40 }
#scope_export
Offset :: (base: s32 = Base(), extra: s32) -> s32 {
    return base + extra
}
InferredFromPrivate :: (value := Base()) -> s32 { return value }
COUNT :: 42;
ChooseScoped :: (first: $T, second: T = Base()) -> T {
    return first + second
}
Box :: struct { value: s32 }
ChooseRecord :: (first: $T, second: T = T.{value = 40}) -> T {
    return second
}
ChooseInferredRecord :: (first: $T, second: T = .{value = 41}) -> T {
    return second
}
ChooseRecordScoped :: (first: $T, second: T = T.{value = Base()}) -> T {
    return second
}
PairBox :: struct {
    first: s32
    second: s32
}
ChoosePairScoped :: (first: $T,
                     second: T = T.{first = Base(), second = Base() + 2}) -> T {
    return second
}
ChooseImported :: (first: $T,
                   second: T = T.{value = Dependency.Other()}) -> T {
    return second
}
ProcedureName :: () -> string { return #procedure_name() }
DefaultName :: (name: string = #procedure_name()) -> string {
    return name
}
ChooseName :: (first: $T, name: string = #procedure_name()) -> string {
    return name
}
GenericBodyName :: (first: $T) -> string {
    return #procedure_name()
}
ZI
cat > "$work/app.zi" <<'ZI'
#import "lib"
Library :: #import "lib";
Embedded :: #import, string "Defaulted :: (value := 42) -> s64 { return value }";
Base :: () -> s32 { return 2 }
FROM_RUN :: #run Offset(extra = 2);
#assert FROM_RUN == 42
NAME_FROM_RUN :: #run Library.ProcedureName();
#assert NAME_FROM_RUN == "ProcedureName"
calls: s32;
Next :: () -> s32 {
    calls += 1
    return calls
}
Evaluate :: (first: s32 = Next(), second: s32) -> s32 {
    return first * 10 + second
}
Simple :: (first: s32 = 40, second: s32 = 2) -> s32 {
    return first + second
}
SliceDefault :: (values: []s32,
                 extra: s32 = 2) -> s32 {
    return values[0] + extra
}
DEFAULT: s32;
LocalDefault :: (value: s32 = DEFAULT) -> s32 { return value }
Choose :: (first: $T, second: T = 0) -> T { return first + second }
Zero :: () -> s32 { return 7 }
Inferred :: (value := 40, extra: s64 = 2) -> s64 {
    return value + extra
}
InferredBeforeRequired :: (value := 40, extra: s64) -> s64 {
    return value + extra
}
InferredCall :: (value := Zero()) -> s32 { return value }
InferredBool :: (value := true) -> bool { return value }
InferredString :: (value := "hello") -> string { return value }
InferredReal :: (value := 1.5) -> float64 { return value }
ImportedInferred :: (value := Library.Offset(extra = 2)) -> s32 {
    return value
}
OpenConstantDefault :: (value := COUNT) -> s64 { return value }
InferredForward :: (value := Later()) -> s32 { return value }
Later :: () -> s32 { return 42 }
NestedInferred :: (value := LaterInferred()) -> s64 { return value }
LaterInferred :: (value := 42) -> s64 { return value }
Operation :: #type () -> s32;
Shadowed :: (Evaluate: Operation) -> s32 { return Evaluate() }
#program_export
Answer :: () -> s32 {
    if Offset(extra = 2) != FROM_RUN { return 0 }
    if Library.Offset(extra = 2) != FROM_RUN { return 0 }
    if Embedded.Defaulted() != 42 { return 0 }
    if InferredFromPrivate() != 40 ||
       Library.InferredFromPrivate() != 40 { return 0 }
    if Simple() != 42 || Simple(second = 3) != 43 { return 0 }
    values: [1]s32 = .{40}
    if SliceDefault(values[:]) != 42 || SliceDefault(values[:], extra = 3) != 43 { return 0 }
    DEFAULT: s32 = 2
    if LocalDefault() != 0 { return 0 }
    value: s32 = Evaluate(second = Next())
    if value != 21 || calls != 2 { return 0 }
    typed: s32 = 42
    if Choose(typed) != 42 { return 0 }
    if ChooseScoped(typed) != 82 || Library.ChooseScoped(typed) != 82 {
        return 0
    }
    boxed: Box = Box.{value = 2}
    if ChooseRecord(boxed).value != 40 ||
       Library.ChooseRecord(boxed).value != 40 ||
       ChooseInferredRecord(boxed).value != 41 ||
       ChooseRecordScoped(boxed).value != 40 ||
       Library.ChooseRecordScoped(boxed).value != 40 ||
       Library.ChooseImported(boxed).value != 43 { return 0 }
    pair: PairBox = PairBox.{first = 0, second = 0}
    if ChoosePairScoped(pair).first != 40 ||
       Library.ChoosePairScoped(pair).second != 42 { return 0 }
    if ProcedureName() != "ProcedureName" ||
       Library.ProcedureName() != "ProcedureName" ||
       DefaultName() != "DefaultName" ||
       Library.DefaultName() != "DefaultName" ||
       ChooseName(typed) != "ChooseName" ||
       Library.ChooseName(typed) != "ChooseName" ||
       Library.GenericBodyName(typed) != "GenericBodyName" { return 0 }
    if Inferred() != 42 || Inferred(value = 41) != 43 { return 0 }
    if InferredBeforeRequired(extra = 2) != 42 { return 0 }
    if InferredCall() != 7 || !InferredBool() { return 0 }
    if InferredString() != "hello" { return 0 }
    if InferredReal() != 1.5 { return 0 }
    if ImportedInferred() != 42 { return 0 }
    if OpenConstantDefault() != 42 { return 0 }
    if InferredForward() != 42 { return 0 }
    if NestedInferred() != 42 { return 0 }
    if Shadowed(Zero) != 7 { return 0 }
    return 42
}
ZI

"$ziran" check --root "$work" "$work/app.zi"
"$ziran" ir --root "$work" -o "$work/ir" "$work/app.zi"
"$ziran" bundle --root "$work" --entry app:Answer \
    -o "$work/source.zib" "$work/app.zi"
"$ziran" bundle --root "$work/ir" --entry app:Answer \
    -o "$work/saved.zib" "$work/ir/app.zir"
cmp "$work/source.zib" "$work/saved.zib"
test "$("$ziran" run "$work/source.zib")" = 42
test "$("$ziran" run "$work/saved.zib")" = 42

for input in "$work/app.zi" "$work/ir/app.zir"; do
    case "$input" in
        *.zi) suffix=source; root=$work ;;
        *) suffix=saved; root=$work/ir ;;
    esac
    for target in c cpp go; do
        out="$work/$target-$suffix"
        if test "$target" = go; then
            "$ziran" build --target=go --pkg main --root "$root" \
                -o "$out" "$input"
            cat > "$out/main.go" <<'GO'
package main
func main() { if App_Answer() != 42 { panic("default arguments") } }
GO
            GO111MODULE=off go run "$out"/*.go
        elif test "$target" = c; then
            "$ziran" build --target=c --root "$root" -o "$out" "$input"
            cat > "$out/main.c" <<'C'
#include "app.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
            "${CC:-cc}" -std=c11 -I"$repo/include" -I"$out" \
                "$out"/*.c -o "$out/app"
            "$out/app"
        else
            "$ziran" build --target=cpp --root "$root" -o "$out" "$input"
            cat > "$out/main.cpp" <<'CPP'
#include "app.hpp"
int main() { return Answer() == 42 ? 0 : 1; }
CPP
            "${CXX:-c++}" -std=c++17 -I"$repo/include" -I"$out" \
                "$out"/*.cpp -o "$out/app"
            "$out/app"
        fi
    done
done

cat > "$work/missing.zi" <<'ZI'
Required :: (optional: s32 = 1, required: s32) -> s32 {
    return optional + required
}
Answer :: () -> s32 { return Required() }
ZI
if "$ziran" check --root "$work" "$work/missing.zi" \
    2> "$work/missing.err"; then
    echo 'omitted required argument was accepted' >&2
    exit 1
fi
grep -Fq 'argument count mismatch' "$work/missing.err"

cat > "$work/procedure_name_outside.zi" <<'ZI'
NAME :: #procedure_name();
ZI
if "$ziran" check --root "$work" "$work/procedure_name_outside.zi" \
    2> "$work/procedure_name_outside.err"; then
    echo 'accepted #procedure_name() outside a procedure' >&2
    exit 1
fi
grep -Fq '#procedure_name() requires a procedure scope' \
    "$work/procedure_name_outside.err"

cat > "$work/invalid_default.zi" <<'ZI'
Broken :: (value: s32 = Missing()) -> s32 { return value }
Answer :: () -> s32 { return Broken(42) }
ZI
if "$ziran" check --root "$work" "$work/invalid_default.zi" \
    2> "$work/invalid_default.err"; then
    echo 'unchecked unused default expression was accepted' >&2
    exit 1
fi
grep -Fq 'unresolved function: Missing' "$work/invalid_default.err"

cat > "$work/unknown_inferred_type.zi" <<'ZI'
Broken :: (value := null) -> s32 { return 0 }
ZI
if "$ziran" check --root "$work" "$work/unknown_inferred_type.zi" \
    2> "$work/unknown_inferred_type.err"; then
    echo 'accepted an inferred default without a concrete type' >&2
    exit 1
fi
grep -Fq 'cannot infer default parameter type: value' \
    "$work/unknown_inferred_type.err"

cat > "$work/missing_in_inferred_default.zi" <<'ZI'
Broken :: (value := Required()) -> s32 { return value }
Required :: (value: s32) -> s32 { return value }
ZI
if "$ziran" check --root "$work" "$work/missing_in_inferred_default.zi" \
    2> "$work/missing_in_inferred_default.err"; then
    echo 'inferred default call omitted a required argument' >&2
    exit 1
fi
grep -Fq 'argument count mismatch: Required' \
    "$work/missing_in_inferred_default.err"
