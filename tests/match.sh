#!/bin/sh
set -eu
. "$(dirname -- "$0")/lib/expect_rejection.sh"

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/modes.zi" <<'ZI'
Mode :: enum {
    Off :: -1;
    On;
    Later :: On + 4;
}
ZI
cat > "$work/app.zi" <<'ZI'
#import "modes"
calls: s32;
Next :: () -> Mode {
    using Mode;
    calls += 1
    return cast(Mode)On
}
Value :: (mode: Mode) -> s32 {
    if #complete mode == {
        case Mode.Off;
            return 30
        case Mode.On;
            if true { return 41 }
            return 0
        case Mode.Later;
            return 42
    }
}
Nested :: (mode: Mode) -> s32 {
    using Mode;
    selected: Mode = cast(Mode) On
    if #complete mode == {
        case Mode.Off;
            if #complete selected == {
                case Mode.Off;
                    return 0
                case Mode.On;
                    return 42
                case Mode.Later;
                    return 0
            }
        case Mode.On;
            return 0
        case Mode.Later;
            return 0
    }
}
Once :: () -> s32 {
    if #complete Next() == {
        case Mode.Off;
            return 0
        case Mode.On;
            return calls
        case Mode.Later;
            return 0
    }
}
#program_export
Answer :: () -> s32 {
    using Mode;
    if ~(cast(s32) 0) != cast(s32) -1 { return 0 }
    return Value(cast(Mode)Off) + Value(cast(Mode)On) + Value(cast(Mode)Later) + Nested(cast(Mode)Off) + Once()
}
#program_export
Invalid :: () -> s32 {
    return Value(cast(Mode)99)
}
ZI

"$ziran" check --root "$work" "$work/modes.zi" "$work/app.zi"
"$ziran" ir --root "$work" -o "$work/ir" "$work/modes.zi" "$work/app.zi"
if grep -aFq 'Off :: -1' "$work/ir/modes.zir" ||
   grep -aFq 'Later :: On + 4' "$work/ir/modes.zir"; then
    echo 'Jai enum member declaration was saved without lowering' >&2
    exit 1
fi
for input in source saved; do
    if test "$input" = source; then
        modules="$work/modes.zi $work/app.zi"
        root=$work
    else
        modules="$work/ir/modes.zir $work/ir/app.zir"
        root=$work/ir
    fi
    # Paths from mktemp contain no whitespace on supported test systems.
    # shellcheck disable=SC2086
    "$ziran" bundle --root "$root" --entry app:Answer \
        -o "$work/$input.zx" $modules
    test "$("$ziran" run "$work/$input.zx")" = 156
    # shellcheck disable=SC2086
    "$ziran" bundle --root "$root" --entry app:Invalid \
        -o "$work/invalid-$input.zx" $modules
    expect_rejection 'invalid enum value unexpectedly matched' "$work/invalid.err" \
        "$ziran" run "$work/invalid-$input.zx"
    grep -Fq 'unreachable code executed' "$work/invalid.err"

    for target in c cpp go; do
        output="$work/$target-$input"
        # shellcheck disable=SC2086
        if test "$target" = go; then
            # shellcheck disable=SC2086
            "$ziran" build --target=go --pkg main --root "$root" \
                -o "$output" $modules
            cat > "$output/main.go" <<'GO'
