#!/bin/sh
# Caller-owned record specializations keep their defining module's private
# helper graph, even through a named import. Same-named callback types keep
# distinct native identities. Both contracts survive saved IR on every target.
set -eu
ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/lib.zi" <<'ZI'
#load "helpers.zi";
#scope_export
Largest :: (values: []$T) -> T {
    return Select(values, cast(s64)0)
}
#scope_file
step: s64 = 1;
Increment :: (index: s64) -> s64 { return index + step }
Select :: (values: []$T, index: s64) -> T {
    if index == values.count - 1 { return values[index] }
    next: T = Select(values, Increment(index))
    if next < values[index] { return values[index] }
    return next
}
ZI
cat > "$work/helpers.zi" <<'ZI'
#scope_export
Smallest :: (values: []$T) -> T {
    return Select(values, cast(s64)0)
}
#scope_file
step: s64 = 1;
Increment :: (index: s64) -> s64 { return index + step }
Select :: (values: []$T, index: s64) -> T {
    if index == values.count - 1 { return values[index] }
    next: T = Select(values, Increment(index))
    if values[index] < next { return values[index] }
    return next
}
ZI
cat > "$work/first.zi" <<'ZI'
#scope_file
Callback :: #type (value: s32) -> s32;
Add :: (value: s32) -> s32 { return value + 1 }
#scope_export
Run :: () -> s32 {
    callback: Callback = Add
    return callback(9)
}
ZI
cat > "$work/second.zi" <<'ZI'
#scope_file
Callback :: #type (left: s64, right: s64) -> s64;
Add :: (left: s64, right: s64) -> s64 { return left + right }
#scope_export
Run :: () -> s32 {
    callback: Callback = Add
    return cast(s32)callback(10, 12)
}
ZI
cat > "$work/keywords.zi" <<'ZI'
#scope_file
class :: #type (value: s32) -> s32;
Identity :: (value: s32) -> s32 { return value }
#scope_export
Run :: () -> s32 {
    callback: class = Identity
    return callback(0)
}
ZI
cat > "$work/app.zi" <<'ZI'
#scope_file
UnusedLibrary :: #import "lib"
#load "body.zi";
ZI
cat > "$work/body.zi" <<'ZI'
#scope_file
Library :: #import "lib"
Sorting :: #import "std/sort"
First :: #import "first"
Second :: #import "second"
Keywords :: #import "keywords"
#scope_export
Callback :: struct { unused: s32; }
Item :: struct { key: s32; }
operator < :: (left: Item, right: Item) -> bool { return left.key < right.key }
Select :: (value: s32) -> s32 { return -100 }
main :: () {
    values: [3]Item = .[Item.{key = 9}, Item.{key = 1}, Item.{key = 5}]
    Sorting.Sort(values[:])
    if values[0].key != 1 || values[2].key != 9 { print("bad sort\n"); return }
    if Library.Largest(values[:]).key != 9 { print("bad scope\n"); return }
    if Library.Smallest(values[:]).key != 1 { print("bad loaded scope\n"); return }
    print("%\n", First.Run() + Second.Run() + Keywords.Run() + values[0].key + values[2].key)
}
ZI

"$ziran" check --root "$work" "$work/app.zi"
"$ziran" ir --root "$work" -o "$work/ir" "$work/app.zi"
printf '42\n' > "$work/expected"
for input in source saved; do
    if test "$input" = source; then
        root=$work
        source=$work/app.zi
    else
        root=$work/ir
        source=$work/ir/app.zir
    fi
    output=$work/$input
    "$ziran" bundle --root "$root" --entry app:main -o "$output.zib" "$source"
    "$ziran" run "$output.zib" > "$output.vm"
    cmp "$work/expected" "$output.vm"
    "$ziran" build --target=c --exe --entry app:main --root "$root" -o "$output/c" "$source"
    "$output/c/app" > "$output.c"
    cmp "$work/expected" "$output.c"
    "$ziran" build --target=cpp --root "$root" -o "$output/cpp" "$source"
    printf '#include "app.hpp"\nint main() { app_main(); }\n' > "$output/cpp/run.cpp"
    "${CXX:-c++}" -std=c++17 -I"$output/cpp" "$output"/cpp/*.cpp -o "$output/cpp/app"
    "$output/cpp/app" > "$output.cpp"
    cmp "$work/expected" "$output.cpp"
    "$ziran" build --target=go --exe --entry app:main --pkg main --root "$root" -o "$output/go" "$source"
    GO111MODULE=off go run "$output"/go/*.go > "$output.go"
    cmp "$work/expected" "$output.go"
    "$ziran" build --target=py --exe --entry app:main --root "$root" -o "$output/py" "$source"
    python3 "$output/py" > "$output.py"
    cmp "$work/expected" "$output.py"
    if command -v cargo >/dev/null 2>&1; then
        "$ziran" build --target=rust --exe --entry app:main --root "$root" -o "$output/rust" "$source"
        CARGO_TARGET_DIR="$work/rust-target" cargo build --quiet --manifest-path "$output/rust/Cargo.toml"
        "$work/rust-target/debug/ziran_generated" > "$output.rust"
        cmp "$work/expected" "$output.rust"
    fi
    "$ziran" api --json --root "$root" "$source" > "$output.api"
    if rg -q '__zi_dependency_' "$output.api"; then
        echo 'generated dependency leaked into public API' >&2
        exit 1
    fi
    "$ziran" api --root "$root" "$source" > "$output.api-text"
    if rg -q '__zi_dependency_' "$output.api-text"; then
        echo 'generated dependency leaked into text API' >&2
        exit 1
    fi
done
cmp "$work/source.zib" "$work/saved.zib"

cat > "$work/rejected.zi" <<'ZI'
Library :: #import "lib"
Answer :: () -> s64 { return Library.Increment(0) }
ZI
if "$ziran" check --root "$work" "$work/rejected.zi" 2> "$work/rejected.err"; then
    echo 'private source name became importable' >&2
    exit 1
fi
rg -q 'unresolved function' "$work/rejected.err"
echo 'named generic helper scope and callback identities: passed'
