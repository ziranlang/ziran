#!/bin/sh
# Shapes a UI library's code takes that every target must agree on: slicing
# a buffer with an s32 bound, negative numbers in a float32 table, an s32
# index compared with a folded constant product, and widget modules that
# import each other while another module needs startup.
set -eu

ziran=${1:?pass the ziran command}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/button.zi" <<'ZI'
#import "label"
ButtonWidth :: (text: string) -> s32 { return LabelWidth(text) + 8 }
ZI
cat > "$work/label.zi" <<'ZI'
#import "button"
LabelWidth :: (text: string) -> s32 { return cast(s32)text.count * 6 }
LabelInButton :: (text: string) -> s32 { return ButtonWidth(text) - 8 }
ZI
cat > "$work/theme.zi" <<'ZI'
Base :: () -> s32 { return 40 }
accent: s32 = Base() + 2;
ZI
cat > "$work/shapes.zi" <<'ZI'
#import "button"
#import "label"
#import "theme"

Width :: 96;
Height :: 60;
circle: [4]float32 = .{1.0, 0.5, -0.5, -1.0};
buffer: [32]u8;

Joined :: (first: string, second: string, output: []u8) -> string {
    length: s32 = 0
    index: s32 = 0
    while index < cast(s32)first.count && length < cast(s32)output.count {
        output[length] = first[index]
        length += 1
        index += 1
    }
    index = 0
    while index < cast(s32)second.count && length < cast(s32)output.count {
        output[length] = second[index]
        length += 1
        index += 1
    }
    return TextView(output[0:length])
}

#program_export
main :: () -> s32 {
    cells: s32 = 0
    index: s32 = 0
    while index < Width * Height {
        cells += 1
        index += 16
    }
    total: float32 = circle[0] + circle[1] + circle[2] + circle[3] + circle[2]
    print("% % % % %\n", cells, total, Joined("Ki", "ryon", buffer[:]),
        ButtonWidth("ok") + LabelInButton("go"), accent)
    return 0
}
ZI
sources="$work/shapes.zi $work/button.zi $work/label.zi $work/theme.zi"
expected='360 -0.5 Kiryon 32 42'

"$ziran" build --target=c --exe --root "$work" --entry shapes:main -o "$work/c" $sources
test "$("$work/c/shapes")" = "$expected"

"$ziran" build --target=cpp --root "$work" --entry shapes:main -o "$work/cpp" $sources
"${CXX:-c++}" -std=c++17 -I"$work/cpp" "$work"/cpp/*.cpp -o "$work/cpp/program"
test "$("$work/cpp/program")" = "$expected"

"$ziran" build --target=go --pkg main --exe --root "$work" --entry shapes:main \
    -o "$work/go" $sources
(cd "$work/go" && GO111MODULE=off go build -o program .)
test "$("$work/go/program")" = "$expected"

"$ziran" build --target=rust --exe --root "$work" --entry shapes:main \
    -o "$work/rust" $sources
CARGO_TARGET_DIR=$work/rust-target cargo build --quiet \
    --manifest-path "$work/rust/Cargo.toml"
test "$("$work/rust-target/debug/ziran_generated")" = "$expected"

"$ziran" build --target=py --exe --root "$work" --entry shapes:main \
    -o "$work/py" $sources
test "$(python3 "$work/py")" = "$expected"

"$ziran" bundle --root "$work" --entry shapes:main -o "$work/shapes.zib" $sources
test "$("$ziran" run "$work/shapes.zib")" = "$expected
0"
