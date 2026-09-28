#!/bin/sh
set -eu

# Call results go straight into the call or print that uses them, yet every
# argument still runs left to right on every target, reading values before
# a later call changes them.
ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/order.zi" <<'EOF'
counter: s32 = 0;
Next :: () -> s32 { counter += 1; return counter; }
Pair :: (a: s32, b: s32) -> s32 { return a * 10 + b; }
Name :: (n: s32) -> string {
    if n % 2 == 0 {
        return "even";
    }
    return "odd";
}
main :: () {
    print("% % %\n", Next(), Next(), Next());
    print("%\n", Pair(Next(), Next()));
    print("% %\n", counter, Next());
    print("%\n", Pair(counter, Next()));
    print("% %\n", counter + 1, Next());
    print("% %\n", Name(Next()), Name(Next()));
    print("% %\n", Next(), 1.5);
}
EOF
cat > "$work/expected" <<'EOF'
1 2 3
45
5 6
67
8 8
odd even
11 1.5
EOF

"$ziran" bundle --root "$work" --entry order:main -o "$work/order.zib" "$work/order.zi"
"$ziran" run "$work/order.zib" > "$work/vm.out"

"$ziran" build --target=c --root "$work" -o "$work/c" "$work/order.zi"
printf '#include "order.h"\nint main(void) { order_main(); return 0; }\n' > "$work/c/run.c"
"${CC:-cc}" -std=c99 -I"$work/c" "$work/c/order.c" "$work/c/run.c" -o "$work/c/program"
"$work/c/program" > "$work/c.out"

"$ziran" build --target=cpp --root "$work" -o "$work/cpp" "$work/order.zi"
printf '#include "order.hpp"\nint main() { order_main(); return 0; }\n' > "$work/cpp/run.cpp"
"${CXX:-c++}" -std=c++17 -I"$work/cpp" "$work/cpp/order.cpp" "$work/cpp/run.cpp" \
    -o "$work/cpp/program"
"$work/cpp/program" > "$work/cpp.out"

"$ziran" build --target=go --pkg main --exe --entry order:main --root "$work" \
    -o "$work/go" "$work/order.zi"
(cd "$work/go" && GO111MODULE=off go run .) > "$work/go.out"

"$ziran" build --target=rust --exe --entry order:main --root "$work" \
    -o "$work/rust" "$work/order.zi"
cargo build --quiet --manifest-path "$work/rust/Cargo.toml"
"$work/rust/target/debug/ziran_generated" > "$work/rust.out"

for target in vm c cpp go rust; do
    if ! cmp -s "$work/expected" "$work/$target.out"; then
        echo "$target runs call arguments out of order" >&2
        diff "$work/expected" "$work/$target.out" >&2 || true
        exit 1
    fi
done
