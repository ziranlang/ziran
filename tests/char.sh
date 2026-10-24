#!/bin/sh
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/character.zi" <<'ZI'
Letter :: struct {
    char: s64
    ziran_keyword_char_0: s64
}

#program_export
Answer :: () -> s64 {
    letter: Letter
    letter.char = #char "A"
    letter.ziran_keyword_char_0 = 1
    char := letter.char
    if size_of(type_of(char)) != 8 { return 0 }
    return char - 23 + letter.ziran_keyword_char_0 - 1
}
ZI

"$ziran" ir --root "$work" -o "$work/ir" "$work/character.zi"
for input in source saved; do
    if test "$input" = source; then
        source="$work/character.zi"
    else
        source="$work/ir/character.zir"
    fi
    "$ziran" bundle --root "$work" --entry character:Answer \
        -o "$work/$input.zx" "$source"
    test "$("$ziran" run "$work/$input.zx")" = 42
    for target in c go; do
        output="$work/$input-$target"
        "$ziran" build "--target=$target" --root "$work" \
            -o "$output" "$source"
        case "$target" in
            c)
                printf '#include "character.h"\nint main(void) { return Answer() == 42 ? 0 : 1; }\n' \
                    > "$work/main.c"
                "${CC:-cc}" -std=c11 -I"$repo/include" -I"$output" \
                    "$output/character.c" "$work/main.c" -o "$output/app"
                "$output/app"
                ;;
            go)
                printf 'package ziran\nimport "testing"\nfunc TestCharacter(t *testing.T) { if Character_Answer() != 42 { t.Fatal("character") } }\n' \
                    > "$output/character_test.go"
                GO111MODULE=off go test "$output"/*.go
                ;;
        esac
    done
done
cmp "$work/source.zx" "$work/saved.zx"

cat > "$work/bad.zi" <<'ZI'
Answer :: () -> s64 { return #char "AB" }
ZI
if "$ziran" check --root "$work" "$work/bad.zi" \
    2> "$work/bad.err"; then
    echo '#char accepted multiple bytes' >&2
    exit 1
fi
grep -Fq '#char requires a one-byte string literal' "$work/bad.err"

cat > "$work/old_type.zi" <<'ZI'
Letter :: struct {
    value: char
}
ZI
if "$ziran" check --root "$work" "$work/old_type.zi" \
    2> "$work/old_type.err"; then
    echo 'non-Jai char type was accepted' >&2
    exit 1
fi
grep -Fq 'non-Jai primitive type spelling: char' "$work/old_type.err"

for expression in 'cast(char)65' 'size_of(char)'; do
    printf 'Answer :: () -> s64 { return %s }\n' "$expression" > "$work/old_type.zi"
    if "$ziran" check --root "$work" "$work/old_type.zi" \
        2> "$work/old_type.err"; then
        echo "non-Jai type expression was accepted: $expression" >&2
        exit 1
    fi
    grep -Fq 'non-Jai primitive type spelling: char' "$work/old_type.err"
done
