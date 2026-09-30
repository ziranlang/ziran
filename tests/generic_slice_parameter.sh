#!/bin/sh
set -eu

# A polymorphic procedure can bind $T through a slice (`[]$T`) or a pointer
# (`*$T`) parameter. Every target, source and saved IR, prints the same lines.
ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/lib.zi" <<'ZI'
Sum :: (values: []$T) -> T {
    total: T = 0
    index: s64 = 0
    while index < cast(s64)values.count {
        total += values[index]
        index += 1
    }
    return total
}
Largest :: (values: []$T, fallback: T) -> T {
    if values.count == 0 { return fallback }
    best: T = values[0]
    index: s64 = 1
    while index < cast(s64)values.count {
        if best < values[index] { best = values[index] }
        index += 1
    }
    return best
}
Reverse :: (values: []$T) {
    low: s64 = 0
    high: s64 = cast(s64)values.count - 1
    while low < high {
        saved: T = values[low]
        values[low] = values[high]
        values[high] = saved
        low += 1
        high -= 1
    }
}
Count :: (values: []$T) -> s64 { return cast(s64)values.count }
ZI
cat > "$work/app.zi" <<'ZI'
#import "lib"
Record :: struct { key: s32; tag: s32; }
main :: () {
    numbers: [4]s32 = .[3, 9, 4, 6]
    print("% %\n", Sum(numbers[:]), Largest(numbers[:], 0))
    none: [0]s32
    print("%\n", Largest(none[:], 7))
    wide: [3]s64 = .[5000000000, 1, 2]
    print("%\n", Sum(wide[:]))
    Reverse(numbers[:])
    print("% % % %\n", numbers[0], numbers[1], numbers[2], numbers[3])
    records: [3]Record = .[Record.{key = 1, tag = 10},
        Record.{key = 2, tag = 20}, Record.{key = 3, tag = 30}]
    Reverse(records[:])
    print("% % %\n", records[0].tag, records[2].tag, Count(records[:]))
    print("%\n", Count(numbers[1:3]))
}
ZI
cat > "$work/expected" <<'EOF'
22 9
7
5000000003
6 4 9 3
30 10 3
2
EOF

"$ziran" check --root "$work" "$work/app.zi"
"$ziran" ir --root "$work" -o "$work/ir" "$work/app.zi"
"$ziran" bundle --root "$work" --entry app:main -o "$work/source.zib" "$work/app.zi"
"$ziran" bundle --root "$work/ir" --entry app:main -o "$work/saved.zib" "$work/ir/app.zir"
cmp "$work/source.zib" "$work/saved.zib"
"$ziran" run "$work/source.zib" > "$work/vm.out"
cmp "$work/expected" "$work/vm.out"

for input in "$work/app.zi" "$work/ir/app.zir"; do
    case "$input" in
        *.zi) root=$work; out=$work/source ;;
        *) root=$work/ir; out=$work/saved ;;
    esac
    "$ziran" build --target=c --root "$root" -o "$out/c" "$input"
    printf '#include "app.h"\nint main(void) { app_main(); return 0; }\n' > "$out/c/run.c"
    "${CC:-cc}" -std=c11 -I"$out/c" "$out"/c/*.c -o "$out/c/program"
    "$out/c/program" > "$out/c.out"

    "$ziran" build --target=cpp --root "$root" -o "$out/cpp" "$input"
    printf '#include "app.hpp"\nint main() { app_main(); return 0; }\n' > "$out/cpp/run.cpp"
    "${CXX:-c++}" -std=c++17 -I"$out/cpp" "$out"/cpp/*.cpp -o "$out/cpp/program"
    "$out/cpp/program" > "$out/cpp.out"

    "$ziran" build --target=go --pkg main --exe --entry app:main --root "$root" \
        -o "$out/go" "$input"
    (cd "$out/go" && GO111MODULE=off go run .) > "$out/go.out"

    targets="c cpp go"
    if command -v cargo >/dev/null 2>&1; then
        "$ziran" build --target=rust --exe --entry app:main --root "$root" \
            -o "$out/rust" "$input"
        CARGO_TARGET_DIR=$work/rust-target cargo build --quiet \
            --manifest-path "$out/rust/Cargo.toml"
        "$work/rust-target/debug/ziran_generated" > "$out/rust.out"
        targets="$targets rust"
    fi

    "$ziran" build --target=py --exe --entry app:main --root "$root" \
        -o "$out/py" "$input"
    python3 "$out/py" > "$out/py.out"
    targets="$targets py"

    for target in $targets; do
        if ! cmp -s "$work/expected" "$out/$target.out"; then
            echo "$target output differs for $input" >&2
            diff "$work/expected" "$out/$target.out" >&2 || true
            exit 1
        fi
    done
done

# A pointer parameter binds the pointee type. Pointers are outside the
# portable .zib subset, so the native targets cover it.
cat > "$work/swap.zi" <<'ZI'
Pair :: struct { left: s32; right: s32; }
Swap :: (left: *$T, right: *T) {
    saved: T = left.*
    left.* = right.*
    right.* = saved
}
#program_export
Answer :: () -> s32 {
    a: s32 = 40
    b: s32 = 2
    Swap(*a, *b)
    first: Pair = Pair.{left = 1, right = 2}
    second: Pair = Pair.{left = 3, right = 4}
    Swap(*first, *second)
    if a != 2 || b != 40 || first.left != 3 || second.right != 2 { return 0 }
    return a + b
}
ZI
"$ziran" build --target=c --root "$work" -o "$work/swap-c" "$work/swap.zi"
printf '#include "swap.h"\nint main(void) { return Answer() == 42 ? 0 : 1; }\n' \
    > "$work/swap-c/run.c"
"${CC:-cc}" -std=c11 -I"$work/swap-c" "$work"/swap-c/*.c -o "$work/swap-c/program"
"$work/swap-c/program"

reject() {
    name=$1
    message=$2
    if "$ziran" check --root "$work" "$work/$name.zi" > "$work/$name.out" 2>&1; then
        echo "$name was accepted" >&2
        exit 1
    fi
    if ! grep -q "$message" "$work/$name.out"; then
        echo "$name: expected '$message'" >&2
        cat "$work/$name.out" >&2
        exit 1
    fi
}

cat > "$work/not_slice.zi" <<'ZI'
#import "lib"
Bad :: () -> s64 { value: s32 = 1; return Count(value) }
ZI
reject not_slice "polymorphic slice parameter needs a slice"

cat > "$work/mixed.zi" <<'ZI'
#import "lib"
Bad :: () -> s32 {
    values: [2]s32 = .[1, 2]
    return Largest(values[:], 1.5)
}
ZI
reject mixed "argument type mismatch"

cat > "$work/not_pointer.zi" <<'ZI'
Clear :: (target: *$T) { }
Bad :: () { value: s32 = 1; Clear(value) }
ZI
reject not_pointer "polymorphic pointer parameter needs a pointer"

cat > "$work/two_binders.zi" <<'ZI'
Bad :: (left: []$T, right: $T) { }
ZI
reject two_binders "\$T binds its type more than once"
