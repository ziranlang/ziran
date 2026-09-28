#!/bin/sh
set -eu

ziran=${1:?pass the ziran command}
script_dir=$(CDPATH= cd "$(dirname "$0")" && pwd)
mkdir -p "$script_dir/../build"
work=$(mktemp -d "$script_dir/../build/rust-backend.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM

command -v cargo >/dev/null 2>&1 || {
    echo 'cargo is required to test the Rust backend' >&2
    exit 1
}

cat > "$work/scalars.zi" <<'ZI'
Answer :: () -> s32 {
    mode: Mode = Identify(Mode.On)
    point: Point = MakePoint(2, 3, mode)
    point.x += 1
    if point.x != 3 || point.y != 3 || point.mode != Mode.On { return 1 }
    values: [4]s32 = .[1, 2, 3, 4]
    values[1] += 10
    if Sum(values) != 20 { return 1 }
    if Add(Loop(), -10) != 0 { return 1 }
    text: string = "A\u00e9B"
    if text.count != 4 { return 2 }
    if text[0] != cast(u8)65 { return 3 }
    if text[1:3].count != 2 { return 4 }
    if text[1:3] != "\u00e9" { return 5 }
    if "\a" != "\u0007" { return 6 }
    if text != "A\u00e9B" { return 7 }
    bytes: [4]u8
    bytes[0] = cast(u8)97
    bytes[1] = cast(u8)98
    bytes[2] = cast(u8)99
    view := TextView(bytes[0:3])
    whole := TextView(bytes[:])
    if view != "abc" || view.count != 3 || whole.count != 4 ||
       whole[3] != cast(u8)0 { return 8 }
    if Limit + Offset != 11 { return 9 }
    return 0
}
Add :: (a: s32, b: s32) -> s32 { return a + b }
Identify :: (value: Mode) -> Mode { return value }
Limit :: 8;
Offset :: 3;
Loop :: () -> s32 {
    total: int = 0
    for i: 0..3 { total += i }
    if total != 6 { return 1 }
    while total < 10 { total += 1 }
    return cast(s32) total
}
Mode :: enum { Off; On :: 4; }
Point :: struct { x: s32; y: s32; mode: Mode }
MakePoint :: (x: s32, y: s32, mode: Mode) -> Point {
    return .{x = x, y = y, mode = mode}
}
Sum :: (values: [4]s32) -> s32 {
    total: s32 = 0
    for value: values { total += value }
    return total
}
ZI

"$ziran" check --root "$work" "$work/scalars.zi"
"$ziran" ir --root "$work" -o "$work/ir" "$work/scalars.zi"

for input in source saved; do
    if test "$input" = source; then
        file=$work/scalars.zi
        root=$work
    else
        file=$work/ir/scalars.zir
        root=$work/ir
    fi
    "$ziran" build --target=rust --entry scalars:Answer --root "$root" \
        --exe -o "$work/$input" "$file"
    test -s "$work/$input/Cargo.toml"
    test -s "$work/$input/src/main.rs"
    cargo build --quiet --manifest-path "$work/$input/Cargo.toml"
    "$work/$input/target/debug/ziran_generated"
done

cmp "$work/source/src/main.rs" "$work/saved/src/main.rs"
cmp "$work/source/Cargo.toml" "$work/saved/Cargo.toml"

cat > "$work/vectors.zi" <<'ZI'
#import "vec"
#import "option"
Holder :: struct { values: Vec(s32); }

Make :: () -> Vec(s32) {
    result: Vec(s32)
    VecPush(result, 12)
    return result
}

Consume :: (items: Vec(s32)) -> s32 {
    return cast(s32) items.count
}

Early :: () -> s32 {
    values: Vec(s32)
    VecPush(values, 13)
    return values[0]
}

#program_export
main :: () -> s32 {
    values: Vec(s32)
    if !VecPush(values, 7) { return 1 }
    if !VecPush(values, 9) { return 2 }
    if values.count != 2 || values[1] != 9 { return 3 }
    if !VecGet(values, 0).has_value { return 4 }
    if !VecPop(values).has_value { return 5 }
    VecFree(values)
    {
        nested: Vec(s32) = Make()
        if nested.count != 1 { return 6 }
    }
    moved: Vec(s32) = Make()
    if Consume(moved) != 1 { return 7 }
    if Early() != 13 { return 8 }
    holder: Holder
    if !VecPush(holder.values, 14) { return 9 }
    if holder.values.count != 1 || holder.values[0] != 14 { return 10 }
    VecFree(holder.values)
    original: Vec(s32)
    VecPush(original, 4)
    VecPush(original, 5)
    VecPush(original, 6)
    copy: Vec(s32)
    if !VecClone(copy, original) { VecFree(original); VecFree(copy); return 11 }
    {
        view: []s32 = VecSlice(original, 1, 3)
        if view.count != 2 || view[0] != 5 || view[1] != 6 {
            return 12
        }
    }
    if !VecPush(copy, 7) { return 13 }
    if original.count != 3 || copy.count != 4 || copy[3] != 7 {
        return 14
    }
    return 0
}
ZI
"$ziran" check --root "$work" --module-path std "$work/vectors.zi"
"$ziran" ir --root "$work" --module-path std -o "$work/vector-ir" \
    "$work/vectors.zi"
for input in source saved; do
    if test "$input" = source; then
        file=$work/vectors.zi
        root=$work
    else
        file=$work/vector-ir/vectors.zir
        root=$work/vector-ir
    fi
    "$ziran" build --target=rust --entry vectors:main --root "$root" \
        --module-path std --exe -o "$work/vector-$input" "$file"
    cargo build --quiet --manifest-path "$work/vector-$input/Cargo.toml"
    "$work/vector-$input/target/debug/ziran_generated"
done
cmp "$work/vector-source/src/main.rs" "$work/vector-saved/src/main.rs"

"$ziran" capabilities --target=rust --json > "$work/capabilities.json"
rg -q '"target":"rust".*"parallel_execution":"serial"' "$work/capabilities.json"
rg -q '"target_contract":"experimental"' "$work/capabilities.json"

cat > "$work/bounds.zi" <<'ZI'
#program_export
main :: () -> s32 {
    text: string = "A"
    return cast(s32)text[2:1].count
}
ZI
"$ziran" build --target=rust --entry bounds:main --root "$work" --exe     -o "$work/bounds" "$work/bounds.zi"
cargo build --quiet --manifest-path "$work/bounds/Cargo.toml"
set +e
"$work/bounds/target/debug/ziran_generated" > "$work/bounds.out" 2> "$work/bounds.err"
status=$?
set -e
test "$status" -ne 0
rg -q 'assertion failed: low >= 0 && low <= high' "$work/bounds.err"

cat > "$work/unsupported.zi" <<'ZI'
Value :: union { item: s32 }
Answer :: () -> s32 {
    value: Value
    value.item = 1
    return value.item
}
ZI
if "$ziran" build --target=rust --entry unsupported:Answer --root "$work" \
    -o "$work/unsupported-output" "$work/unsupported.zi" \
    > "$work/unsupported.out" 2> "$work/unsupported.err"; then
    echo 'the initial Rust target accepted an unsupported aggregate' >&2
    exit 1
fi
rg -q 'initial Rust target supports' "$work/unsupported.err"
