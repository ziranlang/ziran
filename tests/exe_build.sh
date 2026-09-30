#!/bin/sh
# ziran build --target=c --exe --entry module:function writes the C and
# compiles it into DIR/module with the toolchain's headers. An entry takes
# nothing or (argc, argv) and returns nothing or an integer; an exported
# main is the program's main as it is.
set -eu

ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
Answer :: () -> s32 {
    print("answer\n")
    return 7
}
Quiet :: () {
    print("quiet\n")
}
Count :: (argc: s32, argv: **u8) -> s32 {
    return argc
}
Name :: () -> string { return "no" }
ZI
cat > "$work/tool.zi" <<'ZI'
#program_export
main :: (argc: s32, argv: **s8) -> s32 {
    print("% arguments\n", argc - 1)
    return 0
}
ZI

build() {
    "$ziran" build --target=c --exe --entry "$1" --root "$work" -o "$work/$2" "$work/$3"
}

build app:Answer answer app.zi
status=0
"$work/answer/app" > "$work/answer.out" || status=$?
test "$status" = 7
test "$(cat "$work/answer.out")" = answer

build app:Quiet quiet app.zi
test "$("$work/quiet/app")" = quiet

build app:Count count app.zi
status=0
"$work/count/app" one two || status=$?
test "$status" = 3

build tool:main tool tool.zi
test ! -e "$work/tool/ziran_exe_main.c"
test "$("$work/tool/tool" a b c)" = "3 arguments"

if build app:Name name app.zi 2> "$work/name.err"; then
    echo 'an entry returning a string was accepted' >&2
    exit 1
fi
grep -q 'must return nothing or an integer' "$work/name.err"

if "$ziran" build --target=c --exe --root "$work" -o "$work/none" "$work/app.zi" 2> "$work/none.err"; then
    echo '--exe without --entry was accepted' >&2
    exit 1
fi
grep -q -- '--exe needs --entry' "$work/none.err"
