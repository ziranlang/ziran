#!/bin/sh
# Fixed-array binders infer their element type while keeping their declared
# capacity and value semantics, including through separately saved libraries.
set -eu
ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY

cat > "$work/lib.zi" <<'ZI'
#scope_file
Width :: 3;
T :: [5]u8;
#scope_export
Copy :: (values: [Width]$T) -> [Width]T { return values; }
First :: (values: [3] $T) -> T { return values[0]; }
Change :: (values: [3]$T, replacement: T) -> T {
    values[0] = replacement;
    return values[0];
}
Empty :: (values: [0]$T) -> s64 { return values.count; }
Pair :: (values: [3]$E, label: $L) -> E, L { return values[0], label; }
Outer :: (values: [2]$T) -> T { return values[1]; }
Take :: (values: [3]$T, other: [2]T) -> T { return other[1]; }
ZI
cat > "$work/app.zi" <<'ZI'
#import "lib";
Library :: #import "lib";
Width :: 7;
Item :: struct { value: s32; }
Numbers :: [3]s32;
main :: () {
    values: Numbers;
    values[0] = 40; values[1] = 1; values[2] = 2;
    copied := Library.Copy(values);
    if copied[0] != 40 || copied[2] != 2 { print("copied array\n"); return; }
    if First(values) != 40 || Library.Change(values, 8) != 8 || values[0] != 40 {
        print("array value semantics\n"); return;
    }
    wide: [3]s64 = .[5000000000, 1, 2];
    if First(wide) != 5000000000 { print("wide element\n"); return; }
    items: [3]Item = .[.{value = 40}, .{value = 1}, .{value = 2}];
    records := Library.Copy(items);
    if records[2].value != 2 || Library.First(items).value != 40 {
        print("record element\n"); return;
    }
    item, label := Library.Pair(items, "array");
    if item.value != 40 || label != "array" { print("independent binders\n"); return; }
    none: [0]Item;
    if Library.Empty(none) != 0 { print("empty array\n"); return; }
    result := Library.First(values = copied);
    other: [2]s32 = .[1, 40];
    if Library.Take(values, other) != result { print("dependent array type\n"); return; }
    print("%\n", result + copied[2]);
}
ZI
cat > "$work/matrix.zi" <<'ZI'
Library :: #import "lib";
main :: () {
    matrix: [2][3]s32;
    matrix[1][0] = 40; matrix[1][1] = 1; matrix[1][2] = 2;
    row := Library.Outer(matrix);
    print("%\n", row[0] + row[2]);
}
ZI

