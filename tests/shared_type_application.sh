#!/bin/sh
set -eu

ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/values.zi" <<'ZI'
#import "vec"
Box :: struct($T: Type) { value: T }
Many :: struct($A: Type, $B: Type, $C: Type, $D: Type, $E: Type, $F: Type, $G: Type, $H: Type, $I: Type) { value: A }
MakeMany :: () -> Many(u8, u8, u8, u8, u8, u8, u8, u8, u16) {
    result: Many(u8, u8, u8, u8, u8, u8, u8, u8, u16)
    result.value = cast(u8)42
    return result
}
MakeBox :: () -> Box(u8) {
    result: Box(u8)
    result.value = cast(u8)42
    return result
}
MakeValues :: () -> Vec(u8) {
    result: Vec(u8)
    VecPush(result, cast(u8)40)
    return result
}
ZI
cat > "$work/app.zi" <<'ZI'
#import "vec"
#import "values"
#program_export
SelfTest :: () -> s32 {
    values: Vec(u8) = MakeValues()
    VecPush(values, cast(u8)2)
    copy: Vec(u8)
    if !VecClone(copy, values) {
        return 0
    }
    VecSwap(copy, values)
    boxed: Box(u8) = MakeBox()
    many: Many(u8, u8, u8, u8, u8, u8, u8, u8, u16) = MakeMany()
    if boxed.value != cast(u8)42 || many.value != cast(u8)42 || copy.count != 2 {
        return 0
    }
    return cast(s32)values[0] + cast(s32)values[1]
}
ZI
"$ziran" ir --root "$work" --module-path std -o "$work/ir" "$work/app.zi"
for input in source saved; do
    if test "$input" = source; then
        root=$work
        file=$work/app.zi
    else
        root=$work/ir
        file=$work/ir/app.zir
    fi
    for target in c cpp go; do
        out=$work/$input-$target
        if test "$target" = go; then
            "$ziran" build --target=go --no-main --pkg main --root "$root" --module-path std -o "$out" "$file"
        else
            "$ziran" build "--target=$target" --no-main --root "$root" --module-path std -o "$out" "$file"
        fi
        case "$target" in
        c)
            printf '#include "app.h"\nint main(void) { return SelfTest() == 42 ? 0 : 1; }\n' > "$out/main.c"
            ${CC:-cc} -Iinclude -I"$out" "$out"/*.c -o "$out/test"
            "$out/test"
            ;;
        cpp)
            printf '#include "app.hpp"\nint main() { return SelfTest() == 42 ? 0 : 1; }\n' > "$out/main.cpp"
            ${CXX:-c++} -Iinclude -I"$out" "$out"/*.cpp -o "$out/test"
            "$out/test"
            ;;
        go)
            printf 'package main\nfunc main() { if App_SelfTest() != 42 { panic("shared vector") } }\n' > "$out/main.go"
            GO111MODULE=off go run "$out"/*.go
            ;;
        esac
    done
    "$ziran" bundle --root "$root" --module-path std --entry app:SelfTest -o "$work/$input.zib" "$file"
    test "$("$ziran" run "$work/$input.zib")" = 42
done

cat > "$work/different_argument.zi" <<'ZI'
#import "vec"
#import "values"
Wrong :: () -> s32 {
    values: Vec(u16) = MakeValues()
    return 0
}
ZI
cat > "$work/different_template.zi" <<'ZI'
#import "values"
Box :: struct($T: Type) { value: T }
Wrong :: () -> s32 {
    value: Box(u8) = MakeBox()
    return 0
}
ZI
cat > "$work/different_phantom_argument.zi" <<'ZI'
#import "values"
Wrong :: () -> s32 {
    value: Many(u8, u8, u8, u8, u8, u8, u8, u8, u8) = MakeMany()
    return 0
}
ZI
for invalid in different_argument different_template different_phantom_argument; do
    if "$ziran" check --root "$work" --module-path std "$work/$invalid.zi" \
        > "$work/$invalid.out" 2> "$work/$invalid.err"; then
        echo "incompatible direct type application was accepted: $invalid" >&2
        exit 1
    fi
    test -s "$work/$invalid.err"
done
echo 'shared direct type application identity: passed'
