#!/bin/sh
set -eu

ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/variant.zi" <<'ZI'
Choice :: variant { Empty; Number: s32; }
ZI
cat > "$work/anon_enum.zi" <<'ZI'
#enum { Empty; Number; }
ZI
cat > "$work/match.zi" <<'ZI'
Choice :: enum { Empty; Number; }
Read :: (choice: Choice) -> s32 {
    match choice {
        case Empty: return 0
        case Number: return 1
    }
}
ZI
cat > "$work/postfix.zi" <<'ZI'
Value :: () -> s32 { return 42 }
Read :: () -> s32 { return Value()? }
ZI
cat > "$work/guard.zi" <<'ZI'
Answer :: () -> s32 {
    guard true
    return 42
}
ZI
cat > "$work/switch.zi" <<'ZI'
Answer :: () -> s32 {
    switch 1 {
        case 1: return 42
    }
}
ZI
cat > "$work/goto.zi" <<'ZI'
Answer :: () -> s32 {
    goto done
    return 0
    done:
    return 42
}
ZI
cat > "$work/default_label.zi" <<'ZI'
Answer :: () -> s32 {
    if 1 == {
        default:
            return 42
    }
}
ZI
cat > "$work/label.zi" <<'ZI'
Answer :: () -> s32 {
    done:
    return 42
}
ZI
cat > "$work/state.zi" <<'ZI'
state {
    count: s32
}
ZI
cat > "$work/c_local.zi" <<'ZI'
Answer :: () -> s32 {
    int count = 42;
    return count
}
ZI
cat > "$work/raw_c.zi" <<'ZI'
Answer :: () -> s32 {
    c int count = 42;
    return count
}
ZI
cat > "$work/slot_decl.zi" <<'ZI'
Child :: (value: s32) #slot
ZI
cat > "$work/slot_body.zi" <<'ZI'
Answer :: () -> s32 {
    child = (value: s32) #slot {
        return
    }
    return 42
}
ZI
cat > "$work/block_call.zi" <<'ZI'
Props :: struct {
    value: s32
}
Read :: (props: Props) -> s32 {
    return props.value
}
Answer :: () -> s32 {
    Read result: {
        value = 42
    }
    return result
}
ZI
cat > "$work/c_global_literal.zi" <<'ZI'
Props :: struct { value: s32; }
item: Props = (Props){.value = 42};
#program_export
Answer :: () -> s32 { return item.value }
ZI
cat > "$work/c_constant_ternary.zi" <<'ZI'
VALUE :: 1 ? 42 : 0
#program_export
Answer :: () -> s32 { return VALUE }
ZI
cat > "$work/c_multiline_ternary.zi" <<'ZI'
Answer :: () -> s32 {
    return true ? 42
        : 0
}
ZI
cat > "$work/c_adjacent_strings.zi" <<'ZI'
Answer :: () -> string {
    return "a"
           "b"
}
ZI

for feature in variant anon_enum match postfix guard switch goto default_label label state c_local raw_c slot_decl slot_body block_call c_global_literal c_constant_ternary c_multiline_ternary c_adjacent_strings; do
    if "$ziran" check --root "$work" "$work/$feature.zi" \
        2> "$work/$feature.err"; then
        echo "non-Jai $feature syntax was accepted" >&2
        exit 1
    fi
    case "$feature" in
        variant) message='variant declarations are not Jai syntax' ;;
        anon_enum) message='#enum is not Jai syntax' ;;
        match) message='match is not Jai syntax' ;;
        postfix) message='postfix ? is not Jai syntax' ;;
        guard) message='guard is not Jai syntax' ;;
        switch) message='switch is not Jai syntax' ;;
        goto|label) message='goto and labels are not Jai syntax' ;;
        default_label) message='goto and labels are not Jai syntax' ;;
        state) message='state blocks are not Jai syntax' ;;
        c_local) message='C-style local declarations are not Jai syntax' ;;
        raw_c) message='raw C statements are not Jai syntax' ;;
        slot_decl|slot_body) message='#slot is not Jai syntax' ;;
        block_call) message='block calls are not Jai syntax' ;;
        c_global_literal) message='C-style cast or literal is not valid Jai syntax' ;;
        c_constant_ternary) message='C-style conditional is not valid Jai syntax' ;;
        c_multiline_ternary) message='C-style conditional is not valid Jai syntax' ;;
        c_adjacent_strings) message='statement is not supported by language checking' ;;
    esac
    grep -Fq "$message" "$work/$feature.err"
    for target in c go; do
        if "$ziran" build --target="$target" --root "$work" \
            -o "$work/out-$feature-$target" "$work/$feature.zi" \
            2> "$work/build-$feature-$target.err"; then
            echo "$target build accepted non-Jai $feature syntax" >&2
            exit 1
        fi
        grep -Fq "$message" "$work/build-$feature-$target.err"
    done