"$ziran" check --root "$work" "$work/app.zi"
"$ziran" ir --root "$work" -o "$work/ir" "$work/app.zi"
"$ziran" ir --root "$work" -o "$work/matrix-ir" "$work/matrix.zi"
"$ziran" ir --root "$work" -o "$work/library-ir" "$work/lib.zi"
cp "$work/app.zi" "$work/library-ir/app.zi"
cp "$work/matrix.zi" "$work/library-ir/matrix.zi"
printf '42\n' > "$work/expected"
for fixture in app matrix; do
for form in source saved library; do
    case "$form" in
        source) root=$work; input=$work/$fixture.zi ;;
        saved)
            root=$work/ir
            if test "$fixture" = matrix; then root=$work/matrix-ir; fi
            input=$root/$fixture.zir ;;
        library) root=$work/library-ir; input=$root/$fixture.zi ;;
    esac
    output=$work/$fixture-$form
    if test "$fixture" = app; then
        "$ziran" bundle --root "$root" --entry app:main -o "$output.zib" "$input"
        "$ziran" run "$output.zib" > "$output.vm"
        cmp "$work/expected" "$output.vm"
    else
        # Fixed-array returns containing nested arrays remain a native-only
        # capability. The portable linker must reject them explicitly.
        if "$ziran" bundle --root "$root" --entry matrix:main -o "$output.zib" "$input" > "$output.err" 2>&1; then
            echo 'nested array result was accepted by the portable linker' >&2
            exit 1
        fi
        rg -q 'outside the portable subset' "$output.err"
    fi
    "$ziran" build --target=c --exe --entry "$fixture:main" --root "$root" -o "$output/c" "$input"
    "$output/c/$fixture" > "$output.c"
    cmp "$work/expected" "$output.c"
    "$ziran" build --target=cpp --root "$root" -o "$output/cpp" "$input"
    printf '#include "%s.hpp"\nint main() { %s_main(); }\n' "$fixture" "$fixture" > "$output/cpp/run.cpp"
    "${CXX:-c++}" -std=c++17 -I"$output/cpp" "$output"/cpp/*.cpp -o "$output/cpp/app"
    "$output/cpp/app" > "$output.cpp"
    cmp "$work/expected" "$output.cpp"
    "$ziran" build --target=go --exe --entry "$fixture:main" --pkg main --root "$root" -o "$output/go" "$input"
    GO111MODULE=off go run "$output"/go/*.go > "$output.go"
    cmp "$work/expected" "$output.go"
    "$ziran" build --target=py --exe --entry "$fixture:main" --root "$root" -o "$output/py" "$input"
    python3 "$output/py" > "$output.py"
    cmp "$work/expected" "$output.py"
    if command -v cargo >/dev/null 2>&1; then
        "$ziran" build --target=rust --exe --entry "$fixture:main" --root "$root" -o "$output/rust" "$input"
        CARGO_TARGET_DIR=$work/rust-target cargo build --quiet --manifest-path "$output/rust/Cargo.toml"
        "$work/rust-target/debug/ziran_generated" > "$output.rust"
        cmp "$work/expected" "$output.rust"
    fi
done
done
cmp "$work/app-source.zib" "$work/app-saved.zib"
cmp "$work/app-source.zib" "$work/app-library.zib"

reject() {
    name=$1
    message=$2
    if "$ziran" check --root "$work" "$work/$name.zi" > "$work/$name.out" 2>&1; then
        echo "$name was accepted" >&2
        exit 1
    fi
    if ! rg -q -- "$message" "$work/$name.out"; then
        cat "$work/$name.out" >&2
        exit 1
    fi
}
cat > "$work/not_array.zi" <<'ZI'
#import "lib";
Bad :: () { value: s32 = 1; First(value); }
ZI
reject not_array 'polymorphic array parameter needs a fixed array'
cat > "$work/slice.zi" <<'ZI'
#import "lib";
Bad :: () { values: [3]s32; First(values[:]); }
ZI
reject slice 'polymorphic array parameter needs a fixed array'
cat > "$work/capacity.zi" <<'ZI'
#import "lib";
Bad :: () { values: [2]s32; First(values); }
ZI
reject capacity 'polymorphic array capacity mismatch'
cat > "$work/type_mismatch.zi" <<'ZI'
#import "lib";
Bad :: () { values: [3]s32; Change(values, "wrong"); }
ZI
reject type_mismatch 'argument type mismatch'
cat > "$work/dependent_mismatch.zi" <<'ZI'
#import "lib";
Bad :: () { values: [3]s32; other: [2]string; Take(values, other); }
ZI
reject dependent_mismatch 'argument type mismatch'
cat > "$work/unknown_bound.zi" <<'ZI'
Bad :: (values: [Unknown]$T) { }
ZI
reject unknown_bound 'polymorphic array parameter needs a resolved capacity'
cat > "$work/negative_bound.zi" <<'ZI'
Bad :: (values: [-1]$T) { }
ZI
reject negative_bound 'polymorphic array parameter needs a resolved capacity'
cat > "$work/two_binders.zi" <<'ZI'
Bad :: (left: [3]$T, right: $T) { }
ZI
reject two_binders '\$T binds its type more than once'
