#!/bin/sh
set -eu

# The deepest nesting the language accepts must compile and run on an
# ordinary 8 MB stack with every tool: expressions 126 levels deep, blocks
# 100 levels deep, and a call chain 100 levels deep.
ziran=${1:?pass the ziran command}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
ulimit -s 8192

expression=1
level=0
while [ "$level" -lt 126 ]; do
    expression="($expression + 1)"
    level=$((level + 1))
done

{
    echo 'Depth :: (n: s32) -> s32 {'
    echo '    if n == 0 { return 0; }'
    echo '    return Depth(n - 1) + 1;'
    echo '}'
    echo
    echo 'main :: () {'
    echo "    sum: s32 = $expression;"
    echo '    print("%\n", sum);'
    level=0
    while [ "$level" -lt 100 ]; do
        echo "    if sum > $level {"
        level=$((level + 1))
    done
    echo '    print("%\n", Depth(100));'
    level=0
    while [ "$level" -lt 100 ]; do
        echo '    }'
        level=$((level + 1))
    done
    echo '}'
} > "$work/deep.zi"
printf '127\n100\n' > "$work/expected"

"$ziran" check --root "$work" "$work/deep.zi"
"$ziran" bundle --root "$work" --entry deep:main -o "$work/deep.zib" "$work/deep.zi"
"$ziran" run "$work/deep.zib" > "$work/vm.out"

"$ziran" build --target=c --root "$work" -o "$work/c" "$work/deep.zi"
printf '#include "deep.h"\nint main(void) { deep_main(); return 0; }\n' > "$work/c/run.c"
"${CC:-cc}" -std=c99 -I"$work/c" "$work/c/deep.c" "$work/c/run.c" -o "$work/c/program"
"$work/c/program" > "$work/c.out"

"$ziran" build --target=cpp --root "$work" -o "$work/cpp" "$work/deep.zi"
printf '#include "deep.hpp"\nint main() { deep_main(); return 0; }\n' > "$work/cpp/run.cpp"
"${CXX:-c++}" -std=c++17 -I"$work/cpp" "$work/cpp/deep.cpp" "$work/cpp/run.cpp" \
    -o "$work/cpp/program"
"$work/cpp/program" > "$work/cpp.out"

"$ziran" build --target=go --pkg main --exe --entry deep:main --root "$work" \
    -o "$work/go" "$work/deep.zi"
(cd "$work/go" && GO111MODULE=off go run .) > "$work/go.out"

"$ziran" build --target=rust --exe --entry deep:main --root "$work" \
    -o "$work/rust" "$work/deep.zi"
cargo build --quiet --manifest-path "$work/rust/Cargo.toml"
"$work/rust/target/debug/ziran_generated" > "$work/rust.out"

for target in vm c cpp go rust; do
    if ! cmp -s "$work/expected" "$work/$target.out"; then
        echo "$target output differs for deeply nested code" >&2
        diff "$work/expected" "$work/$target.out" >&2 || true
        exit 1
    fi
done
