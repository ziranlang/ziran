#!/bin/sh
set -eu

# An if-case over an integer or enum is a switch in C and Go, with the same
# results as the portable VM. An arm that breaks out of a loop keeps the if
# chain, since break inside a switch would leave only the switch. `case;` is
# the default arm for an enum subject as for an integer, and a label such as
# #char "{" does not open a block. A procedure with an if-case still stores an
# opened enum member (using Tab) in an integer.
ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/switches.zi" <<'EOF'
Hue :: enum {
    RED;
    GREEN;
    BLUE;
}
Label :: (hue: Hue) -> string {
    if hue == {
        case .RED; return "red";
        case .GREEN; return "green";
    }
    return "blue";
}
Warmth :: (hue: Hue) -> string {
    result := "cool";
    if hue == {
        case .RED; result = "warm";
        case; result = "other";
    }
    return result;
}
Tab :: enum {
    TAB_HOME :: 0;
    TAB_PRACTICE :: 1;
}
using Tab;
Opened :: (mode: s32) -> s32 {
    stored: s32 = TAB_PRACTICE;
    if mode == {
        case 1; stored += 10;
        case; stored += 100;
    }
    return stored;
}
Bracket :: (byte: u8) -> s32 {
    result: s32 = 0;
    if byte == {
        case #char "{";
            result = 1;
        case #char "}";
            result = 2;
        case;
            result = 3;
    }
    return result;
}
Size :: (n: s32) -> s32 {
    result: s32 = 0;
    if n == {
        case 1;
            doubled := n * 2;
            result = doubled;
        case 2;
            result = 20;
        case;
            result = -1;
    }
    return result;
}
Shifted :: (n: s32, value: s32) -> s32 {
    result: s32 = 0;
    if n == {
        case 1;
            result = value;
            adjusted := value + 1;
            result = adjusted;
        case 2;
            result = cast(u8)(0xC0 | (value >> 6));
        case 3;
            result = cast(u8)(0xE0 | (value >> 12));
        case;
            result = cast(u8)(0xF0 | (value >> 18));
    }
    return result;
}
main :: () {
    print("% % %\n", Label(.RED), Label(.GREEN), Label(.BLUE));
    print("% % %\n", Size(1), Size(2), Size(9));
    print("% %\n", Warmth(.RED), Warmth(.BLUE));
    print("% % %\n", Bracket(123), Bracket(125), Bracket(65));
    print("% %\n", Opened(1), Opened(2));
    print("% % % %\n", Shifted(1, 1061), Shifted(2, 1061), Shifted(3, 1061), Shifted(9, 1061));
    for step: 0..5 {
        if step == {
            case 3; break;
            case 1; print("one\n");
        }
        print("step %\n", step);
    }
}
EOF
cat > "$work/expected" <<'EOF'
red green blue
2 20 -1
warm other
1 2 3
11 101
1062 208 224 240
step 0
one
step 1
step 2
EOF

"$ziran" bundle --root "$work" --entry switches:main -o "$work/switches.zib" "$work/switches.zi"
"$ziran" run "$work/switches.zib" > "$work/vm.out"

"$ziran" build --target=c --root "$work" -o "$work/c" "$work/switches.zi"
printf '#include "switches.h"\nint main(void) { switches_main(); return 0; }\n' > "$work/c/run.c"
"${CC:-cc}" -std=c99 -I"$work/c" "$work/c/switches.c" "$work/c/run.c" -o "$work/c/program"
"$work/c/program" > "$work/c.out"
grep -Fq 'switch (hue) {' "$work/c/switches.c"
grep -Fq 'case Hue_RED:' "$work/c/switches.c"

"$ziran" build --target=cpp --root "$work" -o "$work/cpp" "$work/switches.zi"
printf '#include "switches.hpp"\nint main() { switches_main(); return 0; }\n' > "$work/cpp/run.cpp"
"${CXX:-c++}" -std=c++17 -I"$work/cpp" "$work/cpp/switches.cpp" "$work/cpp/run.cpp" \
    -o "$work/cpp/program"
"$work/cpp/program" > "$work/cpp.out"

"$ziran" build --target=go --pkg main --exe --entry switches:main --root "$work" \
    -o "$work/go" "$work/switches.zi"
(cd "$work/go" && GO111MODULE=off go run .) > "$work/go.out"
grep -Fq 'switch n {' "$work/go/switches.go"
grep -Fq 'default:' "$work/go/switches.go"

"$ziran" build --target=rust --exe --entry switches:main --root "$work" \
    -o "$work/rust" "$work/switches.zi"
CARGO_TARGET_DIR=$work/rust-target cargo build --quiet \
    --manifest-path "$work/rust/Cargo.toml" 2> /dev/null
"$work/rust-target/debug/ziran_generated" > "$work/rust.out"

for target in vm c cpp go rust; do
    if ! cmp -s "$work/expected" "$work/$target.out"; then
        echo "$target switches differ" >&2
        diff "$work/expected" "$work/$target.out" >&2 || true
        exit 1
    fi
done
