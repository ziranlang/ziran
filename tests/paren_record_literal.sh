#!/bin/sh
set -eu
ziran=$1
work=$(mktemp -d)
trap "rm -rf $work" EXIT HUP INT TERM
cat > "$work/paren_record.zi" <<'EOF'
Color2 :: struct { r: u8; g: u8; b: u8; a: u8 }
Pick :: (flag: bool) -> Color2 {
    chosen: Color2 = ifx (flag) then (Color2.{.r = 1, .g = 2, .b = 3, .a = 4}) else (Color2.{.r = 5, .g = 6, .b = 7, .a = 8})
    return chosen
}
#program_export
Answer :: () -> s32 {
    value := Pick(true)
    return cast(s32)value.r + 41
}
EOF
"$ziran" check --root "$work" "$work/paren_record.zi"
"$ziran" ir --root "$work" -o "$work/ir" "$work/paren_record.zi"
"$ziran" bundle --root "$work" --entry paren_record:Answer -o "$work/a.zx" "$work/paren_record.zi"
test "$("$ziran" run "$work/a.zx")" = 42
cat > "$work/bad_cast.zi" <<'EOF'
Answer :: () -> s32 {
    return (s32)1
}
EOF
if "$ziran" check --root "$work" "$work/bad_cast.zi" 2> "$work/bad_cast.err"; then
    echo "C-style cast accepted" >&2
    exit 1
fi
grep -Fq "C-style cast" "$work/bad_cast.err"
