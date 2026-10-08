#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
cat > "$work/assignment.zi" <<'ZI'
Point :: struct { value: s32; }
State :: struct {
    matrix: [2][3]s32
    bytes: [2][3]u8
    words: [2][2]string
    points: [2][2]Point
    volume: [2][2][2]s32
}
#program_export
Answer :: () -> s32 {
    state: State; source: State
    state.matrix[0][2]=9
    source.matrix[1][2]=40; source.bytes[1][2]=cast(u8)65
    source.words[1][1]="copied"; source.points[1][1].value=7
    source.volume[1][1][1]=2
    state=source
    if state.matrix[1][2]!=40 || state.points[1][1].value!=7 || state.matrix[0][2]!=0 { return 1 }
    if state.bytes[1][2]!=cast(u8)65 || state.words[1][1]!="copied" || state.volume[1][1][1]!=2 { return 2 }
    source.matrix[1][2]=0; source.points[1][1].value=0; source.words[1][1]="changed"
    if state.matrix[1][2]!=40 || state.points[1][1].value!=7 || state.words[1][1]!="copied" { return 3 }
    source.matrix=state.matrix
    state.matrix[1][2]=1
    if source.matrix[1][2]!=40 { return 4 }
    state=state
    return source.matrix[1][2]+state.volume[1][1][1]
}
#program_export
Main :: () -> s32 { result:=Answer(); return ifx result==42 then 0 else result }
ZI
"$ziran" ir --root "$work" -o "$work/ir" "$work/assignment.zi"
for form in source saved; do
    root=$work; input=$work/assignment.zi
    if test "$form" = saved; then root=$work/ir; input=$root/assignment.zir; fi
    "$ziran" bundle --root "$root" --entry assignment:Answer -o "$work/$form.zib" "$input"
    answer=$("$ziran" run "$work/$form.zib")
    if test "$answer" != 42; then echo "Nested assignment $form VM returned $answer, expected 42" >&2; exit 1; fi
    "$ziran" build --target=py --exe --root "$root" --entry assignment:Main -o "$work/$form-py" "$input"
    python3 "$work/$form-py"
    for target in c cpp; do
        output=$work/$form-$target
        if test "$target" = c; then
            "$ziran" build --target=c --exe --root "$root" --entry assignment:Main -o "$output" "$input"
            "$output/assignment"
        else
            "$ziran" build --target=cpp --no-main --root "$root" -o "$output" "$input"
            printf '#include "assignment.hpp"\nint main() { return Main(); }\n' > "$output/main.cpp"
            "${CXX:-c++}" -O2 -std=c++17 -I"$repo/include" -I"$output" "$output"/*.cpp -lm -o "$output/app"
            "$output/app"
        fi
    done
    output=$work/$form-go
    "$ziran" build --target=go --pkg main --root "$root" -o "$output" "$input"
    printf 'package main\nfunc main() { if Assignment_Main()!=0 { panic("nested array assignment") } }\n' > "$output/main.go"
    GO111MODULE=off go run "$output"/*.go
    output=$work/$form-rust
    "$ziran" build --target=rust --exe --root "$root" --entry assignment:Main -o "$output" "$input"
    CARGO_TARGET_DIR="$work/rust-target" cargo build --quiet --offline --manifest-path "$output/Cargo.toml"
    "$work/rust-target/debug/ziran_generated"
done
cmp "$work/source.zib" "$work/saved.zib"
echo 'Nested array assignments preserve independent copied values in source/saved VM, Python, C/C++, Go and Rust'
