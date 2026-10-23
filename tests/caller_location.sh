#!/bin/sh
set -eu

ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/lib/native_answer.sh"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/lib.zi" <<'ZI'
Captured :: (loc := #caller_location) -> Source_Code_Location {
    return loc
}
Typed :: (loc: Source_Code_Location = #caller_location) -> Source_Code_Location {
    return loc
}
Generic :: (value: $T, loc := #caller_location) -> s64 {
    return loc.line_number
}
Nested :: () -> s64 {
    return Captured().line_number
}
ZI
cat > "$work/extra.zi" <<'ZI'
Loaded :: () -> bool {
    if Captured().line_number != #line { return false }
    if Captured().fully_pathed_filename != #file { return false }
    return true
}
ZI
cat > "$work/app.zi" <<'ZI'
#import "lib"
Library :: #import "lib";
#load "extra.zi";
CompileLine :: #run Library.Captured().line_number;
#assert CompileLine == 4;
#assert Library.Captured().line_number == 6;
CompileFile :: #run Library.Captured().fully_pathed_filename;
#assert CompileFile == #file;
NestedLine :: #run Library.Nested();
#assert NestedLine == 11;
#if Library.Captured().line_number == 11 {
CompileFlag :: 1;
} else {
CompileFlag :: 0;
}
#assert CompileFlag == 1;
Manual :: #run Library.Captured(
    loc = Source_Code_Location.{.fully_pathed_filename = "manual",
                                  .line_number = 77}).line_number;
#assert Manual == 77;
Local :: (loc := #caller_location) -> s64 {
    return loc.line_number
}
#if Local() == 24 {
LocalFlag :: 1;
} else {
LocalFlag :: 0;
}
#assert LocalFlag == 1;
#program_export
Answer :: () -> s32 {
    if Captured().line_number != #line { return 0 }
    if Captured().fully_pathed_filename != #file { return 0 }
    if Typed().line_number != #line { return 0 }
    if Library.Captured().line_number != #line { return 0 }
    value: s32 = 42
    if Generic(value) != #line { return 0 }
    if !Loaded() { return 0 }
    provided := Source_Code_Location.{.fully_pathed_filename = "manual",
                                      .line_number = 77}
    if Captured(loc = provided).line_number != 77 { return 0 }
    if Captured(loc = provided).fully_pathed_filename != "manual" { return 0 }
    continued: bool =
        Captured().line_number == #line
    if !continued { return 0 }
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
        native_answer_42 "$target" "$root" "$out" "$input" \
            app App_Answer 'caller location'
    done
done

cat > "$work/direct.zi" <<'ZI'
Answer :: () -> Source_Code_Location { return #caller_location }
ZI
if "$ziran" check --root "$work" "$work/direct.zi" 2> "$work/direct.err"; then
    echo '#caller_location outside a parameter default was accepted' >&2
    exit 1
fi
grep -Fq '#caller_location is only valid as a parameter default' \
    "$work/direct.err"
cat > "$work/file_scope.zi" <<'ZI'
HERE :: #caller_location;
ZI
if "$ziran" check --root "$work" "$work/file_scope.zi" \
    2> "$work/file_scope.err"; then
    echo '#caller_location at file scope was accepted' >&2
    exit 1
fi
grep -Fq '#caller_location is only valid as a parameter default' \
    "$work/file_scope.err"

cat > "$work/wrong_type.zi" <<'ZI'
Bad :: (loc: s32 = #caller_location) -> s32 { return loc }
ZI
if "$ziran" check --root "$work" "$work/wrong_type.zi" \
    2> "$work/wrong_type.err"; then
    echo '#caller_location with a wrong parameter type was accepted' >&2
    exit 1
fi
grep -Fq '#caller_location requires a Source_Code_Location parameter' \
    "$work/wrong_type.err"

cat > "$work/shadow_type.zi" <<'ZI'
Source_Code_Location :: struct { value: s32; }
ZI
if "$ziran" check --root "$work" "$work/shadow_type.zi" \
    2> "$work/shadow_type.err"; then
    echo 'built-in Source_Code_Location was redeclared' >&2
    exit 1
fi
grep -Fq 'built-in type cannot be redeclared' "$work/shadow_type.err"
