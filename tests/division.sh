#!/bin/sh
set -eu

# Division, remainder, and shifts give the same results on every target:
# truncation toward zero, the most negative value divided by -1 wraps, and
# division by zero stops the program.
ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/divide.zi" <<'EOF'
Quotients :: (a: s32, b: s32) {
    print("% % % % % %\n", a / b, a % b, a / 7, a % 7, a << 3, a >> 2);
}
main :: () {
    Quotients(-17, 5);
    Quotients(17, -5);
    small: s8 = -128;
    minus: s8 = -1;
    print("% %\n", small / minus, small % minus);
    wide: s64 = -9223372036854775807 - 1;
    wide_minus: s64 = -1;
    print("% %\n", wide / wide_minus, wide % wide_minus);
    byte: u8 = 250;
    print("% % % %\n", byte / 7, byte % 7, byte >> 3, byte << 1);
    word: u32 = 4000000000;
    print("% %\n", word / 3, word % 1000);
}
EOF
cat > "$work/expected" <<'EOF'
-3 -2 -2 -3 -136 -5
-3 2 2 3 136 4
-128 0
-9223372036854775808 0
35 5 31 244
1333333333 0
EOF
cat > "$work/zero.zi" <<'EOF'
Divide :: (a: s32, b: s32) -> s32 { return a / b; }
main :: () {
    print("%\n", Divide(1, 0));
}
EOF

for name in divide zero; do
    "$ziran" build --target=c --root "$work" -o "$work/c-$name" "$work/$name.zi"
    printf '#include "%s.h"\nint main(void) { %s_main(); return 0; }\n' "$name" "$name" \
        > "$work/c-$name/run.c"
    "${CC:-cc}" -std=c99 -I"$work/c-$name" "$work/c-$name/$name.c" \
        "$work/c-$name/run.c" -o "$work/c-$name/program"
    "$ziran" build --target=cpp --root "$work" -o "$work/cpp-$name" "$work/$name.zi"
    printf '#include "%s.hpp"\nint main() { %s_main(); return 0; }\n' "$name" "$name" \
        > "$work/cpp-$name/run.cpp"
    "${CXX:-c++}" -std=c++17 -I"$work/cpp-$name" "$work/cpp-$name/$name.cpp" \
        "$work/cpp-$name/run.cpp" -o "$work/cpp-$name/program"
    "$ziran" build --target=go --pkg main --exe --entry "$name:main" --root "$work" \
        -o "$work/go-$name" "$work/$name.zi"
    (cd "$work/go-$name" && GO111MODULE=off go build -o program .)
    "$ziran" build --target=rust --exe --entry "$name:main" --root "$work" \
        -o "$work/rust-$name" "$work/$name.zi"
    CARGO_TARGET_DIR=$work/rust-target-$name cargo build --quiet \
        --manifest-path "$work/rust-$name/Cargo.toml" 2> /dev/null
    cp "$work/rust-target-$name/debug/ziran_generated" "$work/rust-$name/program"
done

"$ziran" bundle --root "$work" --entry divide:main -o "$work/divide.zib" "$work/divide.zi"
"$ziran" run "$work/divide.zib" > "$work/vm.out"
cmp -s "$work/expected" "$work/vm.out" || {
    echo "the portable VM divides differently" >&2
    diff "$work/expected" "$work/vm.out" >&2 || true
    exit 1
}
for target in c cpp go rust; do
    "$work/$target-divide/program" > "$work/$target.out"
    if ! cmp -s "$work/expected" "$work/$target.out"; then
        echo "$target divides differently from the portable VM" >&2
        diff "$work/expected" "$work/$target.out" >&2 || true
        exit 1
    fi
    if "$work/$target-zero/program" > /dev/null 2>&1; then
        echo "$target division by zero kept running" >&2
        exit 1
    fi
done