package main
func main() { if App_Answer() != 156 { panic("wrong match result") } }
GO
            GO111MODULE=off go run "$output"/*.go
        elif test "$target" = c; then
            # shellcheck disable=SC2086
            "$ziran" build --target=c --root "$root" \
                -o "$output" $modules
            cat > "$output/main.c" <<'C'
#include "app.h"
int main(void) { return Answer() == 156 ? 0 : 1; }
C
            "${CC:-cc}" -I"$repo/include" -I"$output" \
                "$output"/*.c -o "$output/app"
            "$output/app"
        else
            # shellcheck disable=SC2086
            "$ziran" build --target=cpp --root "$root" \
                -o "$output" $modules
            cat > "$output/main.cpp" <<'CPP'
#include "app.hpp"
int main() { return Answer() == 156 ? 0 : 1; }
CPP
            "${CXX:-c++}" -I"$repo/include" -I"$output" \
                "$output"/*.cpp -o "$output/app"
            "$output/app"
        fi
    done
done
cmp "$work/source.zx" "$work/saved.zx"

cat > "$work/through.zi" <<'ZI'
Mode :: enum { First; Second; Third; Fourth; }
Value :: (mode: Mode) -> s32 {
    total: s32 = 0
    if #complete mode == {
        case .First;
            total += 1
            #through;
        case .Second;
            total += 2
            #through;
        case .Third;
            total += 4
        case .Fourth;
            total += 8
    }
    return total
}
DeferredCase :: () -> s32 {
    using Mode;
    total: s32 = 0
    if cast(Mode)First == {
        case .First;
            defer { total += 1 }
            #through;
        case .Second;
            total += 2
    }
    if cast(Mode)First == {
        case .First;
            defer { total += 4 }
        case .Second;
            total += 100
    }
    return total
}
ReturnThrough :: () -> s32 {
    using Mode;
    if #complete cast(Mode)First == {
        case .First;
            marker: s32 = 0
            #through;
        case .Second;
            return 42
        case .Third;
            return 0
        case .Fourth;
            return 0
    }
}
#program_export
Answer :: () -> s32 {
    using Mode;
    if Value(cast(Mode)First) != 7 ||
       Value(cast(Mode)Second) != 6 ||
       Value(cast(Mode)Third) != 4 ||
       Value(cast(Mode)Fourth) != 8 || DeferredCase() != 7 ||
       ReturnThrough() != 42 { return 0 }
    return 42
}
ZI
"$ziran" check --root "$work" "$work/through.zi"
"$ziran" ir --root "$work" -o "$work/through-ir" "$work/through.zi"
for input in source saved; do
    if test "$input" = source; then
        file=$work/through.zi
        root=$work
    else
        file=$work/through-ir/through.zir
        root=$work/through-ir
    fi
    "$ziran" bundle --root "$root" --entry through:Answer \
        -o "$work/through-$input.zx" "$file"
    test "$("$ziran" run "$work/through-$input.zx")" = 42
    for target in c cpp go; do
        output=$work/through-$input-$target
        if test "$target" = go; then
            "$ziran" build --target=go --pkg main --root "$root" \
                -o "$output" "$file"
            cat > "$output/main.go" <<'GO'
package main
func main() { if Through_Answer() != 42 { panic("Jai #through") } }
GO
            GO111MODULE=off go run "$output"/*.go
        elif test "$target" = c; then
            "$ziran" build --target=c --root "$root" -o "$output" "$file"
            cat > "$output/main.c" <<'C'
#include "through.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
            "${CC:-cc}" -I"$repo/include" -I"$output" \
                "$output"/*.c -o "$output/app"
            "$output/app"
        else
            "$ziran" build --target=cpp --root "$root" -o "$output" "$file"
            cat > "$output/main.cpp" <<'CPP'
#include "through.hpp"
int main() { return Answer() == 42 ? 0 : 1; }
CPP
            "${CXX:-c++}" -I"$repo/include" -I"$output" \
                "$output"/*.cpp -o "$output/app"
            "$output/app"
        fi
    done
done
cmp "$work/through-source.zx" "$work/through-saved.zx"

cat > "$work/invalid_through.zi" <<'ZI'
Mode :: enum { First; Second; }
Value :: (mode: Mode) -> s32 {
    if mode == {
        case .First;
            #through;
            return 1
        case .Second;
            return 2
    }
    return 0
}
ZI
expect_rejection 'accepted #through before the end of a case' "$work/invalid_through.err" \
    "$ziran" check --root "$work" "$work/invalid_through.zi"
grep -Fq '#through must end a case before another case' \
    "$work/invalid_through.err"

cat > "$work/final_through.zi" <<'ZI'
Mode :: enum { First; Second; }
Value :: (mode: Mode) -> s32 {
    if mode == {
        case .First;
            return 1
        case .Second;
            #through;
    }
    return 0
}
ZI
expect_rejection 'accepted #through in the final case' "$work/final_through.err" \
    "$ziran" check --root "$work" "$work/final_through.zi"
grep -Fq '#through must end a case before another case' \
    "$work/final_through.err"

cat > "$work/unknown_directive.zi" <<'ZI'
Answer :: () -> s32 {
    #unknown_directive;
    return 42
}
ZI
expect_rejection 'silently ignored an unknown function-body directive' "$work/unknown_directive.err" \
    "$ziran" check --root "$work" "$work/unknown_directive.zi"
grep -Fq 'unknown function-body directive' "$work/unknown_directive.err"

cat > "$work/scalar_case.zi" <<'ZI'
calls: s32;
Next :: () -> s32 {
    calls += 1
    return 2
}
IntegerCase :: () -> s32 {
    calls = 0
    total: s32 = 0
    if Next() == {
        case 1;
            total = 100
        case 2;
            total += 2
            #through;
        case 3;
            total += 3
        case;
            total = 1000
    }
    if calls != 1 { return 0 }
    return total
}
DefaultCase :: (number: s32) -> s32 {
    if number == {
        case 1;
            return 1
        case;
            return 42
    }
}
BoolCase :: (flag: bool) -> s32 {
    if flag == {
        case true;
            return 42
        case;
            return 0
    }
}
StringCase :: (value: string) -> s32 {
    if value == {
        case "yes";
            return 42
        case;
            return 0
    }
}
FloatCase :: (value: float64) -> s32 {
    if value == {
        case 1.5;
            return 42
        case;
            return 0
    }
}
ExpressionCase :: () -> s32 {
    if false == {
        case false || true;
            return 0
        case;
            return 42
    }
}
NestedCase :: () -> s32 {
    total: s32 = 0
    if true == {
        case true;
            if 1 == {
                case 1;
                    defer { total += 1 }
                    total += 1
                    #through;
                case 2;
                    total += 2
            }
        case;
            total = 100
    }
    return total
}
#program_export
Answer :: () -> s32 {
    if IntegerCase() != 5 || DefaultCase(99) != 42 ||
       BoolCase(true) != 42 || StringCase("yes") != 42 ||
       FloatCase(1.5) != 42 || ExpressionCase() != 42 ||
       NestedCase() != 4 { return 0 }
    return 42
}
ZI
"$ziran" check --root "$work" "$work/scalar_case.zi"
"$ziran" ir --root "$work" -o "$work/scalar-ir" "$work/scalar_case.zi"
for input in source saved; do
    if test "$input" = source; then
        file=$work/scalar_case.zi
        root=$work
    else
        file=$work/scalar-ir/scalar_case.zir
        root=$work/scalar-ir
    fi
    "$ziran" bundle --root "$root" --entry scalar_case:Answer \
        -o "$work/scalar-$input.zx" "$file"
    test "$("$ziran" run "$work/scalar-$input.zx")" = 42
    for target in c cpp go; do
        output=$work/scalar-$input-$target
        if test "$target" = go; then
            "$ziran" build --target=go --pkg main --root "$root" \
                -o "$output" "$file"
            cat > "$output/main.go" <<'GO'
package main
func main() { if ScalarCase_Answer() != 42 { panic("Jai scalar case") } }
GO
            GO111MODULE=off go run "$output"/*.go
        elif test "$target" = c; then
            "$ziran" build --target=c --root "$root" -o "$output" "$file"
            cat > "$output/main.c" <<'C'
#include "scalar_case.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
            "${CC:-cc}" -I"$repo/include" -I"$output" \
                "$output"/*.c -o "$output/app"
            "$output/app"
        else
            "$ziran" build --target=cpp --root "$root" -o "$output" "$file"
            cat > "$output/main.cpp" <<'CPP'
#include "scalar_case.hpp"
int main() { return Answer() == 42 ? 0 : 1; }
CPP
            "${CXX:-c++}" -I"$repo/include" -I"$output" \
                "$output"/*.cpp -o "$output/app"
            "$output/app"
        fi
    done
done
cmp "$work/scalar-source.zx" "$work/scalar-saved.zx"

cat > "$work/scalar_default_order.zi" <<'ZI'
Value :: (number: s32) -> s32 {
    if number == {
        case;
            return 0
        case 1;
            return 1
    }
}
ZI
expect_rejection 'accepted a default case before another case' "$work/scalar_default_order.err" \
    "$ziran" check --root "$work" "$work/scalar_default_order.zi"
grep -Fq 'default case must be last' "$work/scalar_default_order.err"

cat > "$work/scalar_case_type.zi" <<'ZI'
Value :: (number: s32) -> s32 {
    if number == {
        case "wrong";
            return 0
        case;
            return 42
    }
}
ZI
expect_rejection 'accepted a case with an incompatible type' "$work/scalar_case_type.err" \
    "$ziran" check --root "$work" "$work/scalar_case_type.zi"
grep -Fq 'operand types differ' "$work/scalar_case_type.err"

cat > "$work/missing.zi" <<'ZI'
#import "modes"
Value :: (mode: Mode) -> s32 {
    if #complete mode == {
        case Mode.Off;
            return 1
        case Mode.On;
            return 2
    }
}
ZI
expect_rejection 'non-exhaustive if-case unexpectedly accepted' "$work/missing.err" \
    "$ziran" check --diagnostics=json --root "$work" "$work/modes.zi" "$work/missing.zi"
grep -Fq '"code":"check.if_case"' "$work/missing.err"
grep -Fq 'non-exhaustive if-case; missing case: Later' "$work/missing.err"

cat > "$work/duplicate.zi" <<'ZI'
#import "modes"
Value :: (mode: Mode) -> s32 {
    if #complete mode == {
        case Mode.Off;
            return 1
        case Mode.On;
            return 2
        case Mode.On;
            return 3
    }
}
ZI
expect_rejection 'duplicate if-case member unexpectedly accepted' "$work/duplicate.err" \
    "$ziran" check --root "$work" "$work/modes.zi" "$work/duplicate.zi"
grep -Fq 'duplicate if-case member: On' "$work/duplicate.err"

cat > "$work/not_enum.zi" <<'ZI'
Value :: (number: s32) -> s32 {
    if #complete number == {
        case .Zero;
            return 0
    }
}
ZI
expect_rejection 'non-enum complete if-case unexpectedly accepted' "$work/not_enum.err" \
    "$ziran" check --root "$work" "$work/not_enum.zi"
grep -Fq 'if-case requires a checked enum or scalar value' "$work/not_enum.err"

cat > "$work/inline.zi" <<'ZI'
Mode :: enum { Off :: -1; On :: 1; }
#program_export
Answer :: () -> s32 {
    using Mode;
    if #complete cast(Mode)On == {
        case Mode.Off;
            return 0
        case Mode.On;
            return 42
    }
}
ZI
"$ziran" bundle --root "$work" --entry inline:Answer \
    -o "$work/inline.zx" "$work/inline.zi"
test "$("$ziran" run "$work/inline.zx")" = 42

cat > "$work/partial.zi" <<'ZI'
Color :: enum { Red; Blue; }
Value :: (color: Color) -> s32 {
    if color == {
    case .Red;
        return 1;
    }
    return 2;
}
#program_export
Answer :: () -> s32 {
    return Value(cast(Color)0) + Value(cast(Color)1) + 39;
}
ZI
"$ziran" bundle --root "$work" --entry partial:Answer \
    -o "$work/partial.zx" "$work/partial.zi"
test "$("$ziran" run "$work/partial.zx")" = 42

cat > "$work/unqualified.zi" <<'ZI'
Color :: enum { Red; Blue; }
Value :: (color: Color) -> s32 {
    if #complete color == {
        case Red;
            return 1
        case .Blue;
            return 2
    }
}
ZI
expect_rejection 'unqualified Jai enum case was accepted' "$work/unqualified.err" \
    "$ziran" check --root "$work" "$work/unqualified.zi"
grep -Fq "enum case requires 'case .Member;'" "$work/unqualified.err"

cat > "$work/old_match.zi" <<'ZI'
Color :: enum { Red; Blue; }
Value :: (color: Color) -> s32 {
    match color {
        case Red: return 1
        case Blue: return 2
    }
}
ZI
expect_rejection 'non-Jai enum match was accepted' "$work/old_match.err" \
    "$ziran" check --root "$work" "$work/old_match.zi"
grep -Fq 'match is not Jai syntax' "$work/old_match.err"

{
    printf 'Many :: enum {\n'
    index=0
    while test "$index" -lt 70; do
        printf '    Item%d :: %d;\n' "$index" "$index"
        index=$((index + 1))
    done
    printf '}\n#program_export\nAnswer :: () -> s32 {\n    using Many;\n    if #complete cast(Many)Item69 == {\n'
    index=0
    while test "$index" -lt 70; do
        printf '        case Many.Item%d;\n            return %d\n' "$index" "$index"
        index=$((index + 1))
    done
    printf '    }\n}\n'
} > "$work/many.zi"
"$ziran" bundle --root "$work" --entry many:Answer \
    -o "$work/many.zx" "$work/many.zi"
test "$("$ziran" run "$work/many.zx")" = 69
