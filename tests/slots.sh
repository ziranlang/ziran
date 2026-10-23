#!/bin/sh
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/slotlib.zi" <<'EOF'

Child :: #type (value: s32) -> ();
Compute :: #type (value: s32) -> s32;

Props :: struct {
    value: s32
}
BuildProps :: #type (value: s32) -> Props;

#program_export
Container :: (props: Props, child: Child) -> s32 {
    child(props.value)
    return props.value + 1
}

#program_export
PlainChild :: (value: s32) {
    return
}

#program_export
Apply :: (value: s32, compute: Compute) -> s32 {
    return compute(value)
}

#program_export
AddOne :: (value: s32) -> s32 {
    return value + 1
}

#program_export
UseProps :: (value: s32, build: BuildProps) -> s32 {
    props: Props = build(value)
    return props.value + 1
}

#program_export
MakeProps :: (value: s32) -> Props {
    return Props.{value = value}
}
EOF
cat > "$work/old_slot.zi" <<'EOF'
Child :: (s32) -> ();
EOF
if "$ziran" check --root "$work" "$work/old_slot.zi" \
    2> "$work/old_slot.err"; then
    echo 'procedure type without Jai #type was accepted' >&2
    exit 1
fi
grep -Fq 'procedure types require Jai #type syntax' "$work/old_slot.err"
cat > "$work/bad_result.zi" <<'EOF'
Broken :: #type (value: s32) -> Missing;
EOF
if "$ziran" check --root "$work" "$work/bad_result.zi" \
    2> "$work/bad_result.err"; then
    echo 'procedure type with unknown result was accepted' >&2
    exit 1
fi
grep -Fq 'invalid procedure type result' "$work/bad_result.err"
cat > "$work/slotapp.zi" <<'EOF'
#import "slotlib"

observed: s32;

Observe :: (value: s32) {
    observed = value
}

#program_export
Answer :: () -> s32 {
    chosen: s32 = Container(.{value = 41}, Observe)
    if observed != 41 { return 0 }
    plain: s32 = Container(.{value = 41}, PlainChild)
    if plain != 42 { return 0 }
    result: s32 = Apply(41, AddOne)
    if result != 42 { return 0 }
    record_result: s32 = UseProps(41, MakeProps)
    if record_result != 42 { return 0 }
    return chosen
}
EOF

"$ziran" ir --root "$work" -o "$work/ir" \
    "$work/slotlib.zi" "$work/slotapp.zi"
for input in source saved; do
    if test "$input" = source; then
        set -- "$work/slotlib.zi" "$work/slotapp.zi"
    else
        set -- "$work/ir/slotlib.zir" "$work/ir/slotapp.zir"
    fi
    "$ziran" bundle --root "$work" --entry slotapp:Answer \
        -o "$work/$input.zx" "$@"
    test "$("$ziran" run "$work/$input.zx")" = 42
    c_out="$work/c-$input"
    cpp_out="$work/cpp-$input"
    go_out="$work/go-$input"
    "$ziran" build --target=c --root "$work" -o "$c_out" "$@"
    "$ziran" build --target=cpp --root "$work" -o "$cpp_out" "$@"
    "$ziran" build --target=go --pkg main \
        --root "$work" -o "$go_out" "$@"

    cat > "$c_out/main.c" <<'C'
#include "slotapp.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
    "${CC:-cc}" -std=c11 -I"$repo/include" -I"$c_out" \
        "$c_out/slotlib.c" "$c_out/slotapp.c" "$c_out/main.c" \
        -o "$c_out/app"
    "$c_out/app"

    cat > "$cpp_out/main.cpp" <<'CPP'
#include "slotapp.hpp"
int main() { return Answer() == 42 ? 0 : 1; }
CPP
    "${CXX:-c++}" -std=c++17 -I"$repo/include" -I"$cpp_out" \
        "$cpp_out/slotlib.cpp" "$cpp_out/slotapp.cpp" \
        "$cpp_out/main.cpp" -o "$cpp_out/app"
    "$cpp_out/app"

    cat > "$go_out/main.go" <<'GO'
package main
func main() { if Slotapp_Answer() != 42 { panic("slot result") } }
GO
    GO111MODULE=off go run "$go_out/slotlib.go" \
        "$go_out/slotapp.go" "$go_out/main.go"
done

cmp "$work/source.zx" "$work/saved.zx"

cat > "$work/wrong_callback.zi" <<'EOF'
#import "slotlib"
Wrong :: (value: bool) {
    return
}
Answer :: () -> s32 {
    return Container(Props.{value = 41}, Wrong)
}
EOF
cat > "$work/missing_callback.zi" <<'EOF'
#import "slotlib"
Answer :: () -> s32 {
    return Container(Props.{value = 41})
}
EOF
cat > "$work/wrong_return.zi" <<'EOF'
#import "slotlib"
Wrong :: (value: s32) -> bool {
    return true
}
Answer :: () -> s32 {
    return Apply(41, Wrong)
}
EOF
cat > "$work/wrong_record.zi" <<'EOF'
#import "slotlib"
Other :: struct {
    value: s32
}
Answer :: () -> s32 {
    return Container(Other.{value = 41}, PlainChild)
}
EOF
for bad in wrong_callback missing_callback wrong_record wrong_return; do
    if "$ziran" check --root "$work" "$work/$bad.zi" \
        > "$work/$bad.out" 2>&1; then
        echo "invalid procedure call passed checking: $bad" >&2
        exit 1
    fi
    case "$bad" in
        wrong_callback|wrong_return) diagnostic='function does not match slot signature' ;;
        missing_callback) diagnostic='argument count mismatch' ;;
        wrong_record) diagnostic='argument type mismatch' ;;
    esac
    grep -Fq "$diagnostic" "$work/$bad.out"
    if "$ziran" bundle --root "$work" --entry "$bad:Answer" \
        -o "$work/$bad.zx" "$work/$bad.zi" \
        > "$work/$bad.out" 2>&1; then
        echo "invalid procedure call entered a bundle: $bad" >&2
        exit 1
    fi
    for target in c cpp go; do
        if "$ziran" build --target="$target" --root "$work" \
            -o "$work/$bad-$target" "$work/$bad.zi" \
            > "$work/$bad.out" 2>&1; then
            echo "invalid procedure call built for $target: $bad" >&2
            exit 1
        fi
    done
done