done

cat > "$work/names.zi" <<'ZI'
#program_export
Answer :: () -> s32 {
    match: s32 = 12
    variant: s32 = 10
    guard: s32 = 5
    switch: s32 = 4
    goto: s32 = 3
    state: s32 = 6
    c: s32 = 2
    return match + variant + guard + switch + goto + state + c
}
ZI
# Checking is unconditional; the old explicit switch is no longer accepted.
for target in c go; do
    if "$ziran" build "--target=$target" --strict --root "$work" \
        -o "$work/strict-$target" "$work/names.zi" \
        2> "$work/strict-$target.err"; then
        echo "$target build accepted removed --strict option" >&2
        exit 1
    fi
    grep -Fq 'usage:' "$work/strict-$target.err"
done
"$ziran" check --root "$work" "$work/names.zi"
"$ziran" bundle --root "$work" --entry names:Answer \
    -o "$work/names.zx" "$work/names.zi"
test "$("$ziran" run "$work/names.zx")" = 42

cat > "$work/state_name.zi" <<'ZI'
Record :: struct {
    value: s32
}
state: Record = Record.{value = 42};
#program_export
Answer :: () -> s32 { return state.value }
ZI
"$ziran" check --root "$work" "$work/state_name.zi"
"$ziran" build --target=c --root "$work" \
    -o "$work/state-name-c" "$work/state_name.zi"

cat > "$work/bindings.zi" <<'ZI'
Bindings :: (switch: s32, goto: s32) -> s32 {
    ziran_keyword_switch_0: s32 = 4
    ziran_keyword_goto_0: s32 = 8
    return switch + goto + ziran_keyword_switch_0 + ziran_keyword_goto_0
}
#program_export
Answer :: () -> s32 { return Bindings(10, 20) }
ZI
"$ziran" check --root "$work" "$work/bindings.zi"
"$ziran" ir --root "$work" -o "$work/bindings-ir" "$work/bindings.zi"
for input in source saved; do
    if test "$input" = source; then
        file=$work/bindings.zi
        root=$work
    else
        file=$work/bindings-ir/bindings.zir
        root=$work/bindings-ir
    fi
    "$ziran" bundle --root "$root" --entry bindings:Answer \
        -o "$work/bindings-$input.zx" "$file"
    test "$("$ziran" run "$work/bindings-$input.zx")" = 42
    for target in c go; do
        out=$work/bindings-$input-$target
        if test "$target" = go; then
            "$ziran" build --target=go --pkg main --root "$root" \
                -o "$out" "$file"
        else
            "$ziran" build "--target=$target" --root "$root" \
                -o "$out" "$file"
        fi
        case "$target" in
            c)
                cat > "$out/main.c" <<'C'
#include "bindings.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
                ${CC:-cc} -std=c11 -Iinclude -I"$out" \
                    "$out/bindings.c" "$out/main.c" -o "$out/app"
                "$out/app" ;;
            go)
                cat > "$out/main.go" <<'GO'
package main
func main() { if Bindings_Answer() != 42 { panic("wrong result") } }
GO
                GO111MODULE=off go run "$out"/*.go ;;
        esac
    done
done

cat > "$work/unresolved.zi" <<'ZI'
Answer :: () -> s32 { return Missing }
ZI
for target in c go; do
    if "$ziran" build "--target=$target" --root "$work" \
        -o "$work/unresolved-$target" "$work/unresolved.zi" \
        2> "$work/unresolved-$target.err"; then
        echo "unchecked $target build accepted an unresolved name" >&2
        exit 1
    fi
    grep -Fq 'unresolved name: Missing' "$work/unresolved-$target.err"
    if "$ziran" build "--target=$target" --no-strict --root "$work" \
        -o "$work/no-strict-$target" "$work/names.zi" \
        2> "$work/no-strict-$target.err"; then
        echo "$target build accepted --no-strict" >&2
        exit 1
    fi
    grep -Fq 'usage:' "$work/no-strict-$target.err"
done
